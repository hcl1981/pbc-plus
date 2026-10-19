/* audio_pwm.c -- see audio_pwm.h */
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "audio_pwm.h"

#define AUDIO_PIN  15
#define PWM_WRAP   1023u                  /* 10 bit; ~244 kHz carrier at 250 MHz */
#define PWM_MID    (PWM_WRAP / 2)

#define RING_N     2048u                  /* 92 ms at 22 kHz */
#define RING_MASK  (RING_N - 1)
#define FILL_TARGET 740                   /* two frames */
#define FILL_LOW    400
#define FILL_HIGH   1100

static uint16_t ring[RING_N];
static volatile uint32_t head;            /* written by core 0 */
static volatile uint32_t tail;            /* advanced by the timer */
static uint slice, chan;
static repeating_timer_t timer;
static alarm_pool_t *pool;          /* NULL: default pool (core 0) */
static bool running;
static int  volume;

static bool __not_in_flash_func(tick)(repeating_timer_t *t)
{
    (void)t;
    uint32_t tl = tail;
    if (tl != head) {
        pwm_set_chan_level(slice, chan, ring[tl & RING_MASK]);
        tail = tl + 1;
    }
    return true;                          /* empty: hold the last level */
}

void audio_pwm_init(void)
{
    slice = pwm_gpio_to_slice_num(AUDIO_PIN);
    chan  = pwm_gpio_to_channel(AUDIO_PIN);     /* channel B */
    pwm_config c = pwm_get_default_config();
    pwm_config_set_wrap(&c, PWM_WRAP);
    pwm_config_set_clkdiv(&c, 1.0f);
    pwm_init(slice, &c, true);

    gpio_init(AUDIO_PIN);
    gpio_set_dir(AUDIO_PIN, GPIO_OUT);
    gpio_put(AUDIO_PIN, 0);
}

void audio_pwm_set_pool(alarm_pool_t *p)
{
    pool = p;
}

void audio_pwm_set_volume(int vol)
{
    if (vol < 0) vol = 0;
    if (vol > AUDIO_VOL_MAX) vol = AUDIO_VOL_MAX;
    volume = vol;

    if (vol && !running) {
        head = tail = 0;
        pwm_set_chan_level(slice, chan, PWM_MID);
        gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);
        /* Negative period: fixed rate, measured from the previous start. */
        if (pool)
            alarm_pool_add_repeating_timer_us(pool, -AUDIO_TICK_US, tick, NULL, &timer);
        else
            add_repeating_timer_us(-AUDIO_TICK_US, tick, NULL, &timer);
        running = true;
    } else if (!vol && running) {
        cancel_repeating_timer(&timer);
        running = false;
        gpio_init(AUDIO_PIN);             /* no idle current through the speaker */
        gpio_set_dir(AUDIO_PIN, GPIO_OUT);
        gpio_put(AUDIO_PIN, 0);
    }
}

static inline uint16_t to_level(int32_t s)
{
    s = s * volume / 4;                   /* volume 4 = unity */
    /* compress peaks instead of clipping them */
    const int32_t th = 24000;
    if (s > th)  s = th + (s - th) / 8;
    if (s < -th) s = -th + (s + th) / 8;
    if (s > 32767)  s = 32767;
    if (s < -32768) s = -32768;
    return (uint16_t)((s + 32768) >> 6);  /* 16 -> 10 bit */
}

void audio_pwm_push(const int16_t *s, int n)
{
    if (!running || n <= 0)
        return;

    uint32_t h = head;
    uint32_t fill = h - tail;
    if (fill > RING_N - 64 || fill < FILL_LOW / 2) {
        /* far off (start, stall, emulator below full speed): restart at the
         * target fill with silence in front */
        uint32_t t = tail;
        h = t;
        for (int i = 0; i < FILL_TARGET - n && i < (int)RING_N - 64; i++)
            ring[h++ & RING_MASK] = PWM_MID;
    }
    fill = h - tail;
    bool drop = fill > FILL_HIGH, repeat = fill < FILL_LOW;

    for (int i = 0; i < n; i++) {
        if (drop && i == n / 2)
            continue;
        uint16_t v = to_level(s[i]);
        ring[h++ & RING_MASK] = v;
        if (repeat && i == n / 2)
            ring[h++ & RING_MASK] = v;
    }
    head = h;                             /* publish after the data */
}
