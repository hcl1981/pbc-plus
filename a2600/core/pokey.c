/* pokey.c -- see pokey.h
 *
 * Time counts in machine clocks (1.79 MHz). Each channel keeps the clocks
 * left until its divider runs out; pokey_run() goes from one such event to
 * the next and sums every channel's output weighted by how long it lasted,
 * so tones above the sample rate come out as their mean instead of
 * aliasing. Channels with volume 0 that no filter depends on have no
 * events: their dividers are only moved on.
 *
 * At an event the channel's output flip-flop changes as AUDC bits 7-5 say:
 * bit 7 clear -- only when the 5-bit polynomial's bit is set; then bit 5
 * set -- it toggles (pure tone), else it takes the bit of the 4-bit
 * polynomial (bit 6 set) or of the 17-bit one (the 9-bit one if AUDCTL
 * bit 7). The polynomials run at the machine clock, so their bit is the
 * one at the time of the event. The 17-bit one is stepped once per event
 * instead: its length (2^17 - 1) is prime, so taking every n-th bit of it
 * gives a sequence just as random.
 */
#include <string.h>
#include <stdbool.h>
#include "pokey.h"

#ifdef PBC_ON_DEVICE
#define POKEY_RAM __attribute__((section(".time_critical.pokey")))
#else
#define POKEY_RAM
#endif

static uint8_t audf[4], audc[4], audctl;
static int cnt[4];                  /* clocks until the divider runs out */
static uint8_t ff[4], hp[2];        /* output flip-flops, high-pass latches */
static uint32_t p4, p5, p9;         /* positions in the short polynomials */
static uint32_t lfsr17 = 1;
static uint32_t poly4_bits, poly5_bits;
static uint8_t poly9_bits[64];
static int per_c[4];                /* period(c), worked out when a register changes */

static void make_polys(void)
{
    /* 4 bits x^4 + x^3 + 1, 5 bits x^5 + x^3 + 1, 9 bits x^9 + x^5 + 1 */
    uint32_t r = 1;
    poly4_bits = 0;
    for (int i = 0; i < 15; i++) {
        poly4_bits |= (r & 1) << i;
        r = (r >> 1) | ((((r >> 0) ^ (r >> 1)) & 1) << 3);
    }
    r = 1;
    poly5_bits = 0;
    for (int i = 0; i < 31; i++) {
        poly5_bits |= (r & 1) << i;
        r = (r >> 1) | ((((r >> 0) ^ (r >> 2)) & 1) << 4);
    }
    r = 1;
    memset(poly9_bits, 0, sizeof poly9_bits);
    for (int i = 0; i < 511; i++) {
        poly9_bits[i >> 3] |= (uint8_t)((r & 1) << (i & 7));
        r = (r >> 1) | ((((r >> 0) ^ (r >> 4)) & 1) << 8);
    }
}

static inline POKEY_RAM uint32_t step17(void)
{
    /* x^17 + x^12 + 1 */
    lfsr17 = (lfsr17 >> 1) | ((((lfsr17 >> 0) ^ (lfsr17 >> 5)) & 1) << 16);
    return lfsr17 & 1;
}

/* the divider of channel c in machine clocks; 0: the channel has no
 * events of its own (low half of a joined pair) */
static POKEY_RAM int period(int c)
{
    int base = (audctl & 0x01) ? 114 : 28;
    switch (c) {
    case 0:
        if (audctl & 0x10) return 0;
        return (audctl & 0x40) ? audf[0] + 4 : (audf[0] + 1) * base;
    case 1:
        if (audctl & 0x10) {
            int f = audf[1] << 8 | audf[0];
            return (audctl & 0x40) ? f + 7 : (f + 1) * base;
        }
        return (audf[1] + 1) * base;
    case 2:
        if (audctl & 0x08) return 0;
        return (audctl & 0x20) ? audf[2] + 4 : (audf[2] + 1) * base;
    default:
        if (audctl & 0x08) {
            int f = audf[3] << 8 | audf[2];
            return (audctl & 0x20) ? f + 7 : (f + 1) * base;
        }
        return (audf[3] + 1) * base;
    }
}

void pokey_reset(void)
{
    make_polys();
    memset(audf, 0, sizeof audf);
    memset(audc, 0, sizeof audc);
    memset(ff, 0, sizeof ff);
    hp[0] = hp[1] = 0;
    audctl = 0;
    p4 = p5 = p9 = 0;
    lfsr17 = 1;
    for (int c = 0; c < 4; c++)
        cnt[c] = per_c[c] = period(c);
}

POKEY_RAM void pokey_write(uint8_t reg, uint8_t v)
{
    reg &= 0x0f;
    if (reg < 8) {
        if (reg & 1)
            audc[reg >> 1] = v;
        else
            audf[reg >> 1] = v;
    } else if (reg == 0x08) {
        audctl = v;
    } else if (reg == 0x09) {           /* STIMER: all dividers start again */
        for (int c = 0; c < 4; c++)
            cnt[c] = period(c);
    }
    /* a new divider takes effect when the old one runs out */
    for (int c = 0; c < 4; c++) {
        int p = per_c[c] = period(c);
        if (p && cnt[c] > p)
            cnt[c] = p;
        if (!p)
            cnt[c] = 0;
        else if (cnt[c] <= 0)
            cnt[c] = p;
    }
}

POKEY_RAM uint8_t pokey_read(uint8_t reg)
{
    switch (reg & 0x0f) {
    case 0x0a: {                        /* RANDOM */
        uint8_t v = 0;
        for (int i = 0; i < 8; i++)
            v = (uint8_t)(v << 1 | step17());
        return v;
    }
    case 0x0e:                          /* IRQST: nothing pending */
    case 0x0f:                          /* SKSTAT */
        return 0xff;
    default:
        return 0;
    }
}

static inline POKEY_RAM int level(int c)
{
    int vol = audc[c] & 15;
    if (audc[c] & 0x10)
        return vol;                     /* volume only */
    int bit = ff[c];
    if (c == 0 && (audctl & 0x04)) bit ^= hp[0];
    if (c == 1 && (audctl & 0x02)) bit ^= hp[1];
    return bit ? vol : 0;
}

static POKEY_RAM void event(int c, int t)
{
    int d = audc[c] >> 5;
    if ((d & 4) || ((poly5_bits >> ((p5 + (uint32_t)t) % 31)) & 1)) {
        if (d & 1)
            ff[c] ^= 1;
        else if (d & 2)
            ff[c] = (uint8_t)((poly4_bits >> ((p4 + (uint32_t)t) % 15)) & 1);
        else if (audctl & 0x80) {
            uint32_t i = (p9 + (uint32_t)t) % 511;
            ff[c] = (poly9_bits[i >> 3] >> (i & 7)) & 1;
        } else
            ff[c] = (uint8_t)step17();
    }
    if (c == 2) hp[0] = ff[0];          /* channel 3 clocks channel 1's filter */
    if (c == 3) hp[1] = ff[1];
}

POKEY_RAM int pokey_run(int n)
{
    if (n <= 0)
        return 0;
    int per[4];
    bool live[4];
    for (int c = 0; c < 4; c++) {
        per[c] = per_c[c];
        bool needed = (audc[c] & 15) && !(audc[c] & 0x10);
        if (c == 2 && (audctl & 0x04) && (audc[0] & 15)) needed = true;
        if (c == 3 && (audctl & 0x02) && (audc[1] & 15)) needed = true;
        live[c] = per[c] && needed;
        if (per[c] && !live[c]) {       /* just move the divider on */
            int left = cnt[c] - n;
            cnt[c] = left > 0 ? left : per[c] - ((-left) % per[c]);
        }
    }
    int sum = level(0) + level(1) + level(2) + level(3);
    int32_t acc = 0;
    int t = 0;
    while (t < n) {
        int step = n - t;
        for (int c = 0; c < 4; c++)
            if (live[c] && cnt[c] < step)
                step = cnt[c];
        acc += sum * step;
        t += step;
        bool ev = false;
        for (int c = 0; c < 4; c++) {
            if (!live[c])
                continue;
            cnt[c] -= step;
            if (cnt[c] <= 0) {
                event(c, t);
                cnt[c] = per[c];
                ev = true;
            }
        }
        if (ev)
            sum = level(0) + level(1) + level(2) + level(3);
    }
    p4 = (p4 + (uint32_t)n) % 15;
    p5 = (p5 + (uint32_t)n) % 31;
    p9 = (p9 + (uint32_t)n) % 511;
    return (int)((acc << 8) / n);
}
