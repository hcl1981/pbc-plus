/* audio.c -- PWM auf GP15 ueber RC-Tiefpass auf den Mini-Lautsprecher.
 *
 * GP15 ist Kanal B seines Slices, die DMA-Zieladresse muss also die obere
 * Haelfte des CC-Registers treffen (CLAUDE.md Abschnitt 4).  Falsch getroffen
 * heisst: stummer Ausgang ohne Fehlermeldung.
 *
 * Der Ringpuffer wird von der Hauptschleife gefuellt, die DMA laeuft im
 * ENDLESS-Modus mit Ringadressierung -- kein Interrupt, keine Verkettung,
 * damit im Tonpfad nichts haengen kann.  Die Abtastrate ist mit 22050 Hz
 * hoeher als die 11025 Hz mancher Portierung, weil hier keine Synth-Emulation
 * je Abtastwert rechnet: der Mixer kostet ein paar Dutzend Zyklen.
 */
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "hardware/regs/dma.h"
#include "board.h"
#include "audio.h"

#define RATE      22050
#define NSAMP     2048                     /* 4096 B, muss 2er-Potenz sein  */
#define RING_BITS 12                       /* log2(NSAMP * sizeof(uint16_t)) */
#define AHEAD     900                      /* ~41 ms Vorlauf, deckt einen
                                            * ausgerutschten Frame ab       */

static uint16_t ring[NSAMP] __attribute__((aligned(4096)));
static int  dma_ch = -1, dma_timer = -1;
static uint  slice;
static bool  running;
static uint32_t wr;

/* ---- Synthesezustand ---------------------------------------------------- */
static uint32_t lfsr = 0x2545F491u;
static int   thrust_target, thrust_amp;
static int   crackle_amp;
static int   lp;
static uint32_t tone_phase, tone_step;
static int   tone_amp, tone_left;
static uint8_t melody[8], melody_n, melody_i;
static uint16_t melody_hz[8];
static int   melody_wait;

static inline int noise(void)
{
    lfsr ^= lfsr << 13; lfsr ^= lfsr >> 17; lfsr ^= lfsr << 5;
    return (int)((lfsr >> 8) & 0xff) - 128;
}

static void note(uint16_t hz, int ms, int amp)
{
    tone_step  = (uint32_t)(((uint64_t)hz << 32) / RATE);
    tone_phase = 0;
    tone_amp   = amp;
    tone_left  = ms * RATE / 1000;
}

static void play(const uint16_t *hz, const uint8_t *ms, int n)
{
    int i;
    if (n > 8) n = 8;
    for (i = 0; i < n; i++) { melody_hz[i] = hz[i]; melody[i] = ms[i]; }
    melody_n = (uint8_t)n;
    melody_i = 0;
    melody_wait = 0;
}

bool pbc_audio_ok(void) { return running; }

bool pbc_audio_init(void)
{
    uint32_t sys = clock_get_hz(clk_sys);
    uint16_t numer = 0, denom = 0;
    int n;

    memset(ring, 0, sizeof ring);
    for (n = 0; n < NSAMP; n++)
        ring[n] = 128;                      /* Ruhepegel Mitte              */
    wr = 0;

    /* PWM: 8 Bit Aufloesung, Traeger sys/256 (~586 kHz), weit ueber dem
     * Hoerbereich und damit vom RC-Glied sauber weggefiltert. */
    gpio_set_function(PBC_PIN_AUDIO, GPIO_FUNC_PWM);
    slice = pwm_gpio_to_slice_num(PBC_PIN_AUDIO);
    {
        pwm_config c = pwm_get_default_config();
        pwm_config_set_wrap(&c, 255);
        pwm_config_set_clkdiv_int(&c, 1);
        pwm_init(slice, &c, true);
    }
    pwm_set_chan_level(slice, PWM_CHAN_B, 128);

    /* DMA-Takt: DREQ-Rate = sys * numer / denom */
    for (n = 15; n >= 1; n--) {
        uint64_t d = ((uint64_t)sys * (uint64_t)n) / RATE;
        if (d <= 65535u && d > 0) { numer = (uint16_t)n; denom = (uint16_t)d; break; }
    }
    if (!numer)
        return false;

    dma_timer = dma_claim_unused_timer(false);
    dma_ch    = dma_claim_unused_channel(false);
    if (dma_timer < 0 || dma_ch < 0)
        return false;
    dma_timer_set_fraction((uint)dma_timer, numer, denom);

    {
        dma_channel_config c = dma_channel_get_default_config((uint)dma_ch);
        channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
        channel_config_set_read_increment(&c, true);
        channel_config_set_write_increment(&c, false);
        channel_config_set_ring(&c, false, RING_BITS);       /* Lesering     */
        channel_config_set_dreq(&c, dma_get_timer_dreq((uint)dma_timer));
        /* Kanal 15 des PWM-Slices ist B -> obere Haelfte des CC-Registers. */
        dma_channel_configure((uint)dma_ch, &c,
                              (volatile uint16_t *)&pwm_hw->slice[slice].cc + 1,
                              ring, NSAMP, false);
    }
    /* ENDLESS: der Zaehler laeuft nie ab, der Lesering wiederholt sich. */
    dma_hw->ch[dma_ch].al1_transfer_count_trig =
        (DMA_CH0_TRANS_COUNT_MODE_VALUE_ENDLESS << DMA_CH0_TRANS_COUNT_MODE_LSB) | NSAMP;

    running = true;
    return true;
}

void pbc_audio_silence(void)
{
    thrust_target = thrust_amp = crackle_amp = tone_amp = 0;
    melody_n = melody_i = 0;
    tone_left = 0;
}

/* ---- einen Abtastwert erzeugen ------------------------------------------ */
static inline uint16_t next_sample(void)
{
    int s = 0, nz;

    /* Schubzischen: gefiltertes Rauschen, Huellkurve laeuft weich nach */
    thrust_amp += (thrust_target - thrust_amp) >> 5;
    nz = noise();
    lp += (nz - lp) >> 2;
    s += (lp * thrust_amp) >> 8;

    /* Kornaufschlaege: kurzes Knistern, klingt je Abtastwert ab */
    if (crackle_amp > 0) {
        s += (nz * crackle_amp) >> 8;
        crackle_amp -= (crackle_amp >> 6) + 1;
    }

    /* Melodie / Einzelton: Rechteck mit linear fallender Huellkurve */
    if (tone_left > 0) {
        int a = tone_amp;
        tone_phase += tone_step;
        s += (tone_phase & 0x80000000u) ? a : -a;
        tone_left--;
        if (tone_left < RATE / 200)
            tone_amp -= tone_amp / 8 + 1;
    } else if (melody_i < melody_n) {
        if (melody_wait > 0) {
            melody_wait--;
        } else {
            note(melody_hz[melody_i], melody[melody_i], 34);
            melody_i++;
            melody_wait = RATE / 200;
        }
    }

    s += 128;
    if (s < 0) s = 0;
    if (s > 255) s = 255;
    return (uint16_t)s;
}

/* ---- einmal je Bild: Ereignisse umsetzen und Ring nachfuellen ------------ */
void pbc_audio_frame(const noiz_audio_t *a)
{
    uint32_t rd;
    int room;

    if (!running)
        return;

    /* Grundton: je mehr Geschosse im Bild, desto lauter das Rauschen.  Das
     * ersetzt die Ogg-Musik der Vorlage, die hier nicht zu haben ist, und
     * macht die Dichte des Kugelvorhangs hoerbar. */
    {
        int lvl = a->bullets / 6;
        if (lvl > 26) lvl = 26;
        thrust_target = lvl;
    }

    if (a->ev & EV_SHIP_DIE) {
        static const uint16_t hz[4] = { 330, 262, 196, 131 };
        static const uint8_t  ms[4] = { 90, 90, 110, 240 };
        play(hz, ms, 4);
        crackle_amp = 110;
    } else if (a->ev & EV_BOSS_DIE) {
        static const uint16_t hz[4] = { 523, 659, 880, 1319 };
        static const uint8_t  ms[4] = { 70, 70, 70, 160 };
        play(hz, ms, 4);
        crackle_amp = 100;
    } else if (a->ev & EV_EXTEND) {
        static const uint16_t hz[3] = { 880, 1175, 1568 };
        static const uint8_t  ms[3] = { 60, 60, 120 };
        play(hz, ms, 3);
    } else if (a->ev & EV_FOE_DIE) {
        note(196, 70, 30);
        if (crackle_amp < 80) crackle_amp = 80;
    } else if (a->ev & EV_BONUS) {
        note(1568, 40, 24);
    } else if (a->ev & EV_UI) {
        note(880, 45, 28);
    } else if (a->ev & EV_HIT) {
        if (crackle_amp < 45) crackle_amp = 45;
    }
    if (a->ev & EV_SHOT) {
        if (crackle_amp < 26) crackle_amp = 26;
    }

    /* Vorlauf auffuellen.  Der Lesezeiger der DMA sagt, wo gerade gespielt
     * wird; darueber hinaus wird nichts geschrieben, damit die Verzoegerung
     * klein bleibt. */
    rd = (uint32_t)(((uintptr_t)dma_hw->ch[dma_ch].read_addr - (uintptr_t)ring) / 2u) & (NSAMP - 1u);
    room = (int)((wr - rd) & (NSAMP - 1u));
    /* Hat die DMA den Schreibzeiger ueberholt (Bild deutlich zu lang
     * gebraucht), sieht `room` wie ein fast voller Puffer aus.  Dann neu
     * aufsetzen statt bis zur naechsten Runde stumm zu bleiben. */
    if (room > NSAMP - 256) {
        wr = rd;
        room = 0;
    }
    while (room < AHEAD) {
        ring[wr & (NSAMP - 1u)] = next_sample();
        wr++;
        room++;
    }

    /* Sicherheitsnetz: sollte der Kanal doch einmal stehen, neu anwerfen. */
    if (!dma_channel_is_busy((uint)dma_ch)) {
        dma_channel_set_read_addr((uint)dma_ch, ring, false);
        dma_hw->ch[dma_ch].al1_transfer_count_trig =
            (DMA_CH0_TRANS_COUNT_MODE_VALUE_ENDLESS << DMA_CH0_TRANS_COUNT_MODE_LSB) | NSAMP;
    }
}
