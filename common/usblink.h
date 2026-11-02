//
// usblink -- Punkt-zu-Punkt-Link zwischen zwei PicoBoys ueber das USB-C-Kabel.
//
// Das ist *kein* USB-Protokoll. Der RP2350 laesst die beiden PHY-Leitungen
// D+/D- per USBPHY_DIRECT-Override direkt als IO benutzen; wir fahren darauf
// einen getakteten Halbduplex-Link:
//
//     D-  (DM) = CLK   -- wird ausschliesslich vom Master getrieben
//     D+  (DP) = DATA  -- Halbduplex, Richtung ergibt sich aus der Phase
//
// Damit braucht es weder USB-Host-Modus noch VBUS noch Enumeration -- nur ein
// USB-C-Kabel zwischen den Geraeten (GND + D+/D- durchverbunden).
//
// Ablauf eines Austauschs (immer vom Master angestossen):
//   1. Master zieht CLK hoch  (Attention)
//   2. Slave antwortet mit DATA hoch (Ready)
//   3. Master CLK runter, Slave gibt DATA frei
//   4. Phase 1: Master -> Slave, USBLINK_M2S_FRAME Bytes, Slave sampelt auf
//      CLK-Flanke (der Slave braucht deshalb keine eigene Zeitbasis)
//   5. Umschaltpause
//   6. Phase 2: Slave -> Master, USBLINK_S2M_FRAME Bytes, Master taktet weiter
//
// Beide Frames haben feste Laenge (kein Laengen-Handshake noetig):
//   [0xA5][len][payload ...][crc16_lo][crc16_hi]
//
// GPLv2 (wie der Rest des Ports).
//

#ifndef _USBLINK_H
#define _USBLINK_H

#include <stdint.h>
#include <stdbool.h>

// Nutzdatenlaenge der beiden Richtungen (ohne Rahmen). Muss auf beiden
// Geraeten identisch sein -- ist es, weil beide dieselbe Firmware fahren.
#define USBLINK_M2S_PAYLOAD 60
#define USBLINK_S2M_PAYLOAD 40

#define USBLINK_FRAME_OVERHEAD 4   // sync + len + crc16
#define USBLINK_M2S_FRAME (USBLINK_M2S_PAYLOAD + USBLINK_FRAME_OVERHEAD)
#define USBLINK_S2M_FRAME (USBLINK_S2M_PAYLOAD + USBLINK_FRAME_OVERHEAD)

#ifdef __cplusplus
extern "C" {
#endif

// Einmalig: USB-Block aus dem Reset holen, PHY uebernehmen, Leitungen idle.
void usblink_init(void);

// PHY wieder freigeben (Leitungen los, Overrides aus).
void usblink_deinit(void);

// Taktrate des Masters setzen (halbe Bitzeit in ns). Nur der Master hat eine
// Zeitbasis -- der Slave laeuft flankengesteuert und muss nichts wissen.
void usblink_set_speed(uint32_t half_bit_ns);
uint32_t usblink_get_speed(void);

// True, sobald usblink_init() gelaufen ist.
bool usblink_is_active(void);

// --- Master ---------------------------------------------------------------
// Fuehrt einen kompletten Austausch durch (blockierend, IRQs waehrenddessen
// aus). tx/txlen gehen raus (auf USBLINK_M2S_PAYLOAD genullt aufgefuellt),
// die Antwort landet in rx.
// Rueckgabe: empfangene Nutzdatenlaenge, oder < 0:
//   -1 keine Antwort (kein Geraet / Slave nicht bereit)
//   -2 Rahmen-/CRC-Fehler
//   -3 Leitung belegt (anderes Geraet ist auch Master)
//   -4 Anfrage war schon erledigt (kein Fehler)
int usblink_master_exchange(const void *tx, int txlen, void *rx, int rxmax);

// Aufgeteilte Variante fuer das Spiel: Anfrage stellen (nicht blockierend),
// spaeter pollen, ob der Slave bereit ist, und dann den zeitkritischen Teil
// fahren. Dadurch haengt der Host nicht in einem Timeout fest, wenn gerade
// niemand antwortet -- und der Slave darf beliebig langsam pollen.
bool usblink_master_begin_attention(void);
bool usblink_master_slave_ready(void);
int  usblink_master_finish_exchange(const void *tx, int txlen, void *rx, int rxmax);
void usblink_master_abort(void);

// --- Slave ----------------------------------------------------------------
// True, wenn der Master gerade einen Austausch anfordert (CLK liegt hoch).
bool usblink_slave_request_pending(void);

// Gegenstueck zu usblink_master_exchange(); nur aufrufen, wenn
// usblink_slave_request_pending() true war. Rueckgabe wie oben.
int usblink_slave_exchange(void *rx, int rxmax, const void *tx, int txlen);

// Variante mit Rueckruf zwischen Empfangs- und Sendephase: liefert die Laenge
// der Antwort und darf dafuer das eben empfangene Paket auswerten. Damit passt
// die Quittung in denselben Austausch.
typedef int (*usblink_reply_fn)(int rxlen, void *user);
int usblink_slave_exchange_cb(void *rx, int rxmax, void *tx, int txlen,
                              usblink_reply_fn make_reply, void *user);

// --- Diagnose -------------------------------------------------------------
// Bit1 = CLK (D-) liegt hoch, Bit0 = DATA (D+) liegt hoch.
uint32_t usblink_line_state(void);

// Feindiagnose des letzten Fehlschlags:
//   Master:  2 = Slave gab DATA nicht frei, 3 = Slave meldete sich nicht zur
//            Antwortphase, 4 = falsches Sync-Byte, 5 = unsinnige Laenge,
//            6 = CRC der Antwort falsch
//   Slave:  12 = Master senkte den Takt nicht, 13 = Taktflanke ausgeblieben
//            (pos = Byte), 14 = falsches Sync, 15 = unsinnige Laenge,
//            16 = CRC falsch, 17 = Taktflanke beim Senden ausgeblieben
//   0 = letzter Austausch war in Ordnung
#define USBLINK_FAIL_CODES 24
extern uint8_t usblink_fail_code;
extern uint8_t usblink_fail_pos;
extern uint16_t usblink_fail_counts[USBLINK_FAIL_CODES];
extern uint8_t usblink_fail_pos_min;
extern uint8_t usblink_fail_pos_max;

extern uint32_t usblink_stat_ok;
extern uint32_t usblink_stat_timeout;
extern uint32_t usblink_stat_badframe;

#ifdef __cplusplus
}
#endif

#endif // _USBLINK_H
