/* sound.c -- see sound.h */
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "prefs.h"
#include "sound.h"

#define PIN      15
#define WRAP     1023u
#define MID      512
#define TICK_US  45                     /* 22222 samples a second */
#define RATE     (1000000 / TICK_US)
#define RAMP     (RATE / 100)           /* 10 ms to the middle and back */
#define BELL_N   (RATE * 13 / 10)       /* 1.3 s of ringing glass */
#define BUF_N    (BELL_N + 2 * RAMP + 1)

static uint16_t buf[BUF_N];
static uint slice;
static int ch = -1, timer = -1;
static bool up;                         /* the PWM is set up (and at 0 between sounds) */

void sound_on(void)
{
    if (up || !g_prefs.sound)
        return;
    if (ch < 0) {
        slice = pwm_gpio_to_slice_num(PIN);
        ch = dma_claim_unused_channel(true);
        timer = dma_claim_unused_timer(true);
    }
    pwm_config c = pwm_get_default_config();
    pwm_config_set_wrap(&c, WRAP);
    pwm_init(slice, &c, true);
    pwm_set_chan_level(slice, PWM_CHAN_B, 0);
    gpio_set_function(PIN, GPIO_FUNC_PWM);
    up = true;
}

void sound_off(void)
{
    if (!up)
        return;
    dma_channel_abort((uint)ch);
    gpio_init(PIN);                     /* no current through the speaker */
    gpio_set_dir(PIN, GPIO_OUT);
    gpio_put(PIN, 0);
    up = false;
}

void sound_update(void)
{
    if (g_prefs.sound)
        sound_on();
    else
        sound_off();
}

static void play(int n)
{
    dma_timer_set_fraction((uint)timer, 1, (uint16_t)(clock_get_hz(clk_sys) / 1000000u * TICK_US));
    dma_channel_config c = dma_channel_get_default_config((uint)ch);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);    /* lands in A and B */
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, dma_get_timer_dreq((uint)timer));
    dma_channel_configure((uint)ch, &c, &pwm_hw->slice[slice].cc, buf, (uint)n, true);
}

/* sine, near enough: two parabolas; phase 0..2^32, result +-32768 */
static inline int wave(uint32_t ph)
{
    int x = (int)(ph >> 16 & 0x7fff);
    int y = x * (32768 - x) >> 13;
    return ph & 0x80000000u ? -y : y;
}

typedef struct { int hz, amp, tau_ms; } partial_t;

/* Where the output is now: a sound still playing is cut there, and the
 * next one starts from that level instead of jumping. */
static int level_now(void)
{
    if (!dma_channel_is_busy((uint)ch))
        return 0;
    int i = (int)((const uint16_t *)dma_hw->ch[ch].read_addr - buf);
    dma_channel_abort((uint)ch);
    return i > 0 && i <= BUF_N ? buf[i - 1] : MID;
}

/* smooth step from a to b, i of n */
static inline int ease(int a, int b, int i, int n)
{
    int t = i * 256 / n;
    return a + (b - a) * (t * t * (768 - 2 * t) >> 16) / 256;
}

/* Partials decaying on their own; n samples, faded in over 1.4 ms (a tenth
 * for a short one) and out over the last 40 ms (a third). Around it the
 * output glides from `from` to the middle and back to 0: the PWM rests at 0
 * between sounds, so supply noise does not whistle in the speaker. Returns
 * the length. */
static int synth(const partial_t *p, int np, int n, int gain, int from)
{
    for (int i = 0; i < RAMP; i++)
        buf[i] = (uint16_t)ease(from, MID, i, RAMP);
    uint16_t *out = buf + RAMP;
    uint32_t ph[4] = { 0 }, inc[4], env[4], dec[4];   /* env, dec: 16.16 */
    for (int k = 0; k < np; k++) {
        inc[k] = (uint32_t)((uint64_t)p[k].hz * 4294967296ull / RATE);
        env[k] = 65536;
        dec[k] = 65536u - 65536u * 1000u / (uint32_t)(p[k].tau_ms * RATE);   /* per sample */
    }
    int vol = g_prefs.volume * gain;    /* 12 x 36 = 432 of MID */
    int head = n / 10 < 32 ? n / 10 + 1 : 32, tail = n / 3 < RATE / 25 ? n / 3 : RATE / 25;
    for (int i = 0; i < n; i++) {
        int s = 0;
        for (int k = 0; k < np; k++) {
            s += (p[k].amp * (int)(env[k] >> 8) >> 8) * wave(ph[k]) >> 15;
            ph[k] += inc[k];
            env[k] = env[k] * dec[k] >> 16;
        }
        if (i < head)                   /* no click at the start */
            s = s * i / head;
        if (i > n - tail)
            s = s * (n - i) / tail;
        out[i] = (uint16_t)(MID + s * vol / 32768);
    }
    for (int i = 0; i <= RAMP; i++)
        out[n + i] = (uint16_t)ease(MID, 0, i, RAMP);
    return RAMP + n + RAMP + 1;
}

void sound_click(void)
{
    static const partial_t p[] = { { 3800, 32000, 6 } };   /* high: the small speaker carries it */
    if (!up)
        return;
    int from = level_now();
    play(synth(p, 1, RATE * 15 / 1000, 42, from));
}

void sound_bell(void)
{
    /* Ringing glass: C7 and a second tone 3 Hz above it, so that it
     * shimmers as a struck wine glass does; the glass's own overtone at 2.32
     * times, shorter; and a bright "tink" at 4.25 times that is gone at once.
     * The overtones are not whole multiples: that makes it glass, not a
     * chord. */
    static const partial_t p[] = { { 2093, 14000, 900 }, { 2096, 6000, 900 },
                                   { 4856, 6000, 300 }, { 8895, 3500, 60 } };
    if (!up)
        return;
    int from = level_now();
    play(synth(p, 4, BELL_N, 36, from));
}
