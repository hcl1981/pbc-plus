/* sn76496.c -- SN76489 PSG for SMS Plus
 *
 * Written for the PicoBoy Color Plus port (GPL-2.0-or-later). It replaces
 * the SN76496 code from the old MAME (free for non-commercial use only) and
 * keeps its interface and its output level: each channel gives the share
 * of the sample it spent high times its amplitude, 10922 at full volume
 * and 2 dB less per step, so three tones at full volume stay just below
 * the 16-bit limit; the sum is clipped at 0x7FFF.
 *
 * The chip, as documented by the SMS Power development pages:
 *   - three tone generators: a 10-bit counter, clocked at clock / 16, that
 *     flips its output each time it runs out and reloads the period; a
 *     period of 0 counts as 1 on Sega's chips
 *   - a noise generator: its counter reloads 16, 32 or 64, or follows tone
 *     generator 2; each rising edge of its output shifts a 16-bit register
 *     whose bit 0 is the output; white noise feeds back bit 0 XOR bit 3,
 *     periodic noise bit 0; a write to the noise register restarts it
 *   - 4-bit attenuation per channel in 2 dB steps, 15 = off
 *   - writes: a latch byte (1 cc t dddd) selects a register and sets its
 *     low 4 bits, a data byte (0 - dddddd) sets the high 6 bits of a tone
 *     period or replaces the other registers
 * Time runs in 1/65536 of an output sample; averaging every channel over
 * the sample keeps periods of 0 and 1 (sample playback through the volume)
 * a steady level instead of aliasing.
 */
#include "shared.h"

#define MAX_OUTPUT  0x7FFF
#define STEP        0x10000         /* one output sample */
#define LFSR_START  0x8000

t_SN76496 sn[MAX_76496];

void in_ram(SN76496Write)(int chip, int data)
{
    t_SN76496 *R = &sn[chip];
    int r;
    if (data & 0x80) {              /* latch: 1 cc t dddd */
        r = R->LastRegister = (data >> 4) & 7;
        R->Register[r] = (R->Register[r] & 0x3F0) | (data & 0x0F);
    } else {                        /* data:  0 - dddddd */
        r = R->LastRegister;
        if (r < 6 && !(r & 1))      /* tone period: the high 6 bits */
            R->Register[r] = (R->Register[r] & 0x00F) | ((data & 0x3F) << 4);
        else
            R->Register[r] = data & 0x0F;
    }
    if (r == 6)
        R->LFSR = LFSR_START;
}

/* Half period of a tone register, in sample time */
static inline int half_period(const t_SN76496 *R, int reg)
{
    int p = R->Register[reg];
    return (int)R->UpdateStep * (p ? p : 1);
}

/* How long a tone channel is high during the next sample (0..STEP). */
static inline int tone_high(t_SN76496 *R, int ch, int period)
{
    int high = 0, left = STEP, count = R->Count[ch], out = R->Output[ch];
    while (count <= left) {
        if (out) high += count;
        left -= count;
        out ^= 1;
        count = period;
    }
    if (out) high += left;
    R->Count[ch] = count - left;
    R->Output[ch] = out;
    return high;
}

/* The same for the noise channel: its output is bit 0 of the register. */
static inline int noise_high(t_SN76496 *R, int period)
{
    int high = 0, left = STEP, count = R->Count[3], ff = R->Output[3];
    unsigned int lfsr = R->LFSR;
    int white = R->Register[6] & 4;
    while (count <= left) {
        if (lfsr & 1) high += count;
        left -= count;
        ff ^= 1;
        count = period;
        if (ff) {                   /* rising edge: shift */
            unsigned int in = white ? ((lfsr ^ (lfsr >> 3)) & 1) : (lfsr & 1);
            lfsr = (lfsr >> 1) | (in << 15);
        }
    }
    if (lfsr & 1) high += left;
    R->Count[3] = count - left;
    R->Output[3] = ff;
    R->LFSR = lfsr;
    return high;
}

/* A channel nobody hears (attenuation 15, or off on both sides) only has
 * to come out in the same state after `length` samples: the flip-flop
 * flips at count, count + period, ... up to the end, in one step. The
 * per-sample loops above give the same, as a flip on a sample boundary
 * belongs to the sample before it there too. */
static inline void tone_skip(t_SN76496 *R, int ch, int period, int length)
{
    int t = length * STEP, count = R->Count[ch];
    if (count > t) {
        R->Count[ch] = count - t;
        return;
    }
    int flips = 1 + (t - count) / period;
    R->Output[ch] ^= flips & 1;
    R->Count[ch] = count + flips * period - t;
}

/* The same for the noise channel, edge by edge for the shift register */
static inline void noise_skip(t_SN76496 *R, int period, int length)
{
    int left = length * STEP, count = R->Count[3], ff = R->Output[3];
    unsigned int lfsr = R->LFSR;
    int white = R->Register[6] & 4;
    while (count <= left) {
        left -= count;
        ff ^= 1;
        count = period;
        if (ff) {
            unsigned int in = white ? ((lfsr ^ (lfsr >> 3)) & 1) : (lfsr & 1);
            lfsr = (lfsr >> 1) | (in << 15);
        }
    }
    R->Count[3] = count - left;
    R->Output[3] = ff;
    R->LFSR = lfsr;
}

void in_ram(SN76496Update)(int chip, INT16 *buffer[2], int length, unsigned char mask)
{
    t_SN76496 *R = &sn[chip];
    int period[3] = { half_period(R, 0), half_period(R, 2), half_period(R, 4) };
    int n = R->Register[6] & 3;
    int nperiod = n == 3 ? period[2] : (int)R->UpdateStep << (4 + n);
    int vol[4];
    for (int i = 0; i < 4; i++)
        vol[i] = R->VolTable[R->Register[2 * i + 1] & 0x0F];

    /* PBC: silent channels are wound forward at once instead of per
     * sample; a period of 0 or 1 cost several loop rounds per sample */
    for (int i = 0; i < 4; i++) {
        if (vol[i] && (mask & (0x11 << i)))
            continue;
        vol[i] = 0;
        if (i < 3)
            tone_skip(R, i, period[i], length);
        else
            noise_skip(R, nperiod, length);
    }

    for (int j = 0; j < length; j++) {
        int k[4];
        for (int i = 0; i < 3; i++)
            k[i] = vol[i] ? tone_high(R, i, period[i]) * vol[i] : 0;
        k[3] = vol[3] ? noise_high(R, nperiod) * vol[3] : 0;

        unsigned int left = 0, right = 0;
        for (int i = 0; i < 4; i++) {
            if (mask & (1 << (4 + i))) left += k[i];
            if (mask & (1 << i)) right += k[i];
        }
        if (left > MAX_OUTPUT * STEP) left = MAX_OUTPUT * STEP;
        if (right > MAX_OUTPUT * STEP) right = MAX_OUTPUT * STEP;
        buffer[0][j] = (INT16)(left / STEP);
        buffer[1][j] = (INT16)(right / STEP);
    }
}

void SN76496_set_clock(int chip, int clock)
{
    t_SN76496 *R = &sn[chip];
    R->UpdateStep = (unsigned int)(((double)STEP * R->SampleRate * 16) / clock);
}

/* gain 0: full volume MAX_OUTPUT / 3 per channel; each step up +0.2 dB */
void SN76496_set_gain(int chip, int gain)
{
    t_SN76496 *R = &sn[chip];
    double out = MAX_OUTPUT / 3;
    for (gain &= 0xFF; gain > 0; gain--)
        out *= 1.023292992;
    for (int i = 0; i < 15; i++) {
        R->VolTable[i] = out > MAX_OUTPUT / 3 ? MAX_OUTPUT / 3 : (int)out;
        out /= 1.258925412;         /* 2 dB */
    }
    R->VolTable[15] = 0;
}

int SN76496_init(int chip, int clock, int volume, int sample_rate)
{
    t_SN76496 *R = &sn[chip];
    memset(R, 0, sizeof *R);
    R->SampleRate = sample_rate;
    SN76496_set_clock(chip, clock);
    for (int i = 0; i < 8; i += 2)
        R->Register[i + 1] = 0x0F;  /* all channels off */
    for (int i = 0; i < 4; i++)
        R->Count[i] = (int)R->UpdateStep;
    R->LFSR = LFSR_START;
    SN76496_set_gain(chip, (volume >> 8) & 0xFF);
    return 0;
}
