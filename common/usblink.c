//
// usblink -- getakteter Halbduplex-Link ueber D+/D- des USB-C-Ports.
// Siehe usblink.h fuer die Protokollbeschreibung.
//
// GPLv2 (wie der Rest des Ports).
//

#include "usblink.h"

#if PICO_ON_DEVICE

#include "pico/stdlib.h"
#include "hardware/structs/usb.h"
#include "hardware/regs/usb.h"
#include "hardware/resets.h"
#include "hardware/sync.h"
#include "hardware/clocks.h"

// Alles, was Flanken zaehlt oder erzeugt, laeuft aus dem RAM.
#define USBLINK_TIME_CRITICAL __not_in_flash_func

uint32_t usblink_stat_ok;
uint32_t usblink_stat_timeout;
uint32_t usblink_stat_badframe;
uint8_t usblink_fail_code;   // wo genau es zuletzt geklemmt hat
uint8_t usblink_fail_pos;    // bei welchem Byte
uint16_t usblink_fail_counts[USBLINK_FAIL_CODES];  // wie oft welcher Fehler
uint8_t usblink_fail_pos_min = 255;
uint8_t usblink_fail_pos_max;

#define FAIL(code, pos) do { \
    usblink_fail_code = (code); \
    usblink_fail_pos = (uint8_t)(pos); \
    if ((code) < USBLINK_FAIL_CODES) usblink_fail_counts[code]++; \
    if ((code) == 13 || (code) == 17) { \
        if ((uint8_t)(pos) < usblink_fail_pos_min) usblink_fail_pos_min = (uint8_t)(pos); \
        if ((uint8_t)(pos) > usblink_fail_pos_max) usblink_fail_pos_max = (uint8_t)(pos); \
    } \
} while (0)

#define SYNC_BYTE 0xa5

// Groessenlimits (Rahmen inkl. sync/len/crc).
#define M2S_MAX_PAYLOAD 160
#define S2M_MAX_PAYLOAD 64

// --- Timing ---------------------------------------------------------------
// Nur der Master braucht eine Zeitbasis; der Slave laeuft rein flankengesteuert.
#ifndef USBLINK_HALF_BIT_NS
#define USBLINK_HALF_BIT_NS 1000
#endif
// Wartezeit des Slaves beim Richtungswechsel, bevor er DATA uebernimmt.
#ifndef USBLINK_TURNAROUND_NS
#define USBLINK_TURNAROUND_NS 5000
#endif

// Timeouts
#define ATTN_WAIT_US    4000   // Master wartet auf Ready des Slaves
#define READY_WAIT_US   2000   // Slave haelt sein Ready, bis der Master reagiert
#define HANDSHAKE_US    2000   // kurze Handshake-Schritte (inkl. Antwort-Rueckruf)
#define BIT_WAIT_US      300   // Slave wartet auf die naechste Taktflanke

static uint32_t half_bit_cycles;
static uint32_t half_bit_ns_current = USBLINK_HALF_BIT_NS;
static uint32_t turnaround_cycles;
static bool link_active;

// --- PHY-Zugriff ----------------------------------------------------------
// CLK = DM (D-), DATA = DP (D+)
#define CLK_TX_BIT   USB_USBPHY_DIRECT_TX_DM_BITS
#define CLK_OE_BIT   USB_USBPHY_DIRECT_TX_DM_OE_BITS
#define CLK_RX_BIT   USB_USBPHY_DIRECT_RX_DM_BITS
#define DATA_TX_BIT  USB_USBPHY_DIRECT_TX_DP_BITS
#define DATA_OE_BIT  USB_USBPHY_DIRECT_TX_DP_OE_BITS
#define DATA_RX_BIT  USB_USBPHY_DIRECT_RX_DP_BITS

// Ruhezustand: nichts getrieben, beide Leitungen per Pulldown auf low.
#define PHY_IDLE (USB_USBPHY_DIRECT_DP_PULLDN_EN_BITS | USB_USBPHY_DIRECT_DM_PULLDN_EN_BITS)

// Alles, was wir dem USB-Controller aus der Hand nehmen. TX_PD/RX_PD bleiben
// dabei 0 (Sender/Empfaenger aktiv), TX_DIFFMODE 0 (single-ended treiben),
// Pullups aus, Pulldowns an.
#define PHY_OVERRIDE_ALL ( \
    USB_USBPHY_DIRECT_OVERRIDE_TX_DP_OVERRIDE_EN_BITS | \
    USB_USBPHY_DIRECT_OVERRIDE_TX_DM_OVERRIDE_EN_BITS | \
    USB_USBPHY_DIRECT_OVERRIDE_TX_DP_OE_OVERRIDE_EN_BITS | \
    USB_USBPHY_DIRECT_OVERRIDE_TX_DM_OE_OVERRIDE_EN_BITS | \
    USB_USBPHY_DIRECT_OVERRIDE_TX_DIFFMODE_OVERRIDE_EN_BITS | \
    USB_USBPHY_DIRECT_OVERRIDE_TX_PD_OVERRIDE_EN_BITS | \
    USB_USBPHY_DIRECT_OVERRIDE_RX_PD_OVERRIDE_EN_BITS | \
    USB_USBPHY_DIRECT_OVERRIDE_DP_PULLDN_EN_OVERRIDE_EN_BITS | \
    USB_USBPHY_DIRECT_OVERRIDE_DM_PULLDN_EN_OVERRIDE_EN_BITS | \
    USB_USBPHY_DIRECT_OVERRIDE_DP_PULLUP_EN_OVERRIDE_EN_BITS | \
    USB_USBPHY_DIRECT_OVERRIDE_DM_PULLUP_OVERRIDE_EN_BITS)

static uint32_t phy_shadow;

static inline void phy_write(uint32_t v) {
    phy_shadow = v;
    usb_hw->phy_direct = v;
}

static inline uint32_t phy_read(void) {
    return usb_hw->phy_direct;
}

static inline void clk_set(int level) {
    phy_write(level ? (phy_shadow | CLK_TX_BIT) : (phy_shadow & ~CLK_TX_BIT));
}

static inline void clk_drive(void) { phy_write((phy_shadow | CLK_OE_BIT) & ~CLK_TX_BIT); }
static inline void clk_release(void) { phy_write(phy_shadow & ~(CLK_OE_BIT | CLK_TX_BIT)); }

static inline void data_drive(int level) {
    uint32_t v = phy_shadow | DATA_OE_BIT;
    if (level) v |= DATA_TX_BIT; else v &= ~DATA_TX_BIT;
    phy_write(v);
}
static inline void data_release(void) { phy_write(phy_shadow & ~(DATA_OE_BIT | DATA_TX_BIT)); }

static inline int clk_in(void) { return (phy_read() & CLK_RX_BIT) != 0; }
static inline int data_in(void) { return (phy_read() & DATA_RX_BIT) != 0; }

static inline void delay_half(void) { busy_wait_at_least_cycles(half_bit_cycles); }

// Warten bis eine Leitung den gewuenschten Pegel hat; false bei Timeout.
static bool USBLINK_TIME_CRITICAL(wait_clk)(int level, uint32_t timeout_us) {
    uint32_t t0 = time_us_32();
    while (clk_in() != level) {
        if (time_us_32() - t0 > timeout_us) return false;
    }
    return true;
}

static bool USBLINK_TIME_CRITICAL(wait_data)(int level, uint32_t timeout_us) {
    uint32_t t0 = time_us_32();
    while (data_in() != level) {
        if (time_us_32() - t0 > timeout_us) return false;
    }
    return true;
}

// --- CRC16 (CCITT, ohne Tabelle) ------------------------------------------
static uint16_t USBLINK_TIME_CRITICAL(crc16)(const uint8_t *p, int len) {
    uint16_t crc = 0xffff;
    for (int i = 0; i < len; i++) {
        crc ^= (uint16_t)p[i] << 8;
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

// --- Init -----------------------------------------------------------------
void usblink_init(void) {
    if (link_active) return;

    // USB-Block aus dem Reset holen (pico_stdlib laesst ihn sonst drin).
    unreset_block_mask_wait_blocking(RESETS_RESET_USBCTRL_BITS);

    // PHY an die Pins, Serial-Engine bleibt untaetig.
    usb_hw->muxing = USB_USB_MUXING_TO_PHY_BITS | USB_USB_MUXING_SOFTCON_BITS;
    // VBUS-Erkennung faelschen -- am anderen Ende haengt kein Host.
    usb_hw->pwr = USB_USB_PWR_VBUS_DETECT_BITS | USB_USB_PWR_VBUS_DETECT_OVERRIDE_EN_BITS;

    phy_write(PHY_IDLE);
    usb_hw->phy_direct_override = PHY_OVERRIDE_ALL;

    uint32_t hz = clock_get_hz(clk_sys);
    half_bit_cycles = (uint32_t)(((uint64_t)hz * half_bit_ns_current) / 1000000000u);
    if (!half_bit_cycles) half_bit_cycles = 1;
    turnaround_cycles = (uint32_t)(((uint64_t)hz * USBLINK_TURNAROUND_NS) / 1000000000u);
    if (!turnaround_cycles) turnaround_cycles = 1;

    link_active = true;
}

void usblink_set_speed(uint32_t half_bit_ns) {
    uint32_t hz = clock_get_hz(clk_sys);
    half_bit_cycles = (uint32_t)(((uint64_t)hz * half_bit_ns) / 1000000000u);
    if (!half_bit_cycles) half_bit_cycles = 1;
    half_bit_ns_current = half_bit_ns;
}

uint32_t usblink_get_speed(void) {
    return half_bit_ns_current;
}

void usblink_deinit(void) {
    if (!link_active) return;
    phy_write(PHY_IDLE);
    usb_hw->phy_direct_override = 0;
    link_active = false;
}

bool usblink_is_active(void) {
    return link_active;
}

uint32_t usblink_line_state(void) {
    if (!link_active) return 0;
    uint32_t v = phy_read();
    return ((v & CLK_RX_BIT) ? 2u : 0u) | ((v & DATA_RX_BIT) ? 1u : 0u);
}

// --- Master ---------------------------------------------------------------
static void USBLINK_TIME_CRITICAL(m_send_bit)(int bit) {
    data_drive(bit);
    delay_half();
    clk_set(1);
    delay_half();
    clk_set(0);
    delay_half();      // DATA bleibt stabil, bis der Slave sicher gesampelt hat
}

static void USBLINK_TIME_CRITICAL(m_send_byte)(uint8_t b) {
    for (int i = 7; i >= 0; i--) m_send_bit((b >> i) & 1);
}

// Ein Bit vom Slave holen: der Slave legt es an, sobald CLK low ist. Er
// braucht dafuer Zeit -- und die kann knapp werden, wenn Core1 gerade ein Bild
// schiebt und ihm den Bus wegnimmt. Drei halbe Bitzeiten Vorlauf statt zwei:
// gemessene Einzelbitfehler in genau dieser Richtung waren die Folge.
static int USBLINK_TIME_CRITICAL(m_recv_bit)(void) {
    delay_half();
    delay_half();
    delay_half();
    clk_set(1);
    delay_half();
    int bit = data_in();
    clk_set(0);
    return bit;
}

static uint8_t USBLINK_TIME_CRITICAL(m_recv_byte)(void) {
    uint8_t b = 0;
    for (int i = 0; i < 8; i++) b = (uint8_t)((b << 1) | m_recv_bit());
    return b;
}

// Anfrage stellen und stehen lassen: CLK bleibt hoch, bis der Slave mit DATA
// antwortet oder usblink_master_abort() aufgeraeumt hat. So darf der Slave
// beliebig traege pollen, ohne dass der Master dabei blockiert.
bool usblink_master_begin_attention(void) {
    if (!link_active) return false;
    uint32_t save = save_and_disable_interrupts();
    if (clk_in()) {          // Gegenstelle faehrt auch Master
        restore_interrupts(save);
        FAIL(1, 0);
        return false;
    }
    // Liegt DATA schon hoch, BEVOR wir angeklopft haben, dann haengt die
    // Gegenstelle noch in einem alten Austausch (sie haelt ihr Ready bis zu
    // 30 ms lang). Klopften wir jetzt an, saehen wir dieses alte Ready sofort
    // als Antwort auf unsere neue Anfrage -- und wuerden in einen Austausch
    // takten, auf den der Client gar nicht vorbereitet ist. Genau das ergibt
    // "Client meldet Erfolg, Host liest Muell". Also erst abwarten.
    if (data_in()) {
        restore_interrupts(save);
        FAIL(7, 0);
        return false;
    }
    clk_drive();
    clk_set(1);
    restore_interrupts(save);
    return true;
}

bool usblink_master_slave_ready(void) {
    return link_active && data_in();
}

void usblink_master_abort(void) {
    if (!link_active) return;
    uint32_t save = save_and_disable_interrupts();
    clk_set(0);
    clk_release();
    data_release();
    restore_interrupts(save);
}

// Der zeitkritische Teil: laeuft erst, wenn der Slave bereits Ready gemeldet
// hat. CLK muss dabei schon von uns getrieben und hoch sein.
int USBLINK_TIME_CRITICAL(usblink_master_finish_exchange)(const void *tx, int txlen, void *rx, int rxmax) {
    if (!link_active || txlen < 0 || txlen > M2S_MAX_PAYLOAD) return -2;

    const uint8_t *txb = (const uint8_t *)tx;
    uint8_t *rxb = (uint8_t *)rx;
    int rc;

    uint32_t save = save_and_disable_interrupts();

    // 2. Takt runter, Slave gibt DATA frei
    clk_set(0);
    if (!wait_data(0, HANDSHAKE_US)) { FAIL(2, 0); rc = -1; goto fail; }

    // 3. Phase 1: Master -> Slave
    {
        static uint8_t frame[2 + M2S_MAX_PAYLOAD];
        frame[0] = SYNC_BYTE;
        frame[1] = (uint8_t)txlen;
        for (int i = 0; i < txlen; i++) frame[2 + i] = txb[i];
        uint16_t crc = crc16(frame, 2 + txlen);

        for (int i = 0; i < 2 + txlen; i++) m_send_byte(frame[i]);
        m_send_byte((uint8_t)(crc & 0xff));
        m_send_byte((uint8_t)(crc >> 8));
    }

    // 4. Richtungswechsel.
    //
    //    HIER LAG DER FEHLER: Wir haben DATA freigegeben und sofort auf das
    //    erste Bit des Slaves gewartet (das MSB von 0xa5, also immer 1). War
    //    das letzte Bit der Phase 1 aber selbst eine 1 -- und das haengt vom
    //    High-Byte der Pruefsumme ab, also rund jedes zweite Mal --, dann lag
    //    die Leitung noch hoch, weil der Pulldown sie erst entladen muss. Das
    //    haben wir als "Slave ist bereit" gelesen und losgetaktet, waehrend
    //    der Slave noch seine Pruefsumme rechnete. Die ersten Takte gingen ins
    //    Leere, danach war alles um Bits verschoben: der Host liest ein
    //    falsches Startbyte (F4), der Slave wartet auf Flanken, die nach dem
    //    Abbruch nicht mehr kommen (F17).
    //
    //    Richtig ist: erst abwarten, bis die Leitung wirklich unten ist (dann
    //    ist sie sicher freigegeben und der Slave treibt noch nicht), und erst
    //    danach auf sein erstes Bit warten.
    data_release();
    if (!wait_data(0, HANDSHAKE_US)) { FAIL(8, 0); rc = -1; goto fail; }
    if (!wait_data(1, HANDSHAKE_US)) { FAIL(3, 0); rc = -1; goto fail; }

    // 5. Phase 2: Slave -> Master
    {
        uint8_t sync = m_recv_byte();
        if (sync != SYNC_BYTE) { FAIL(4, sync); rc = -2; goto fail_frame; }
        int len = m_recv_byte();
        if (len > S2M_MAX_PAYLOAD) { FAIL(5, len); rc = -2; goto fail_frame; }
        static uint8_t buf[2 + S2M_MAX_PAYLOAD];
        buf[0] = sync;
        buf[1] = (uint8_t)len;
        for (int i = 0; i < len; i++) buf[2 + i] = m_recv_byte();
        uint16_t crc = m_recv_byte();
        crc |= (uint16_t)m_recv_byte() << 8;
        if (crc != crc16(buf, 2 + len)) { FAIL(6, len); rc = -2; goto fail_frame; }

        int n = len < rxmax ? len : rxmax;
        for (int i = 0; i < n; i++) rxb[i] = buf[2 + i];

        clk_set(0);
        clk_release();
        restore_interrupts(save);
        usblink_stat_ok++;
        usblink_fail_code = 0;
        return n;   // was wirklich im Puffer steht
    }

fail_frame:
    usblink_stat_badframe++;
    clk_set(0);
    clk_release();
    data_release();
    restore_interrupts(save);
    return rc;

fail:
    usblink_stat_timeout++;
    clk_set(0);
    clk_release();
    data_release();
    restore_interrupts(save);
    return rc;
}

// Blockierende Komplettvariante (Testprogramm): Anfrage stellen, auf Ready
// warten, austauschen.
int usblink_master_exchange(const void *tx, int txlen, void *rx, int rxmax) {
    if (!usblink_master_begin_attention()) return -3;
    uint32_t t0 = time_us_32();
    while (!usblink_master_slave_ready()) {
        if (time_us_32() - t0 > ATTN_WAIT_US) {
            usblink_master_abort();
            usblink_stat_timeout++;
            return -1;
        }
    }
    return usblink_master_finish_exchange(tx, txlen, rx, rxmax);
}

// --- Slave ----------------------------------------------------------------
bool USBLINK_TIME_CRITICAL(usblink_slave_request_pending)(void) {
    return link_active && clk_in();
}

// Warten auf einen CLK-Pegel -- so knapp wie moeglich, denn diese Schleife
// bestimmt, wie kurz eine Taktphase sein darf. Deshalb:
//   * genau EIN Registerlesen pro Durchlauf (CLK und DATA stecken im selben
//     Register, die Momentaufnahme wird zurueckgegeben)
//   * die Uhr nur alle 256 Durchlaeufe befragen statt jedes Mal
// Vorher kostete jeder Durchlauf zwei Peripherie-Zugriffe; mit Core1, das
// gleichzeitig rendert und per DMA aufs Display schiebt, hat das gereicht,
// um eine 600-ns-Phase zu verpassen.
static bool USBLINK_TIME_CRITICAL(s_wait_clk)(int want, uint32_t *sample) {
    uint32_t guard = 0;
    uint32_t t0 = time_us_32();
    for (;;) {
        uint32_t v = usb_hw->phy_direct;
        if (((v & CLK_RX_BIT) != 0) == (want != 0)) {
            *sample = v;
            return true;
        }
        if (!(++guard & 0xff) && time_us_32() - t0 > BIT_WAIT_US) return false;
    }
}

// Ein Bit vom Master lesen: DATA aus derselben Momentaufnahme wie das
// CLK-high, damit ein spaet erkanntes High nicht schon das naechste Bit sieht.
static int USBLINK_TIME_CRITICAL(s_recv_bit)(bool *ok) {
    uint32_t v;
    if (!s_wait_clk(1, &v)) { *ok = false; return 0; }
    int bit = (v & DATA_RX_BIT) != 0;
    if (!s_wait_clk(0, &v)) { *ok = false; return 0; }
    return bit;
}

static uint8_t USBLINK_TIME_CRITICAL(s_recv_byte)(bool *ok) {
    uint8_t b = 0;
    for (int i = 0; i < 8; i++) {
        b = (uint8_t)((b << 1) | s_recv_bit(ok));
        if (!*ok) break;   // nicht acht mal in den Timeout laufen
    }
    return b;
}

// Ein Bit an den Master: anlegen solange CLK low ist, halten bis CLK wieder low.
static bool USBLINK_TIME_CRITICAL(s_send_bit)(int bit) {
    uint32_t v;
    data_drive(bit);
    if (!s_wait_clk(1, &v)) return false;
    if (!s_wait_clk(0, &v)) return false;
    return true;
}

static bool USBLINK_TIME_CRITICAL(s_send_byte)(uint8_t b) {
    for (int i = 7; i >= 0; i--) {
        if (!s_send_bit((b >> i) & 1)) return false;
    }
    return true;
}

int USBLINK_TIME_CRITICAL(usblink_slave_exchange)(void *rx, int rxmax, const void *tx, int txlen) {
    return usblink_slave_exchange_cb(rx, rxmax, (void *)tx, txlen, NULL, NULL);
}

// Wie oben, aber mit Rueckruf zwischen Empfangs- und Sendephase: der Slave hat
// das Paket des Hosts dann schon in der Hand und kann seine Antwort darauf
// aufbauen (Quittung im selben Austausch statt erst im naechsten). Der Rueckruf
// laeuft mit gesperrten Interrupts und muss kurz sein -- der Master wartet
// waehrenddessen auf das erste Antwortbit.
int USBLINK_TIME_CRITICAL(usblink_slave_exchange_cb)(void *rx, int rxmax, void *tx, int txlen,
                                                     usblink_reply_fn make_reply, void *user) {
    if (!link_active || txlen < 0 || txlen > S2M_MAX_PAYLOAD) return -2;

    uint8_t *rxb = (uint8_t *)rx;
    const uint8_t *txb = (const uint8_t *)tx;
    int rc = -1;
    int received = -1;

    uint32_t save = save_and_disable_interrupts();

    // Zwischen dem Pending-Test des Aufrufers und hier kann die Anfrage schon
    // erledigt sein (z.B. weil der Timer-IRQ sie abgearbeitet hat).
    if (!clk_in()) {
        restore_interrupts(save);
        return -4;              // nichts zu tun (anderer Pfad war schneller)
    }

    // 1. Ready melden und halten, bis der Master den Takt senkt. Das darf
    //    dauern, wenn der Master gerade nur im Framerhythmus nachschaut.
    data_drive(1);
    if (!wait_clk(0, READY_WAIT_US)) {
        FAIL(12, 0);
        data_release(); restore_interrupts(save); usblink_stat_timeout++; return -1;
    }
    // 2. DATA freigeben -- ab jetzt treibt der Master
    data_release();

    // 3. Phase 1: Master -> Slave
    {
        bool ok = true;
        static uint8_t buf[2 + M2S_MAX_PAYLOAD];
        buf[0] = s_recv_byte(&ok);
        if (!ok) { FAIL(13, 0); goto fail; }
        if (buf[0] != SYNC_BYTE) { FAIL(14, buf[0]); rc = -2; goto fail_frame_drain; }
        buf[1] = s_recv_byte(&ok);
        if (!ok) { FAIL(13, 1); goto fail; }
        int len = buf[1];
        if (len > M2S_MAX_PAYLOAD) { FAIL(15, len); rc = -2; goto fail_frame_drain; }
        for (int i = 0; i < len; i++) {
            buf[2 + i] = s_recv_byte(&ok);
            if (!ok) { FAIL(13, 2 + i); goto fail; }
        }
        uint16_t crc = s_recv_byte(&ok);
        crc |= (uint16_t)s_recv_byte(&ok) << 8;
        if (!ok) { FAIL(13, 2 + len); goto fail; }
        if (crc != crc16(buf, 2 + len)) { FAIL(16, len); rc = -2; goto fail_frame; }

        received = len < rxmax ? len : rxmax;
        for (int i = 0; i < received; i++) rxb[i] = buf[2 + i];
    }

    // Antwort jetzt bauen -- mit dem eben empfangenen Paket als Grundlage.
    if (make_reply) {
        int n = make_reply(received, user);
        if (n < 0 || n > S2M_MAX_PAYLOAD) { FAIL(18, 0); rc = -2; goto fail_frame; }
        txlen = n;
    }

    // 4. Richtungswechsel
    busy_wait_at_least_cycles(turnaround_cycles);

    // 5. Phase 2: Slave -> Master
    {
        static uint8_t frame[2 + S2M_MAX_PAYLOAD];
        frame[0] = SYNC_BYTE;
        frame[1] = (uint8_t)txlen;
        for (int i = 0; i < txlen; i++) frame[2 + i] = txb[i];
        uint16_t crc = crc16(frame, 2 + txlen);

        for (int i = 0; i < 2 + txlen; i++) {
            if (!s_send_byte(frame[i])) { FAIL(17, i); goto fail; }
        }
        if (!s_send_byte((uint8_t)(crc & 0xff))) { FAIL(17, 2 + txlen); goto fail; }
        if (!s_send_byte((uint8_t)(crc >> 8))) { FAIL(17, 3 + txlen); goto fail; }
    }

    data_release();
    restore_interrupts(save);
    usblink_stat_ok++;
    usblink_fail_code = 0;
    return received;

fail_frame_drain:
fail_frame:
    usblink_stat_badframe++;
    data_release();
    restore_interrupts(save);
    return rc;

fail:
    usblink_stat_timeout++;
    data_release();
    restore_interrupts(save);
    return rc;
}

#else // !PICO_ON_DEVICE

uint32_t usblink_stat_ok;
uint32_t usblink_stat_timeout;
uint32_t usblink_stat_badframe;
uint8_t usblink_fail_code;
uint8_t usblink_fail_pos;
uint16_t usblink_fail_counts[USBLINK_FAIL_CODES];
uint8_t usblink_fail_pos_min;
uint8_t usblink_fail_pos_max;

void usblink_init(void) {}
void usblink_deinit(void) {}
void usblink_set_speed(uint32_t half_bit_ns) { (void)half_bit_ns; }
uint32_t usblink_get_speed(void) { return 0; }
uint32_t usblink_line_state(void) { return 0; }
bool usblink_is_active(void) { return false; }
int usblink_master_exchange(const void *tx, int txlen, void *rx, int rxmax) {
    (void)tx; (void)txlen; (void)rx; (void)rxmax;
    return -1;
}
bool usblink_master_begin_attention(void) { return false; }
bool usblink_master_slave_ready(void) { return false; }
void usblink_master_abort(void) {}
int usblink_master_finish_exchange(const void *tx, int txlen, void *rx, int rxmax) {
    (void)tx; (void)txlen; (void)rx; (void)rxmax;
    return -1;
}
bool usblink_slave_request_pending(void) { return false; }
int usblink_slave_exchange(void *rx, int rxmax, const void *tx, int txlen) {
    (void)rx; (void)rxmax; (void)tx; (void)txlen;
    return -1;
}
int usblink_slave_exchange_cb(void *rx, int rxmax, void *tx, int txlen,
                              usblink_reply_fn make_reply, void *user) {
    (void)rx; (void)rxmax; (void)tx; (void)txlen; (void)make_reply; (void)user;
    return -1;
}

#endif
