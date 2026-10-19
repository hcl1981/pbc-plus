/* audio_pwm.c -- see audio_pwm.h
 *
 * A DMA channel paced by a DMA timer copies one sample every 45 us from a
 * ring buffer into the PWM compare register: no interrupt per sample (the
 * repeating timer before fired 22222 times a second and ran the SDK's
 * alarm code from flash). The DMA reads the ring endlessly. How far it
 * got follows from the time since it started, exactly, because the DMA
 * timer and the microsecond timer both run off the crystal. The producer
 * writes ahead of that point and fills the rest of the ring with silence,
 * so an emulator that falls behind is heard as silence, not as old
 * samples. */
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "audio_pwm.h"

#define AUDIO_PIN  15
#define PWM_WRAP   1023u                  /* 10 bit; 150-290 kHz carrier */
#define PWM_MID    (PWM_WRAP / 2)

#define RING_N     2048u                  /* 92 ms at 22 kHz */
#define RING_MASK  (RING_N - 1)
#define RING_BITS  12                     /* 2048 x 2 bytes, aligned to that */
#define FILL_TARGET 740                   /* two frames */
#define FILL_LOW    400
#define FILL_HIGH   1100

static uint16_t ring[RING_N] __attribute__((aligned(RING_N * 2)));
static uint32_t head;                     /* samples written, total */
static uint64_t start_us;                 /* when the DMA took sample 0 */
static uint slice;
static int  dma_ch = -1, dma_timer = -1;
static bool running;
static int  volume;

/* Samples the DMA has taken so far. */
static inline uint32_t tail(void)
{
    return (uint32_t)((time_us_64() - start_us) / AUDIO_TICK_US);
}

void audio_pwm_init(void)
{
    slice = pwm_gpio_to_slice_num(AUDIO_PIN);
    pwm_config c = pwm_get_default_config();
    pwm_config_set_wrap(&c, PWM_WRAP);
    pwm_config_set_clkdiv(&c, 1.0f);
    pwm_init(slice, &c, true);

    gpio_init(AUDIO_PIN);
    gpio_set_dir(AUDIO_PIN, GPIO_OUT);
    gpio_put(AUDIO_PIN, 0);

    dma_ch = dma_claim_unused_channel(true);
    dma_timer = dma_claim_unused_timer(true);
}

void audio_pwm_set_volume(int vol)
{
    if (vol < 0) vol = 0;
    if (vol > AUDIO_VOL_MAX) vol = AUDIO_VOL_MAX;
    volume = vol;

    if (vol && !running) {
        for (uint32_t i = 0; i < RING_N; i++)
            ring[i] = PWM_MID;
        head = 0;
        pwm_set_chan_level(slice, PWM_CHAN_B, PWM_MID);
        gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);

        /* one sample per AUDIO_TICK_US: clk_sys / (MHz * 45), exact for
         * every whole-MHz clock */
        dma_timer_set_fraction((uint)dma_timer, 1,
                               (uint16_t)(clock_get_hz(clk_sys) / 1000000u * AUDIO_TICK_US));
        dma_channel_config c = dma_channel_get_default_config((uint)dma_ch);
        channel_config_set_transfer_data_size(&c, DMA_SIZE_16);  /* lands in A and B */
        channel_config_set_read_increment(&c, true);
        channel_config_set_write_increment(&c, false);
        channel_config_set_ring(&c, false, RING_BITS);
        channel_config_set_dreq(&c, dma_get_timer_dreq((uint)dma_timer));
        dma_channel_configure((uint)dma_ch, &c, &pwm_hw->slice[slice].cc, ring,
                              dma_encode_endless_transfer_count(), false);
        start_us = time_us_64();
        dma_channel_start((uint)dma_ch);
        running = true;
    } else if (!vol && running) {
        dma_channel_abort((uint)dma_ch);
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
    if (n > FILL_HIGH)
        n = FILL_HIGH;

    uint32_t t = tail();
    uint32_t h = head;
    int32_t fill = (int32_t)(h - t);
    if (fill > (int32_t)(RING_N - 64) || fill < FILL_LOW / 2) {
        /* far off (start, stall, emulator below full speed; the DMA has
         * been playing the silence after the last sample): restart at the
         * target fill with silence in front */
        h = t + 2;                        /* a little ahead of the DMA */
        for (int i = 0; i < FILL_TARGET - n; i++)
            ring[h++ & RING_MASK] = PWM_MID;
    }
    fill = (int32_t)(h - t);
    bool drop = fill > FILL_HIGH, repeat = fill < FILL_LOW;

    for (int i = 0; i < n; i++) {
        if (drop && i == n / 2)
            continue;
        uint16_t v = to_level(s[i]);
        ring[h++ & RING_MASK] = v;
        if (repeat && i == n / 2)
            ring[h++ & RING_MASK] = v;
    }
    head = h;

    /* silence up to just before the DMA's position */
    uint32_t end = t + RING_N - 16;
    for (uint32_t i = h; (int32_t)(end - i) > 0; i++)
        ring[i & RING_MASK] = PWM_MID;
}
