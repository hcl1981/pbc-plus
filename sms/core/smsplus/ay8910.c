/* ay8910.c -- see ay8910.h
 *
 * Everything steps at clock / 8: the tone outputs flip after 'period'
 * steps (a square wave of clock / (16 * period)); the noise generator
 * shifts a 17-bit register (taps 0 and 3) every 2 * 'period' steps; the
 * envelope takes the next of its 16 levels every 2 * 'period' steps
 * (a cycle of clock / (256 * period)) and follows the shape in register
 * 13 (continue, attack, alternate, hold). Each output sample is the
 * average of the chip's states over its duration, which keeps high tones
 * from aliasing.
 */
#include <stdbool.h>
#include <string.h>
#include "ay8910.h"

#if PICO_ON_DEVICE
#define AY_RAM __attribute__((section(".time_critical.ay")))
#else
#define AY_RAM
#endif

/* 16 levels, 3 dB per two steps (the chip's logarithmic DAC) */
static const uint16_t vol_tab[16] = {
    0, AY_MAX / 128, AY_MAX / 90, AY_MAX / 64, AY_MAX / 45, AY_MAX / 32, AY_MAX / 23, AY_MAX / 16,
    AY_MAX / 11, AY_MAX / 8, (AY_MAX * 10) / 57, AY_MAX / 4, (AY_MAX * 10) / 28, AY_MAX / 2,
    (AY_MAX * 10) / 14, AY_MAX,
};

static const uint8_t reg_mask[16] = {
    0xff, 0x0f, 0xff, 0x0f, 0xff, 0x0f, 0x1f, 0xff,
    0x1f, 0x1f, 0x1f, 0xff, 0xff, 0x0f, 0xff, 0xff,
};

void ay_init(ay8910_t *ay, uint32_t clock, uint32_t rate)
{
    memset(ay, 0, sizeof *ay);
    /* clock / 8 steps per sample, 16.16 */
    ay->step = (uint32_t)(((uint64_t)clock << 16) / 8 / rate);
    ay_reset(ay);
}

static void env_start(ay8910_t *ay)
{
    ay->env_step = 0;
    ay->env_cnt = 0;
    ay->env_hold = 0;
    ay->env_up = (ay->reg[13] >> 2) & 1;
    ay->env_vol = ay->env_up ? 0 : 15;
}

void ay_reset(ay8910_t *ay)
{
    uint32_t step = ay->step;
    memset(ay, 0, sizeof *ay);
    ay->step = step;
    ay->lfsr = 1;
    ay->reg[7] = 0xff;
    env_start(ay);
}

void ay_write_addr(ay8910_t *ay, uint8_t v)
{
    ay->addr = v & 15;
}

void ay_write_data(ay8910_t *ay, uint8_t v)
{
    int r = ay->addr;
    ay->reg[r] = v & reg_mask[r];
    if (r == 13)
        env_start(ay);
}

uint8_t ay_read_data(ay8910_t *ay, ay_port_read_t port_a)
{
    int r = ay->addr;
    if (r == 14)
        return port_a ? port_a() : 0xff;
    return ay->reg[r];
}

/* one step of clock / 8 */
static inline void tick(ay8910_t *ay)
{
    for (int c = 0; c < 3; c++) {
        int period = ay->reg[c * 2] | (ay->reg[c * 2 + 1] << 8);
        if (!period) period = 1;
        if (++ay->tone_cnt[c] >= period) {
            ay->tone_cnt[c] = 0;
            ay->tone_out[c] ^= 1;
        }
    }
    int np = ay->reg[6];
    if (!np) np = 1;
    if (++ay->noise_cnt >= np * 2) {    /* the noise runs at half that rate */
        ay->noise_cnt = 0;
        uint32_t bit = (ay->lfsr ^ (ay->lfsr >> 3)) & 1;
        ay->lfsr = (ay->lfsr >> 1) | (bit << 16);
        ay->noise_out = ay->lfsr & 1;
    }
    /* envelope: 16 levels per clock / (256 * period) */
    uint32_t ep = ay->reg[11] | (ay->reg[12] << 8);
    if (!ep) ep = 1;
    if (!ay->env_hold && ++ay->env_cnt >= ep * 2) {
        ay->env_cnt = 0;
        if (++ay->env_step >= 16) {         /* end of a cycle */
            int shape = ay->reg[13];
            if (!(shape & 8)) {                     /* once, then silent */
                ay->env_hold = 1;
                ay->env_vol = 0;
                return;
            }
            if (shape & 1) {                        /* hold */
                ay->env_hold = 1;
                int last = ay->env_up ? 15 : 0;
                ay->env_vol = (uint8_t)((shape & 2) ? 15 - last : last);
                return;
            }
            if (shape & 2)                          /* alternate */
                ay->env_up ^= 1;
            ay->env_step = 0;
        }
        ay->env_vol = ay->env_up ? ay->env_step : 15 - ay->env_step;
    }
}

static inline int level(const ay8910_t *ay)
{
    int mix = ay->reg[7];
    int sum = 0;
    for (int c = 0; c < 3; c++) {
        int on = (ay->tone_out[c] | ((mix >> c) & 1)) & (ay->noise_out | ((mix >> (c + 3)) & 1));
        if (!on)
            continue;
        int v = ay->reg[8 + c];
        sum += vol_tab[(v & 0x10) ? ay->env_vol : (v & 15)];
    }
    return sum;
}

/* Steps where nothing changes the level are counted at once -- the same
 * result as stepping one by one, at a fraction of the work. A counter
 * whose output is not heard right now (tone off in the mixer, volume 0,
 * noise used by no channel) is wound on by arithmetic, flips included,
 * so its phase is right when it is heard again; the others and the
 * envelope set how far the level stays as it is. */
static inline int tone_period(const ay8910_t *ay, int c)
{
    int p = ay->reg[c * 2] | (ay->reg[c * 2 + 1] << 8);
    return p ? p : 1;
}

static inline int noise_period(const ay8910_t *ay)
{
    return ay->reg[6] ? ay->reg[6] * 2 : 2;
}

/* n steps of the tone and noise counters */
static inline void wind(ay8910_t *ay, int n)
{
    for (int c = 0; c < 3; c++) {
        int p = tone_period(ay, c), cnt = ay->tone_cnt[c];
        int first = p - cnt < 1 ? 1 : p - cnt;          /* steps to the next flip */
        if (n < first) {
            ay->tone_cnt[c] = (uint16_t)(cnt + n);
        } else {
            int rest = n - first;
            ay->tone_out[c] ^= (uint8_t)((1 + rest / p) & 1);
            ay->tone_cnt[c] = (uint16_t)(rest % p);
        }
    }
    int q = noise_period(ay), cnt = ay->noise_cnt;
    int first = q - cnt < 1 ? 1 : q - cnt;
    if (n < first) {
        ay->noise_cnt = (uint8_t)(cnt + n);
    } else {
        int rest = n - first;
        for (int e = 1 + rest / q; e > 0; e--) {
            uint32_t bit = (ay->lfsr ^ (ay->lfsr >> 3)) & 1;
            ay->lfsr = (ay->lfsr >> 1) | (bit << 16);
        }
        ay->noise_out = ay->lfsr & 1;
        ay->noise_cnt = (uint8_t)(rest % q);
    }
}

/* steps until a heard counter or the envelope runs out (>= 1) */
static inline int next_event(const ay8910_t *ay, int limit)
{
    int d = limit, mix = ay->reg[7];
    bool noise_heard = false;
    for (int c = 0; c < 3; c++) {
        int v = ay->reg[8 + c];
        if (!(v & 0x1f))
            continue;                   /* volume 0: not heard */
        if (!((mix >> (c + 3)) & 1))
            noise_heard = true;
        if ((mix >> c) & 1)
            continue;                   /* tone off */
        int t = tone_period(ay, c) - ay->tone_cnt[c];
        if (t < d) d = t;
    }
    if (noise_heard) {
        int t = noise_period(ay) - ay->noise_cnt;
        if (t < d) d = t;
    }
    if (!ay->env_hold) {
        int ep = ay->reg[11] | (ay->reg[12] << 8);
        int t = (ep ? ep : 1) * 2 - (int)ay->env_cnt;
        if (t < d) d = t;
    }
    return d < 1 ? 1 : d;
}

AY_RAM void ay_render(ay8910_t *ay, int32_t *out, int n)
{
    for (int s = 0; s < n; s++) {
        ay->frac += ay->step;
        int k = (int)(ay->frac >> 16);
        ay->frac &= 0xffff;
        if (!k) {
            out[s] += level(ay);
            continue;
        }
        int acc = 0;
        for (int left = k; left > 0; ) {
            int d = next_event(ay, left);
            int quiet = d - 1;          /* steps without a heard change */
            if (quiet) {
                acc += level(ay) * quiet;
                wind(ay, quiet);
                if (!ay->env_hold)
                    ay->env_cnt += (uint32_t)quiet;
            }
            tick(ay);
            acc += level(ay);
            left -= d;
        }
        out[s] += acc / k;
    }
}
