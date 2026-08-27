/* led.c — drives the internal SK6805-EC14 RGB LED on GP11 and animates it
 * from the city's condition:
 *
 *   Crime  (CrimeAverage   > 100)  -> US police burst: 3 quick red flashes,
 *                                     pause, 3 quick blue flashes, pause.
 *   Traffic(TrafficAverage >  60)  -> amber "honk-honk" echo: two short
 *                                     blips, long pause.
 *
 * Crime has priority (its siren is the more urgent event in the original).
 * Both conditions are latched a few seconds so the LED doesn't chatter when
 * the average dithers around the threshold.
 *
 * The SK6805 is WS2812-compatible: one data wire, 24 bits per pixel MSB-first
 * in GRB order at ~800 kHz. The exact bit timing is handled by a PIO state
 * machine (ws2812.pio); we just push a colour word when it changes. Device
 * only — the host build never compiles this file.
 */
#include "led.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "ws2812.pio.h"
#include "engine.h"

#define LED_PIN     11
#define LED_PIO     pio0
#define LED_FREQ    800000.f

/* brightness (0..255) — kept low; the SK6805 is bright. Blue reads dimmer, so
 * it's lifted a touch. Traffic is a warm ORANGE (red >> green), not yellow. */
#define LVL_R       36    /* crime red   */
#define LVL_B       48    /* crime blue  */
#define AMBER_R     16    /* traffic orange, red part   */
#define AMBER_G     4     /* traffic orange, small green -> orange, not yellow */

#define CRIME_MS    5000  /* crime blinks for 5 s per advisory event   */
#define TRAFFIC_MS  5000  /* traffic: 5 s per burst                    */
#define TRAFFIC_GAP 2000  /* short pause between repeated traffic bursts */

static uint     s_sm;
static uint32_t s_last = 0xFFFFFFFFu;   /* force the first write */

void led_set(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t grb = ((uint32_t)g << 16) | ((uint32_t)r << 8) | (uint32_t)b;
    if (grb == s_last) return;          /* don't resend an unchanged colour */
    s_last = grb;
    pio_sm_put_blocking(LED_PIO, s_sm, grb << 8u);   /* 24 bits, left-aligned */
}

void led_init(void) {
    uint off = pio_add_program(LED_PIO, &ws2812_program);
    s_sm = pio_claim_unused_sm(LED_PIO, true);
    ws2812_program_init(LED_PIO, s_sm, off, LED_PIN, LED_FREQ, false);
    led_set(0, 0, 0);
}

/* --- animation ---------------------------------------------------------- */

enum { C_OFF, C_RED, C_BLUE, C_AMBER };

static void emit(int c) {
    switch (c) {
        case C_RED:   led_set(LVL_R, 0, 0);         break;
        case C_BLUE:  led_set(0, 0, LVL_B);         break;
        case C_AMBER: led_set(AMBER_R, AMBER_G, 0); break;
        default:      led_set(0, 0, 0);             break;
    }
}

typedef struct { uint16_t ms; uint8_t col; } step_t;

/* 3 red flashes, pause, 3 blue flashes, pause (~800 ms loop) */
static const step_t crime_seq[] = {
    {50, C_RED}, {50, C_OFF}, {50, C_RED}, {50, C_OFF}, {50, C_RED}, {150, C_OFF},
    {50, C_BLUE},{50, C_OFF}, {50, C_BLUE},{50, C_OFF}, {50, C_BLUE},{150, C_OFF},
};
/* honk-honk: blip, blip, long pause (~970 ms loop) */
static const step_t traffic_seq[] = {
    {90, C_AMBER}, {90, C_OFF}, {90, C_AMBER}, {700, C_OFF},
};
/* Both at once: crime flashes followed by traffic honks in a single loop, so
 * when crime and heavy traffic coincide BOTH stay visible every cycle instead
 * of one starving or interrupting the other (~1.4 s loop). */
static const step_t both_seq[] = {
    {50, C_RED}, {50, C_OFF}, {50, C_RED}, {50, C_OFF}, {50, C_RED}, {150, C_OFF},
    {50, C_BLUE},{50, C_OFF}, {50, C_BLUE},{50, C_OFF}, {50, C_BLUE},{150, C_OFF},
    {90, C_AMBER}, {90, C_OFF}, {90, C_AMBER}, {250, C_OFF},
};

void led_task(void) {
    static uint32_t crime_until = 0, traffic_until = 0;
    static int last_crime_evt = 0, crime_primed = 0;
    static uint32_t traffic_rearm = 0;
    static const step_t *seq = 0;
    static int seq_len = 0, step_i = 0, mode = -1;
    static uint32_t t0 = 0;

    uint32_t now = to_ms_since_boot(get_absolute_time());

    /* Crime: blink for 5 s each time the advisory actually FIRES (as in the
     * original, only now and then), not continuously while crime is high. */
    int ce = engine_crime_events();
    if (!crime_primed) { last_crime_evt = ce; crime_primed = 1; }  /* ignore boot state */
    if (ce != last_crime_evt) { last_crime_evt = ce; crime_until = now + CRIME_MS; }

    /* Traffic: repeating 5 s amber burst (with a short pause) while heavy
     * traffic persists. Driven by the average, which is reliable -- the engine's
     * traffic advisory (-12) de-duplicates against the last message shown, so in
     * a high-crime city it stops re-firing and the LED would stay dark. The
     * burst/pause keeps each blink to ~5 s (not a continuous latch) yet still
     * shows up in the gaps between crime bursts. */
    if (engine_traffic_average() > 60) {
        if (now >= traffic_rearm) {
            traffic_until = now + TRAFFIC_MS;                 /* 5 s of blinking */
            traffic_rearm = now + TRAFFIC_MS + TRAFFIC_GAP;   /* then a short gap */
        }
    } else {
        traffic_rearm = now;   /* not busy -> re-arm immediately when it returns */
    }

    int crime   = (int)(now < crime_until);
    int traffic = (int)(now < traffic_until);

    /* When both coincide, play the combined sequence so neither starves nor
     * chops the other off; otherwise show whichever single one is active. */
    int want;
    if (crime && traffic) want = 3;
    else if (crime)       want = 1;
    else if (traffic)     want = 2;
    else                  want = 0;

    if (want != mode) {                          /* switch pattern -> restart */
        mode = want;
        step_i = 0;
        t0 = now;
        if (mode == 1)      { seq = crime_seq;   seq_len = (int)(sizeof crime_seq   / sizeof crime_seq[0]); }
        else if (mode == 2) { seq = traffic_seq; seq_len = (int)(sizeof traffic_seq / sizeof traffic_seq[0]); }
        else if (mode == 3) { seq = both_seq;    seq_len = (int)(sizeof both_seq    / sizeof both_seq[0]); }
        else                { seq = 0;           seq_len = 0; emit(C_OFF); return; }
        emit(seq[0].col);
        return;
    }
    if (!seq) return;

    /* advance through the timeline (catch up if a frame was late) */
    while ((now - t0) >= seq[step_i].ms) {
        t0 += seq[step_i].ms;
        step_i = (step_i + 1) % seq_len;
        emit(seq[step_i].col);
    }
}
