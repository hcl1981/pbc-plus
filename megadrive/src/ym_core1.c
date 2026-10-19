/* ym_core1.c -- see ym_core1.h */
#include "pico/stdlib.h"
#include "hardware/sync.h"
#include "hardware/structs/timer.h"

#include "ym_core1.h"
#include "audio_pwm.h"
#include "bus/gwenesis_bus.h"
#include "sound/ym2612.h"
#include "sound/gwenesis_sn76489.h"

/* core 1's YM2612, ym2612_synth.c */
void synth_YM2612Init(void);
void synth_YM2612ResetChip(void);
void synth_YM2612Config(unsigned char type);
void synth_YM2612Write(unsigned int a, unsigned int v, int target);
void synth_ym2612_run(int target);

static int16_t synth_buf[GWENESIS_AUDIO_BUFFER_MAX];
int16_t *synth_ym2612_buffer = synth_buf;
volatile int synth_ym2612_index, synth_ym2612_clock;

volatile uint32_t ym_core1_busy_us;

/* Entry: type (2 bits) | clock / 4 (20 bits) | port (2) | value (8). */
#define T_YM    0u
#define T_PSG   1u
#define T_END   2u
#define RING_N  4096u                   /* a few frames of DAC writes */
#define STEP    16                      /* samples per step on a long gap */

static uint32_t ring[RING_N];
static volatile uint32_t wr, rd;

static inline void put(uint32_t e)
{
    uint32_t w = wr;
    while (w - rd >= RING_N)            /* core 1 behind: wait for room */
        tight_loop_contents();
    ring[w & (RING_N - 1)] = e;
    __dmb();
    wr = w + 1;
}

static inline uint32_t stamp(int clock)
{
    uint32_t q = clock > 0 ? (uint32_t)clock >> 2 : 0;
    return (q > 0xFFFFFu ? 0xFFFFFu : q) << 10;
}

void ym_core1_log_ym(unsigned int port, unsigned int value, int clock)
{
    put(T_YM << 30 | stamp(clock) | (port & 3) << 8 | (value & 0xFF));
}

void ym_core1_log_psg(int value, int clock)
{
    put(T_PSG << 30 | stamp(clock) | (value & 0xFF));
}

void ym_core1_frame_end(int clock, bool pal)
{
    put(T_END << 30 | stamp(clock) | pal);
}

void ym_core1_init(void)
{
    synth_YM2612Init();
    synth_YM2612ResetChip();
    synth_YM2612Config(YM2612_DISCRETE);
    synth_ym2612_clock = synth_ym2612_index = 0;
    sn76489_clock = sn76489_index = 0;
    wr = rd = 0;
}

/* One-pole DC blocker, as in ../sms: the discrete YM2612's DAC ladder adds
 * an offset that must not reach the speaker. */
static int16_t dc_block(int32_t x)
{
    static int32_t prev, dc;
    int32_t y = x - prev + dc - (dc >> 9);
    prev = x;
    dc = y;
    if (y > 32767) y = 32767;
    if (y < -32768) y = -32768;
    return (int16_t)y;
}

/* The frame's YM2612 + PSG samples (53 kHz) to the speaker (22 kHz): each
 * output sample is the average of the input samples it covers, carried
 * over frame boundaries. */
static void frame_out(bool pal)
{
    static int16_t out[GWENESIS_AUDIO_BUFFER_MAX / 2 + 2];
    static int32_t acc;
    static int cnt;
    static uint32_t frac;
    const uint32_t step = (uint32_t)(((uint64_t)(pal ? GWENESIS_AUDIO_FREQ_PAL
                                                     : GWENESIS_AUDIO_FREQ_NTSC) << 16) / AUDIO_RATE);
    int n = synth_ym2612_index < sn76489_index ? synth_ym2612_index : sn76489_index;
    int m = 0;
    for (int i = 0; i < n; i++) {
        acc += synth_buf[i] + gwenesis_sn76489_buffer[i];
        cnt++;
        frac += 1u << 16;
        if (frac >= step) {
            frac -= step;
            out[m++] = dc_block(acc / cnt);
            acc = 0;
            cnt = 0;
        }
    }
    audio_pwm_push(out, m);
    synth_ym2612_clock = synth_ym2612_index = 0;
    sn76489_clock = sn76489_index = 0;
}

void __not_in_flash_func(ym_core1_service)(void)
{
    uint32_t r = rd;
    if (r == wr)
        return;
    uint32_t t0 = timer_hw->timerawl;
    uint32_t e = ring[r & (RING_N - 1)];
    int clock = (int)(((e >> 10) & 0xFFFFFu) << 2);
    int limit = synth_ym2612_clock + STEP * AUDIO_FREQ_DIVISOR;
    if (clock > limit && limit < GWENESIS_AUDIO_BUFFER_MAX * AUDIO_FREQ_DIVISOR) {
        /* a long stretch without writes: in steps, so that core 1 gets
         * back to drawing lines in between */
        synth_ym2612_run(limit);
        gwenesis_SN76489_run(limit);
    } else {
        gwenesis_SN76489_run(clock);
        switch (e >> 30) {
        case T_YM:  synth_YM2612Write((e >> 8) & 3, e & 0xFF, clock); break;
        case T_PSG: gwenesis_SN76489_Apply(e & 0xFF, clock); break;
        default:
            synth_ym2612_run(clock);
            frame_out(e & 1);
            break;
        }
        __dmb();
        rd = r + 1;
    }
    ym_core1_busy_us += timer_hw->timerawl - t0;
}
