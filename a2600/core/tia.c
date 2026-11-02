/* tia.c -- see tia.h
 *
 * Each object (two players, two missiles, the ball) is kept as the set of
 * pixels it covers on the current line, rebuilt only when one of its
 * registers or its position changes. A pixel is then six bits (playfield,
 * ball, missile 1, player 1, missile 0, player 0); two 64-entry tables
 * give its colour source (by the playfield priority) and its collisions.
 *
 * Positions: a RESPx/RESMx/RESBL strobe puts the object where the beam is,
 * players 5 pixels and missiles and the ball 4 pixels later (the TIA's
 * start-up delay), at the left edge if the strobe falls into the
 * horizontal blank. HMOVE moves every object by -8..+7 pixels at once and
 * blanks the first 8 pixels of the line (the "HMOVE bars").
 *
 * Sound: each channel divides the 31.4 kHz audio clock by its frequency
 * register + 1 and steps a waveform chosen by AUDC: pure tones (divide by
 * 2, 6, 31, 93), the 4-, 5- and 9-bit polynomial counters and their
 * combinations.
 */
#include <string.h>
#include <math.h>
#include "tia.h"

#ifdef PBC_ON_DEVICE
#define TIA_RAM __attribute__((section(".time_critical.tia")))
#else
#define TIA_RAM
#endif

enum { P0, P1, M0, M1, BL };

/* collision bits, in the order of the read registers */
enum {
    C_M0P1 = 1 << 0, C_M0P0 = 1 << 1, C_M1P0 = 1 << 2, C_M1P1 = 1 << 3,
    C_P0PF = 1 << 4, C_P0BL = 1 << 5, C_P1PF = 1 << 6, C_P1BL = 1 << 7,
    C_M0PF = 1 << 8, C_M0BL = 1 << 9, C_M1PF = 1 << 10, C_M1BL = 1 << 11,
    C_BLPF = 1 << 12, C_P0P1 = 1 << 13, C_M0M1 = 1 << 14,
};

/* pixel bits */
#define B_P0 0x01
#define B_M0 0x02
#define B_P1 0x04
#define B_M1 0x08
#define B_BL 0x10
#define B_PF 0x20

/* colour sources */
enum { S_BK, S_PF, S_P0, S_P1, S_PF0, S_PF1 };  /* S_PF0/1: score mode halves */

uint32_t tia_palette[128];

static struct {
    uint8_t col[4];         /* P0/M0, P1/M1, PF/BL, BK: palette index */
    uint8_t ctrlpf, pf[3];
    uint8_t nusiz[2], refp[2];
    uint8_t grp_new[2], grp_old[2];
    uint8_t enam[2], enabl_new, enabl_old;
    uint8_t vdelp[2], vdelbl, resmp[2];
    int8_t  hm[5];
    uint8_t pos[5];
    uint8_t vblank, vsync, vsync_started;
    uint8_t fire[2];
    uint16_t cx;            /* collisions */
    uint8_t audc[2], audf[2], audv[2];
} t;

static uint8_t pix[TIA_WIDTH];
static int x_drawn;                     /* pixels of this line drawn so far */
static uint8_t hmove_blank;             /* the first 8 pixels of this line */
static uint8_t objs[TIA_WIDTH];         /* per pixel: the B_* bits of the objects */
static uint8_t dirty;                   /* objects to rebuild */
static uint8_t pf_bits[40];
static uint8_t src_tab[2][64];          /* [priority][pixel bits] -> source */
static uint16_t cx_tab[64];
static uint8_t reg_tab[2][64];          /* [right half][pixel bits] -> t.col[] */
uint8_t tia_wsync_req;

/* the pixels each object covered when it was last built: up to three
 * spans (start, width) */
static uint8_t span_n[5], span_x[5][3], span_w[5][3];

/* ---- tables -------------------------------------------------------------- */

static TIA_RAM void make_tables(void)
{
    for (int b = 0; b < 64; b++) {
        uint16_t c = 0;
        int p0 = b & B_P0, m0 = b & B_M0, p1 = b & B_P1, m1 = b & B_M1, bl = b & B_BL, pf = b & B_PF;
        if (m0 && p1) c |= C_M0P1;
        if (m0 && p0) c |= C_M0P0;
        if (m1 && p0) c |= C_M1P0;
        if (m1 && p1) c |= C_M1P1;
        if (p0 && pf) c |= C_P0PF;
        if (p0 && bl) c |= C_P0BL;
        if (p1 && pf) c |= C_P1PF;
        if (p1 && bl) c |= C_P1BL;
        if (m0 && pf) c |= C_M0PF;
        if (m0 && bl) c |= C_M0BL;
        if (m1 && pf) c |= C_M1PF;
        if (m1 && bl) c |= C_M1BL;
        if (bl && pf) c |= C_BLPF;
        if (p0 && p1) c |= C_P0P1;
        if (m0 && m1) c |= C_M0M1;
        cx_tab[b] = c;
        /* normal: players over the playfield; priority: the other way */
        int s0 = (p0 || m0) ? S_P0 : (p1 || m1) ? S_P1 : (pf || bl) ? S_PF : S_BK;
        int s1 = (pf || bl) ? S_PF : (p0 || m0) ? S_P0 : (p1 || m1) ? S_P1 : S_BK;
        src_tab[0][b] = (uint8_t)s0;
        src_tab[1][b] = (uint8_t)s1;
    }
}

/* NTSC colours from the TIA's hue (0 grey, 1-15 around the colour wheel in
 * steps of about 25.7 degrees, 1 being gold) and luminance (8 steps) */
void tia_make_palette(void)
{
    for (int i = 0; i < 128; i++) {
        int hue = i >> 3, lum = i & 7;
        double y = 0.06 + lum * 0.134;
        double ii = 0, qq = 0;
        if (hue) {
            double a = (hue - 1) * (2 * M_PI / 14.0) + 2.85;
            double s = 0.24;
            ii = s * cos(a);
            qq = s * sin(a);
        }
        double r = y + 0.956 * ii + 0.621 * qq;
        double g = y - 0.272 * ii - 0.647 * qq;
        double b = y - 1.106 * ii + 1.703 * qq;
        int R = (int)(r * 255), G = (int)(g * 255), B = (int)(b * 255);
        R = R < 0 ? 0 : R > 255 ? 255 : R;
        G = G < 0 ? 0 : G > 255 ? 255 : G;
        B = B < 0 ? 0 : B > 255 ? 255 : B;
        tia_palette[i] = (uint32_t)(R << 16 | G << 8 | B);
    }
}

/* ---- objects ------------------------------------------------------------- */

static const uint8_t copy_off[8][3] = {
    { 0, 0, 0 }, { 0, 16, 0 }, { 0, 32, 0 }, { 0, 16, 32 },
    { 0, 64, 0 }, { 0, 0, 0 }, { 0, 32, 64 }, { 0, 0, 0 },
};
static const uint8_t copies[8] = { 1, 2, 2, 3, 2, 1, 3, 1 };

static const uint8_t obj_bit[5] = { B_P0, B_P1, B_M0, B_M1, B_BL };

static inline void span_or(int x, int w, uint8_t m)
{
    uint8_t *p = objs + x;
    if (x + w > TIA_WIDTH) {
        int k = TIA_WIDTH - x;
        for (int i = 0; i < k; i++) p[i] |= m;
        p = objs;
        w -= k;
    }
    for (int i = 0; i < w; i++) p[i] |= m;
}

static inline void span_and(int x, int w, uint8_t m)
{
    uint8_t *p = objs + x;
    if (x + w > TIA_WIDTH) {
        int k = TIA_WIDTH - x;
        for (int i = 0; i < k; i++) p[i] &= m;
        p = objs;
        w -= k;
    }
    for (int i = 0; i < w; i++) p[i] &= m;
}

static const uint8_t rev4[16] = { 0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15 };

static TIA_RAM void build(int o)
{
    uint8_t bitm = obj_bit[o];
    for (int k = 0; k < span_n[o]; k++)
        span_and(span_x[o][k], span_w[o][k], (uint8_t)~bitm);
    span_n[o] = 0;
    int n, w, x0 = t.pos[o];
    if (o <= P1) {
        int i = o;
        uint8_t g = t.vdelp[i] ? t.grp_old[i] : t.grp_new[i];
        if (!g)
            return;
        n = t.nusiz[i] & 7;
        int size = n == 5 ? 2 : n == 7 ? 4 : 1;
        if (!t.refp[i])                         /* bit 7 first: reverse */
            g = (uint8_t)(rev4[g & 15] << 4 | rev4[g >> 4]);
        w = 8 * size;
        for (int k = 0; k < copies[n]; k++) {
            int start = x0 + copy_off[n][k];
            if (start >= TIA_WIDTH)
                start -= TIA_WIDTH;
            span_x[o][k] = (uint8_t)start;
            span_w[o][k] = (uint8_t)w;
            for (int b = 0; b < 8; b++)
                if ((g >> b) & 1) {
                    int x = start + b * size;
                    if (x >= TIA_WIDTH)
                        x -= TIA_WIDTH;
                    span_or(x, size, bitm);
                }
        }
        span_n[o] = (uint8_t)copies[n];
        return;
    } else if (o <= M1) {
        int i = o - M0;
        if (!(t.enam[i] & 2) || (t.resmp[i] & 2))
            return;
        n = t.nusiz[i] & 7;
        w = 1 << ((t.nusiz[i] >> 4) & 3);
    } else {
        uint8_t en = t.vdelbl ? t.enabl_old : t.enabl_new;
        if (!(en & 2))
            return;
        n = 0;
        w = 1 << ((t.ctrlpf >> 4) & 3);
    }
    for (int k = 0; k < copies[n]; k++) {
        int start = x0 + copy_off[n][k];
        if (start >= TIA_WIDTH)
            start -= TIA_WIDTH;
        span_x[o][k] = (uint8_t)start;
        span_w[o][k] = (uint8_t)w;
        span_or(start, w, bitm);
    }
    span_n[o] = (uint8_t)copies[n];
}

/* the playfield bits of one register (0: PF0, 1: PF1, 2: PF2), both halves */
static TIA_RAM void make_pf(int r)
{
    int i0 = r == 0 ? 0 : r == 1 ? 4 : 12, n = r ? 8 : 4;
    uint8_t v = t.pf[r];
    uint8_t *p = pf_bits + i0;
    switch (r) {
    case 0: for (int b = 4; b < 8; b++) *p++ = (uint8_t)(((v >> b) & 1) * B_PF); break;
    case 1: for (int b = 7; b >= 0; b--) *p++ = (uint8_t)(((v >> b) & 1) * B_PF); break;
    default: for (int b = 0; b < 8; b++) *p++ = (uint8_t)(((v >> b) & 1) * B_PF); break;
    }
    if (t.ctrlpf & 1)
        for (int i = i0; i < i0 + n; i++) pf_bits[39 - i] = pf_bits[i];
    else
        for (int i = i0; i < i0 + n; i++) pf_bits[20 + i] = pf_bits[i];
}

/* which colour register each combination of pixel bits shows, for the left
 * and the right half (they differ in score mode) */
static TIA_RAM void make_regs(void)
{
    int pri = (t.ctrlpf >> 2) & 1;
    int score = (t.ctrlpf & 2) && !pri;
    for (int half = 0; half < 2; half++)
        for (int b = 0; b < 64; b++) {
            uint8_t c;
            switch (src_tab[pri][b]) {
            case S_P0: c = 0; break;
            case S_P1: c = 1; break;
            case S_PF: c = (score && !(b & B_BL)) ? (uint8_t)half : 2; break;
            default:   c = 3; break;
            }
            reg_tab[half][b] = c;
        }
}

/* ---- drawing ------------------------------------------------------------- */

static TIA_RAM void draw_to(int x_end)
{
    if (x_end > TIA_WIDTH)
        x_end = TIA_WIDTH;
    int x = x_drawn;
    if (x >= x_end)
        return;
    x_drawn = x_end;
    if (t.vblank & 2) {                         /* beam off */
        memset(pix + x, 0, (size_t)(x_end - x));
        return;
    }
    if (dirty) {
        for (int o = 0; o < 5; o++)
            if (dirty & (1 << o))
                build(o);
        dirty = 0;
    }
    uint16_t cx = 0;
    const uint8_t *col = t.col;
    const uint8_t *reg = reg_tab[0];
    int mid = x_end < 80 ? x_end : 80;
    for (;;) {
        for (; x < mid; x++) {
            int b = objs[x] | pf_bits[x >> 2];
            cx |= cx_tab[b];
            pix[x] = col[reg[b]];
        }
        if (mid == x_end)
            break;
        mid = x_end;
        reg = reg_tab[1];
    }
    t.cx |= cx;
    if (hmove_blank)                            /* the HMOVE bar */
        for (int k = 0; k < 8 && k < x_end; k++)
            pix[k] = 0;
}

static inline int xpos(int clock)
{
    return clock - 68;
}

static TIA_RAM void set_pos(int o, int clock)
{
    int x = xpos(clock);
    int delay = o <= P1 ? 5 : 4;
    int p = x < 0 ? (o <= P1 ? 3 : 2) : x + delay;
    t.pos[o] = (uint8_t)(p % TIA_WIDTH);
    dirty |= (uint8_t)(1 << o);
}

/* a missile locked to its player sits at the player's middle */
static TIA_RAM void lock_missile(int i)
{
    int n = t.nusiz[i] & 7;
    int mid = n == 5 ? 8 : n == 7 ? 16 : 4;
    t.pos[M0 + i] = (uint8_t)((t.pos[P0 + i] + mid) % TIA_WIDTH);
    dirty |= (uint8_t)(1 << (M0 + i));
}

/* ---- registers ----------------------------------------------------------- */

void tia_reset(void)
{
    memset(&t, 0, sizeof t);
    memset(pix, 0, sizeof pix);
    memset(objs, 0, sizeof objs);
    memset(span_n, 0, sizeof span_n);
    x_drawn = 0;
    hmove_blank = 0;
    dirty = 0x1f;
    tia_wsync_req = 0;
    make_tables();
    for (int r = 0; r < 3; r++)
        make_pf(r);
    make_regs();
}

TIA_RAM void tia_write(uint8_t reg, uint8_t v, int clock)
{
    reg &= 0x3f;
    /* draw up to here first; a register written while its pixel is drawn
     * shows from the next one. Not needed for the registers that change
     * nothing on the screen by themselves: VSYNC, WSYNC, RSYNC, sound,
     * HMxx, HMCLR. */
    if (!((0x0000081F07E0000Dull >> reg) & 1))
        draw_to(xpos(clock) + 1);
    switch (reg) {
    case 0x00:                                  /* VSYNC */
        if ((v & 2) && !(t.vsync & 2))
            t.vsync_started = 1;
        t.vsync = v;
        break;
    case 0x01: t.vblank = v; break;             /* VBLANK */
    case 0x02: tia_wsync_req = 1; break;        /* WSYNC */
    case 0x04: case 0x05:                       /* NUSIZ0/1 */
        t.nusiz[reg - 4] = v;
        dirty |= (uint8_t)((1 << (P0 + reg - 4)) | (1 << (M0 + reg - 4)));
        break;
    case 0x06: t.col[0] = v >> 1; break;
    case 0x07: t.col[1] = v >> 1; break;
    case 0x08: t.col[2] = v >> 1; break;
    case 0x09: t.col[3] = v >> 1; break;
    case 0x0a: {
        uint8_t old = t.ctrlpf;
        t.ctrlpf = v;
        if ((old ^ v) & 1)
            for (int r = 0; r < 3; r++)
                make_pf(r);
        if ((old ^ v) & 0x30)
            dirty |= 1 << BL;
        if ((old ^ v) & 6)
            make_regs();
        break;
    }
    case 0x0b: t.refp[0] = (v >> 3) & 1; dirty |= 1 << P0; break;
    case 0x0c: t.refp[1] = (v >> 3) & 1; dirty |= 1 << P1; break;
    case 0x0d: case 0x0e: case 0x0f:
        if (t.pf[reg - 0x0d] != v) {
            t.pf[reg - 0x0d] = v;
            make_pf(reg - 0x0d);
        }
        break;
    case 0x10: set_pos(P0, clock); break;
    case 0x11: set_pos(P1, clock); break;
    case 0x12: set_pos(M0, clock); break;
    case 0x13: set_pos(M1, clock); break;
    case 0x14: set_pos(BL, clock); break;
    case 0x15: t.audc[0] = v & 15; break;
    case 0x16: t.audc[1] = v & 15; break;
    case 0x17: t.audf[0] = v & 31; break;
    case 0x18: t.audf[1] = v & 31; break;
    case 0x19: t.audv[0] = v & 15; break;
    case 0x1a: t.audv[1] = v & 15; break;
    case 0x1b:                                  /* GRP0: also delays GRP1 */
        if (t.grp_new[0] != v && !t.vdelp[0]) dirty |= 1 << P0;
        if (t.grp_old[1] != t.grp_new[1] && t.vdelp[1]) dirty |= 1 << P1;
        t.grp_new[0] = v;
        t.grp_old[1] = t.grp_new[1];
        break;
    case 0x1c:                                  /* GRP1: also delays GRP0, ENABL */
        if (t.grp_new[1] != v && !t.vdelp[1]) dirty |= 1 << P1;
        if (t.grp_old[0] != t.grp_new[0] && t.vdelp[0]) dirty |= 1 << P0;
        if (t.enabl_old != t.enabl_new && t.vdelbl) dirty |= 1 << BL;
        t.grp_new[1] = v;
        t.grp_old[0] = t.grp_new[0];
        t.enabl_old = t.enabl_new;
        break;
    case 0x1d: t.enam[0] = v; dirty |= 1 << M0; break;
    case 0x1e: t.enam[1] = v; dirty |= 1 << M1; break;
    case 0x1f: if (!t.vdelbl) dirty |= 1 << BL; t.enabl_new = v; break;
    case 0x20: case 0x21: case 0x22: case 0x23: case 0x24:   /* HMxx */
        t.hm[reg - 0x20] = (int8_t)((int8_t)v >> 4);
        break;
    case 0x25: t.vdelp[0] = v & 1; dirty |= 1 << P0; break;
    case 0x26: t.vdelp[1] = v & 1; dirty |= 1 << P1; break;
    case 0x27: t.vdelbl = v & 1; dirty |= 1 << BL; break;
    case 0x28: case 0x29: {                     /* RESMP0/1 */
        int i = reg - 0x28;
        if ((t.resmp[i] & 2) && !(v & 2))
            lock_missile(i);
        t.resmp[i] = v;
        if (v & 2)
            lock_missile(i);
        break;
    }
    case 0x2a:                                  /* HMOVE */
        for (int o = 0; o < 5; o++)
            t.pos[o] = (uint8_t)((t.pos[o] - t.hm[o] + TIA_WIDTH) % TIA_WIDTH);
        for (int i = 0; i < 2; i++)
            if (t.resmp[i] & 2)
                lock_missile(i);
        dirty = 0x1f;
        if (xpos(clock) < 0)
            hmove_blank = 1;
        break;
    case 0x2b: memset(t.hm, 0, sizeof t.hm); break;   /* HMCLR */
    case 0x2c: t.cx = 0; break;                        /* CXCLR */
    }
}

TIA_RAM uint8_t tia_read(uint8_t reg, int clock)
{
    reg &= 0x0f;
    draw_to(xpos(clock));
    uint16_t c = t.cx;
    uint8_t v;
    switch (reg) {
    case 0x0: v = (uint8_t)(((c & C_M0P1) ? 0x80 : 0) | ((c & C_M0P0) ? 0x40 : 0)); break;
    case 0x1: v = (uint8_t)(((c & C_M1P0) ? 0x80 : 0) | ((c & C_M1P1) ? 0x40 : 0)); break;
    case 0x2: v = (uint8_t)(((c & C_P0PF) ? 0x80 : 0) | ((c & C_P0BL) ? 0x40 : 0)); break;
    case 0x3: v = (uint8_t)(((c & C_P1PF) ? 0x80 : 0) | ((c & C_P1BL) ? 0x40 : 0)); break;
    case 0x4: v = (uint8_t)(((c & C_M0PF) ? 0x80 : 0) | ((c & C_M0BL) ? 0x40 : 0)); break;
    case 0x5: v = (uint8_t)(((c & C_M1PF) ? 0x80 : 0) | ((c & C_M1BL) ? 0x40 : 0)); break;
    case 0x6: v = (uint8_t)((c & C_BLPF) ? 0x80 : 0); break;
    case 0x7: v = (uint8_t)(((c & C_P0P1) ? 0x80 : 0) | ((c & C_M0M1) ? 0x40 : 0)); break;
    case 0xc: v = t.fire[0] ? 0x00 : 0x80; break;
    case 0xd: v = t.fire[1] ? 0x00 : 0x80; break;
    default:  v = 0; break;                     /* paddles: none */
    }
    return v;                                   /* undriven bits: 0 */
}

TIA_RAM void tia_end_line(tia_line_fn out)
{
    draw_to(TIA_WIDTH);
    if (out)
        out(pix, (t.vblank & 2) != 0);
    x_drawn = 0;
    hmove_blank = 0;
}

TIA_RAM bool tia_vsync_started(void)
{
    bool s = t.vsync_started;
    t.vsync_started = 0;
    return s;
}

bool tia_vsync(void)
{
    return (t.vsync & 2) != 0;
}

TIA_RAM bool tia_wsync(void)
{
    bool w = tia_wsync_req;
    tia_wsync_req = 0;
    return w;
}

void tia_set_fire(int player, bool pressed)
{
    t.fire[player & 1] = pressed;
}

/* ---- sound --------------------------------------------------------------- */

static struct {
    uint8_t div;            /* frequency divider */
    uint8_t poly4, poly5;
    uint16_t poly9;
    uint8_t cnt;            /* pure tone counters */
    uint8_t out;
} ch[2];

static inline int step_poly4(int i) { uint8_t p = ch[i].poly4; ch[i].poly4 = (uint8_t)(((p << 1) | (((p >> 3) ^ (p >> 2)) & 1)) & 15); return ch[i].poly4 & 1; }
static inline int step_poly5(int i) { uint8_t p = ch[i].poly5; ch[i].poly5 = (uint8_t)(((p << 1) | (((p >> 4) ^ (p >> 2)) & 1)) & 31); return ch[i].poly5 & 1; }
static inline int step_poly9(int i) { uint16_t p = ch[i].poly9; ch[i].poly9 = (uint16_t)(((p << 1) | (((p >> 8) ^ (p >> 4)) & 1)) & 511); return ch[i].poly9 & 1; }

static TIA_RAM void audio_clock(int i)
{
    if (++ch[i].div <= t.audf[i])
        return;
    ch[i].div = 0;
    if (!ch[i].poly4) ch[i].poly4 = 15;
    if (!ch[i].poly5) ch[i].poly5 = 31;
    if (!ch[i].poly9) ch[i].poly9 = 511;
    switch (t.audc[i]) {
    case 0x0: case 0xb: ch[i].out = 1; break;                     /* set */
    case 0x1: ch[i].out = (uint8_t)step_poly4(i); break;            /* 4-bit poly */
    case 0x2:                                                       /* /15 -> poly4 */
        if (++ch[i].cnt >= 15) { ch[i].cnt = 0; ch[i].out = (uint8_t)step_poly4(i); }
        break;
    case 0x3:                                                       /* poly5 -> poly4 */
        if (step_poly5(i)) ch[i].out = (uint8_t)step_poly4(i);
        break;
    case 0x4: case 0x5: ch[i].out ^= 1; break;                      /* /2 */
    case 0x6: case 0xa:                                             /* /31 */
        if (++ch[i].cnt >= 31) ch[i].cnt = 0;
        ch[i].out = ch[i].cnt < 18;
        break;
    case 0x7:                                                       /* poly5 -> /2 */
        if (step_poly5(i)) ch[i].out ^= 1;
        break;
    case 0x8: ch[i].out = (uint8_t)step_poly9(i); break;            /* 9-bit poly */
    case 0x9: ch[i].out = (uint8_t)step_poly5(i); break;            /* 5-bit poly */
    case 0xc: case 0xd:                                             /* /6 */
        if (++ch[i].cnt >= 3) { ch[i].cnt = 0; ch[i].out ^= 1; }
        break;
    case 0xe:                                                       /* /93 */
        if (++ch[i].cnt >= 93) ch[i].cnt = 0;
        ch[i].out = ch[i].cnt < 47;
        break;
    case 0xf:                                                       /* poly5 /6 */
        if (++ch[i].cnt >= 3) { ch[i].cnt = 0; ch[i].out = (uint8_t)step_poly5(i); }
        break;
    }
}

TIA_RAM int tia_audio_line(int16_t *buf)
{
    for (int k = 0; k < 2; k++) {
        audio_clock(0);
        audio_clock(1);
        int v = (ch[0].out ? t.audv[0] : 0) + (ch[1].out ? t.audv[1] : 0);
        buf[k] = (int16_t)(v * 700);
    }
    return 2;
}
