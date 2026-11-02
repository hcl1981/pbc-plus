/* gwenesis_sn76489.c -- SN76489 PSG for the Mega Drive emulator
 *
 * Written for the PicoBoy Color Plus port (GPL-3.0-or-later). It replaces
 * the PSG based on Maxim's code with Genesis Plus GX's changes, whose
 * licence does not allow use in a commercial product, and keeps its
 * interface, its timing (gwenesis_SN76489_run) and its output level:
 * a tone channel swings between -vol and +vol, the noise channel between 0
 * and 2 * vol, with vol 2800 at full volume and 2 dB less per step.
 *
 * The chip, as documented by the SMS Power development pages:
 *   - three tone generators: a 10-bit counter, clocked at clock / 16, that
 *     flips its output each time it runs out and reloads the period
 *   - a noise generator: its counter reloads 16, 32 or 64, or follows tone
 *     generator 2; each rising edge of its output shifts a register whose
 *     bit 0 is the output; white noise feeds back the parity of two taps,
 *     periodic noise bit 0
 *   - 4-bit attenuation per channel in 2 dB steps, 15 = off
 *   - writes: a latch byte (1 cc t dddd) selects a register and sets its
 *     low 4 bits, a data byte (0 - dddddd) sets the high 6 bits of a tone
 *     period or replaces the other registers
 * Periods up to 6 are far above hearing; such a channel just holds its
 * level, as the code before did.
 */
#pragma GCC optimize("O2")

#include <string.h>
#include <stdbool.h>
#include "gwenesis_bus.h"
#include "gwenesis_sn76489.h"
#include "gwenesis_savestate.h"
#ifdef PBC_YM_CORE1
#include "ym_core1.h"
#endif

#define FULL_VOLUME 2800
#define CUTOFF      6               /* periods up to this: inaudible, held */

static int volume[16];              /* attenuation -> amplitude */
static SN76489_Context psg;

void gwenesis_SN76489_Init(int PSGClockValue, int SamplingRate, int freq_divisor, int type)
{
    /* 2 dB per step: amplitude x 10^(-1/10) each, 15 = off */
    float a = FULL_VOLUME;
    for (int i = 0; i < 15; i++) {
        volume[i] = (int)a;
        a *= 0.794328234f;
    }
    volume[15] = 0;

    psg.step = (int)((double)PSGClockValue / 16.0 / SamplingRate * 65536.0 + 0.5);
    psg.divisor = freq_divisor;
    if (type == PSG_DISCRETE) {
        psg.noise_width = 14;
        psg.noise_taps = 0x0006;
        psg.zero_period = 0x400;
    } else {
        psg.noise_width = 15;
        psg.noise_taps = 0x0009;
        psg.zero_period = 1;
    }
    gwenesis_SN76489_Reset();
}

void gwenesis_SN76489_Reset()
{
    for (int i = 0; i < 3; i++) {
        psg.regs[2 * i] = 1;
        psg.regs[2 * i + 1] = 0x0F;
    }
    psg.regs[6] = 0;
    psg.regs[7] = 0x0F;
    psg.latch = 3;                  /* tone 2 attenuation is latched at power-on */
    psg.lfsr = (UINT16)(1 << psg.noise_width);
    for (int i = 0; i < 4; i++) {
        psg.count[i] = 0;
        psg.level[i] = -1;
    }
    sn76489_index = 0;
    sn76489_clock = 0;
}

void gwenesis_SN76489_SetContext(uint8 *data) { memcpy(&psg, data, sizeof psg); }
void gwenesis_SN76489_GetContext(uint8 *data) { memcpy(data, &psg, sizeof psg); }
uint8 *gwenesis_SN76489_GetContextPtr() { return (uint8 *)&psg; }
int gwenesis_SN76489_GetContextSize(void) { return sizeof psg; }

static inline int parity(int v)
{
    v ^= v >> 8;
    v ^= v >> 4;
    v ^= v >> 2;
    v ^= v >> 1;
    return v & 1;
}

/* Each output sample takes every channel's level at that moment: a tone
 * +vol or -vol, the noise 0 or 2 * vol. No averaging over the sample (the
 * PicoBoy's speaker would not show the difference), no division. */
static void render(INT16 *buffer, int length)
{
    const int step = psg.step;
    const int v[3] = { volume[psg.regs[1]], volume[psg.regs[3]], volume[psg.regs[5]] };
    const int vn = volume[psg.regs[7]] * 2;
    const int period[3] = { psg.regs[0] << 16, psg.regs[2] << 16, psg.regs[4] << 16 };
    const int ctrl = psg.regs[6];
    int nperiod = (ctrl & 3) == 3 ? psg.regs[4] : 0x10 << (ctrl & 3);
    if (nperiod < 1)
        nperiod = 1;
    nperiod <<= 16;
    const int white = ctrl & 4, taps = psg.noise_taps, width = psg.noise_width;

    int c0 = psg.count[0], c1 = psg.count[1], c2 = psg.count[2], cn = psg.count[3];
    int l0 = psg.level[0], l1 = psg.level[1], l2 = psg.level[2], ln = psg.level[3];
    int lfsr = psg.lfsr;
    const bool h0 = psg.regs[0] <= CUTOFF, h1 = psg.regs[2] <= CUTOFF, h2 = psg.regs[4] <= CUTOFF;
    if (h0) l0 = 1;
    if (h1) l1 = 1;
    if (h2) l2 = 1;

    for (int j = 0; j < length; j++) {
        if (!h0) for (c0 -= step; c0 <= 0; c0 += period[0]) l0 = -l0;
        if (!h1) for (c1 -= step; c1 <= 0; c1 += period[1]) l1 = -l1;
        if (!h2) for (c2 -= step; c2 <= 0; c2 += period[2]) l2 = -l2;
        for (cn -= step; cn <= 0; cn += nperiod) {
            ln = -ln;
            if (ln > 0) {               /* rising edge: shift */
                int in = white ? parity(lfsr & taps) : (lfsr & 1);
                lfsr = (lfsr >> 1) | (in << width);
            }
        }
        buffer[j] = (INT16)(l0 * v[0] + l1 * v[1] + l2 * v[2] + (lfsr & 1) * vn);
    }

    psg.count[0] = c0; psg.count[1] = c1; psg.count[2] = c2; psg.count[3] = cn;
    psg.level[0] = l0; psg.level[1] = l1; psg.level[2] = l2; psg.level[3] = ln;
    psg.lfsr = (UINT16)lfsr;
}

/* PBC: in RAM, it runs on every PSG write */
__attribute__((section(".time_critical.sn76489"))) void gwenesis_SN76489_run(int target)
{
    if (sn76489_clock >= target)
        return;

    /* With sound off, nothing the CPUs can read back depends on the
     * samples: only the clock advances. */
    extern bool audio_enabled;
    if (!audio_enabled) {
        sn76489_clock = target;
        return;
    }

    int prev = sn76489_index;
    sn76489_index += (target - sn76489_clock) / psg.divisor;
    if (sn76489_index > GWENESIS_AUDIO_BUFFER_MAX)
        sn76489_index = GWENESIS_AUDIO_BUFFER_MAX;

    if (sn76489_index > prev) {
        render(gwenesis_sn76489_buffer + prev, sn76489_index - prev);
        sn76489_clock = sn76489_index * psg.divisor;
    } else {
        sn76489_index = prev;
    }
}

uint32_t psg_writes_total;

#ifdef PBC_YM_CORE1
/* PBC: with sound on, the PSG only runs on core 1: core 0 logs the write
 * and core 1 applies it (gwenesis_SN76489_Apply) at the same time. */
void gwenesis_SN76489_Write(int data, int target)
{
    extern bool audio_enabled;
    if (audio_enabled)
        ym_core1_log_psg(data, target);
}

void gwenesis_SN76489_Apply(int data, int target)
#else
void gwenesis_SN76489_Write(int data, int target)
#endif
{
    psg_writes_total++;
    if (GWENESIS_AUDIO_ACCURATE == 1)
        gwenesis_SN76489_run(target);

    int r;
    if (data & 0x80) {              /* latch: 1 cc t dddd */
        r = psg.latch = (data >> 4) & 7;
        psg.regs[r] = (UINT16)((psg.regs[r] & 0x3F0) | (data & 0x0F));
    } else {                        /* data:  0 - dddddd */
        r = psg.latch;
        if (r < 6 && !(r & 1))      /* tone period: the high 6 bits */
            psg.regs[r] = (UINT16)((psg.regs[r] & 0x00F) | ((data & 0x3F) << 4));
        else
            psg.regs[r] = (UINT16)(data & 0x0F);
    }

    if (r < 6 && !(r & 1)) {
        if (psg.regs[r] == 0)
            psg.regs[r] = (UINT16)psg.zero_period;
    } else if (r == 6) {            /* a noise write restarts the register */
        psg.lfsr = (UINT16)(1 << psg.noise_width);
    }
}

void gwenesis_sn76489_save_state()
{
    SaveState *state = saveGwenesisStateOpenForWrite("sn76489");
    saveGwenesisStateSetBuffer(state, "gwenesis_SN76489", &psg, sizeof psg);
}

void gwenesis_sn76489_load_state()
{
    SaveState *state = saveGwenesisStateOpenForRead("sn76489");
    saveGwenesisStateGetBuffer(state, "gwenesis_SN76489", &psg, sizeof psg);
}
