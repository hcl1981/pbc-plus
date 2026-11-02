/* scc.c -- see scc.h
 *
 * Each channel steps through its 32 wave samples, one per (period + 1)
 * clocks; the sample (signed 8 bits) is multiplied by the 4-bit volume.
 * Register layout of the SCC (Konami 051649):
 *   00-7F  waveforms of channels 1-4 (channel 5 plays channel 4's)
 *   80-89  periods of channels 1-5 (12 bits, low byte first)
 *   8A-8E  volumes of channels 1-5
 *   8F     channel enable, bits 0-4
 *   90-9F  mirror of 80-8F; E0-FF deformation (not emulated)
 */
#include <string.h>
#include "scc.h"

#ifdef PBC_ON_DEVICE
#define SCC_RAM __attribute__((section(".time_critical.scc")))
#else
#define SCC_RAM
#endif

static uint32_t clock_hz, rate_hz;

void scc_init(scc_t *s, uint32_t clock, uint32_t rate)
{
    memset(s, 0, sizeof *s);
    clock_hz = clock;
    rate_hz = rate;
}

static void set_inc(scc_t *s, int c)
{
    uint32_t p = s->period[c];
    /* very short periods are above hearing (and the chip is silent below 9) */
    s->inc[c] = p < 9 ? 0 : (uint32_t)(((uint64_t)clock_hz << 27) / ((uint64_t)(p + 1) * rate_hz));
}

void scc_write(scc_t *s, uint16_t addr, uint8_t v)
{
    int o = addr & 0xff;
    s->used = 1;
    if (o < 0x80) {
        s->wave[o >> 5][o & 31] = (int8_t)v;
        return;
    }
    if (o >= 0xa0)
        return;
    o = 0x80 | (o & 0x0f);
    if (o < 0x8a) {
        int c = (o - 0x80) >> 1;
        if (o & 1)
            s->period[c] = (uint16_t)((s->period[c] & 0xff) | ((v & 15) << 8));
        else
            s->period[c] = (uint16_t)((s->period[c] & 0xf00) | v);
        set_inc(s, c);
    } else if (o < 0x8f) {
        s->volume[o - 0x8a] = v & 15;
    } else {
        s->enable = v & 0x1f;
    }
}

uint8_t scc_read(scc_t *s, uint16_t addr)
{
    int o = addr & 0xff;
    if (o < 0x80)
        return (uint8_t)s->wave[o >> 5][o & 31];
    if (o >= 0xa0 && o < 0xc0)
        return (uint8_t)s->wave[3][o & 31];
    return 0xff;
}

SCC_RAM void scc_render(scc_t *s, int32_t *out, int n)
{
    if (!s->used)
        return;
    for (int c = 0; c < 5; c++) {
        if (!(s->enable & (1 << c)) || !s->volume[c] || !s->inc[c])
            continue;
        const int8_t *w = s->wave[c < 4 ? c : 3];
        int vol = s->volume[c];
        uint32_t ph = s->phase[c], inc = s->inc[c];
        for (int i = 0; i < n; i++) {
            out[i] += w[ph >> 27] * vol;
            ph += inc;
        }
        s->phase[c] = ph;
    }
}
