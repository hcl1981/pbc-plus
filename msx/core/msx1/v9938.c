/* v9938.c -- see v9938.h
 *
 * VRAM is kept as the CPU sees it (linear addresses); graphics 6 and 7 are
 * interleaved inside the real chip, which nobody can tell from outside.
 * The screen modes by the bits M5..M1 (R0 bits 3..1, R1 bits 3 and 4):
 *   G1 00000  G2 00100  G3 01000  G4 01100  G5 10000  G6 10100  G7 11100
 *   MC 00010  T1 00001  T2 01001
 */
#include <string.h>
#include "v9938.h"

#ifdef PBC_ON_DEVICE
#define V99_RAM __attribute__((section(".time_critical.v99")))
#else
#define V99_RAM
#endif

uint16_t (*v99_rgb565)(int r, int g, int b);
int v99_hpos;

enum { M_G1, M_G2, M_G3, M_G4, M_G5, M_G6, M_G7, M_MC, M_T1, M_T2 };

static uint8_t vram[0x20000];
static uint8_t reg[48];
static uint8_t stat[10];
static uint16_t pal565[16];
static uint16_t g7_565[256];            /* graphics 7: GGGRRRBB */
static uint16_t spr7_565[16];           /* sprite colours in graphics 7 */
static uint32_t addr;                   /* 17 bits */
static uint8_t latch, readahead, pal_latch;
static bool latched, pal_latched, vr;
static int mode;
static uint32_t frames;                 /* for the text 2 blinking */

/* ---- colours ---------------------------------------------------------------- */

static const uint8_t default_pal[16][3] = {     /* R, G, B (0..7) */
    {0,0,0}, {0,0,0}, {1,6,1}, {3,7,3}, {1,1,7}, {2,3,7}, {5,1,1}, {2,6,7},
    {7,1,1}, {7,3,3}, {6,6,1}, {6,6,4}, {1,4,1}, {6,2,5}, {5,5,5}, {7,7,7},
};
static const uint8_t spr7_grb[16][3] = {        /* G, R, B (0..7) */
    {0,0,0}, {0,0,2}, {0,3,0}, {0,3,2}, {3,0,0}, {3,0,2}, {3,3,0}, {3,3,2},
    {4,7,2}, {0,0,7}, {0,7,0}, {0,7,7}, {7,0,0}, {7,0,7}, {7,7,0}, {7,7,7},
};

static V99_RAM uint16_t rgb(int r, int g, int b)
{
    return v99_rgb565 ? v99_rgb565(r * 255 / 7, g * 255 / 7, b * 255 / 7) : 0;
}

static bool tab_dirty = true;           /* the drawing tables are stale */

static V99_RAM void set_pal(int i, int r, int g, int b)
{
    pal565[i & 15] = rgb(r, g, b);
    tab_dirty = true;
}

/* ---- modes -------------------------------------------------------------------- */

static V99_RAM void update_mode(void)
{
    int m = ((reg[0] >> 3) & 1) << 4 | ((reg[0] >> 2) & 1) << 3 | ((reg[0] >> 1) & 1) << 2 |
            ((reg[1] >> 3) & 1) << 1 | ((reg[1] >> 4) & 1);
    switch (m) {
    case 0x04: mode = M_G2; break;
    case 0x08: mode = M_G3; break;
    case 0x0c: mode = M_G4; break;
    case 0x10: mode = M_G5; break;
    case 0x14: mode = M_G6; break;
    case 0x1c: mode = M_G7; break;
    case 0x02: mode = M_MC; break;
    case 0x01: mode = M_T1; break;
    case 0x09: mode = M_T2; break;
    default:   mode = M_G1; break;
    }
}

static inline bool tms_mode(void)
{
    return mode == M_G1 || mode == M_G2 || mode == M_MC || mode == M_T1;
}

int v99_lines(void)
{
    return (reg[9] & 0x80) ? 212 : 192;
}

V99_RAM bool v99_irq(void)
{
    return ((stat[0] & 0x80) && (reg[1] & 0x20)) || ((stat[1] & 1) && (reg[0] & 0x10));
}

/* ---- the command engine ---------------------------------------------------------- */

enum { C_STOP = 0, C_POINT = 4, C_PSET, C_SRCH, C_LINE, C_LMMV, C_LMMM, C_LMCM, C_LMMC,
       C_HMMV, C_HMMM, C_YMMM, C_HMMC };

static struct {
    int cmd, op;
    int sx, sy, dx, dy, nx, ny;         /* as set */
    int x, y, i, j;                     /* transfer through the CPU: where it is */
    int dix, diy;
    bool ce, tr;
} c;

static inline int cmd_w(void) { return (mode == M_G5 || mode == M_G6) ? 512 : 256; }
static inline int cmd_ymask(void) { return (mode == M_G6 || mode == M_G7) ? 511 : 1023; }
static inline int ppb(void) { return mode == M_G5 ? 4 : mode == M_G7 ? 1 : 2; }       /* pixels per byte */
static inline int bpl(void) { return (mode == M_G6 || mode == M_G7) ? 256 : 128; }    /* bytes per line */

/* what the mode means for the dots, taken once per command instead of
 * per dot (the loops below run from RAM, with these in registers) */
enum { K_NIB, K_2BIT, K_BYTE };         /* graphics 4/6, graphics 5, else bytes */
typedef struct {
    int wmask, ymask, bpl, kind;
    uint8_t cmask;                      /* the colour bits of a dot */
} cmd_mode_t;

static inline cmd_mode_t cmd_mode(void)
{
    cmd_mode_t m;
    m.wmask = cmd_w() - 1;
    m.ymask = cmd_ymask();
    m.bpl = bpl();
    m.kind = (mode == M_G4 || mode == M_G6) ? K_NIB : mode == M_G5 ? K_2BIT : K_BYTE;
    m.cmask = mode == M_G5 ? 3 : mode == M_G7 ? 0xff : 15;
    return m;
}

static inline uint8_t pget_m(const cmd_mode_t *m, int x, int y)
{
    x &= m->wmask;
    y &= m->ymask;
    switch (m->kind) {
    case K_NIB:  { uint8_t b = vram[(y * m->bpl + (x >> 1)) & 0x1ffff]; return (x & 1) ? b & 15 : b >> 4; }
    case K_2BIT: { uint8_t b = vram[(y * 128 + (x >> 2)) & 0x1ffff]; return (b >> ((3 - (x & 3)) * 2)) & 3; }
    default:     return vram[(y * 256 + x) & 0x1ffff];
    }
}

static inline uint8_t logop(uint8_t dst, uint8_t src, int op, uint8_t mask)
{
    switch (op & 7) {
    case 0:  return src;
    case 1:  return dst & src;
    case 2:  return dst | src;
    case 3:  return dst ^ src;
    case 4:  return (uint8_t)~src & mask;
    default: return dst;
    }
}

static inline void pset_m(const cmd_mode_t *m, int x, int y, uint8_t col, int op)
{
    x &= m->wmask;
    y &= m->ymask;
    uint8_t mask = m->cmask;
    col &= mask;
    if ((op & 8) && !col)
        return;                         /* T: colour 0 leaves the dot */
    switch (m->kind) {
    case K_NIB: {
        uint8_t *b = &vram[(y * m->bpl + (x >> 1)) & 0x1ffff];
        int sh = (x & 1) ? 0 : 4;
        uint8_t v = logop((uint8_t)((*b >> sh) & 15), col, op, mask);
        *b = (uint8_t)((*b & ~(15 << sh)) | (v << sh));
        break;
    }
    case K_2BIT: {
        uint8_t *b = &vram[(y * 128 + (x >> 2)) & 0x1ffff];
        int sh = (3 - (x & 3)) * 2;
        uint8_t v = logop((uint8_t)((*b >> sh) & 3), col, op, mask);
        *b = (uint8_t)((*b & ~(3 << sh)) | (v << sh));
        break;
    }
    default: {
        uint8_t *b = &vram[(y * 256 + x) & 0x1ffff];
        *b = logop(*b, col, op, mask);
        break;
    }
    }
}

/* one dot, in the mode of this moment (transfers through the CPU) */
static V99_RAM uint8_t pget(int x, int y)
{
    cmd_mode_t m = cmd_mode();
    return pget_m(&m, x, y);
}

static V99_RAM void pset(int x, int y, uint8_t col, int op)
{
    cmd_mode_t m = cmd_mode();
    pset_m(&m, x, y, col, op);
}

static inline uint8_t *byte_at_m(const cmd_mode_t *m, int bx, int y)
{
    return &vram[((y & m->ymask) * m->bpl + (bx & (m->bpl - 1))) & 0x1ffff];
}

static inline uint8_t *byte_at(int bx, int y)
{
    return &vram[((y & cmd_ymask()) * bpl() + (bx & (bpl() - 1))) & 0x1ffff];
}

static int reg16(int r, int mask) { return (reg[r] | reg[r + 1] << 8) & mask; }

static void set_reg16(int r, int v)
{
    reg[r] = (uint8_t)v;
    reg[r + 1] = (uint8_t)((v >> 8) & 3);
}

static void cmd_end(void)
{
    c.ce = c.tr = false;
    c.cmd = C_STOP;
}

/* the next dot (or byte) of a transfer through the CPU */
static V99_RAM void transfer_step(void)
{
    c.x += c.dix;
    if (++c.i >= c.nx) {
        c.i = 0;
        c.x = c.cmd == C_HMMC ? c.dx / ppb() : c.cmd == C_LMCM ? c.sx : c.dx;
        c.y += c.diy;
        if (++c.j >= c.ny) {
            if (c.cmd == C_LMCM) set_reg16(34, c.y & 1023);
            else set_reg16(38, c.y & 1023);
            cmd_end();
        }
    }
}

static V99_RAM void transfer_in(uint8_t v)      /* HMMC, LMMC: a byte from the CPU */
{
    if (!c.ce)
        return;
    if (c.cmd == C_HMMC)
        *byte_at(c.x, c.y) = v;
    else
        pset(c.x, c.y, v, c.op);
    transfer_step();
    if (c.ce)
        c.tr = true;
}

static V99_RAM void cmd_start(uint8_t v)
{
    int arg = reg[45];
    c.cmd = v >> 4;
    c.op = v & 15;
    c.sx = reg16(32, 511);
    c.sy = reg16(34, 1023);
    c.dx = reg16(36, 511);
    c.dy = reg16(38, 1023);
    c.nx = reg16(40, 1023);
    c.ny = reg16(42, 1023);
    c.dix = (arg & 4) ? -1 : 1;
    c.diy = (arg & 8) ? -1 : 1;
    uint8_t clr = reg[44];
    int w = cmd_w();
    if (!c.nx) c.nx = w;
    if (!c.ny) c.ny = 1024;
    if (c.nx > w) c.nx = w;
    c.ce = true;
    c.tr = false;
    stat[2] &= (uint8_t)~0x10;          /* BD */

    /* the loops work on copies: VRAM stores (bytes) would make the
     * compiler reload c and the mode after each dot */
    const cmd_mode_t m = cmd_mode();
    const int sx = c.sx, sy = c.sy, dx = c.dx, dy = c.dy, nx = c.nx, ny = c.ny;
    const int dix = c.dix, diy = c.diy, op = c.op;

    switch (c.cmd) {
    case C_STOP:
        cmd_end();
        break;
    case C_POINT:
        stat[7] = pget_m(&m, sx, sy);
        cmd_end();
        break;
    case C_PSET:
        pset_m(&m, dx, dy, clr, op);
        cmd_end();
        break;
    case C_SRCH: {
        bool want_diff = (arg & 2) != 0;
        uint8_t want = clr & m.cmask;
        int x = sx;
        for (int n = 0; n < w; n++) {
            if ((pget_m(&m, x, sy) == want) != want_diff) {
                stat[2] |= 0x10;
                stat[8] = (uint8_t)x;
                stat[9] = (uint8_t)(0xfe | ((x >> 8) & 1));
                break;
            }
            x += dix;
            if (x < 0 || x >= w)
                break;
        }
        cmd_end();
        break;
    }
    case C_LINE: {
        int x = dx, y = dy, major = nx, minor = reg16(42, 1023);
        int err = major / 2;
        for (int n = 0; n <= major; n++) {
            pset_m(&m, x, y, clr, op);
            if (arg & 1) {              /* MAJ: long side along y */
                y += diy;
                err -= minor;
                if (err < 0) { err += major; x += dix; }
            } else {
                x += dix;
                err -= minor;
                if (err < 0) { err += major; y += diy; }
            }
        }
        set_reg16(38, y & 1023);
        cmd_end();
        break;
    }
    case C_LMMV:
        for (int j = 0; j < ny; j++)
            for (int i = 0; i < nx; i++)
                pset_m(&m, dx + i * dix, dy + j * diy, clr, op);
        set_reg16(38, (dy + ny * diy) & 1023);
        cmd_end();
        break;
    case C_LMMM:
        for (int j = 0; j < ny; j++)
            for (int i = 0; i < nx; i++)
                pset_m(&m, dx + i * dix, dy + j * diy, pget_m(&m, sx + i * dix, sy + j * diy), op);
        set_reg16(34, (sy + ny * diy) & 1023);
        set_reg16(38, (dy + ny * diy) & 1023);
        cmd_end();
        break;
    case C_HMMV: {
        int p = ppb(), bx = dx / p, bn = (nx + p - 1) / p;
        for (int j = 0; j < ny; j++)
            for (int i = 0; i < bn; i++)
                *byte_at_m(&m, bx + i * dix, dy + j * diy) = clr;
        set_reg16(38, (dy + ny * diy) & 1023);
        cmd_end();
        break;
    }
    case C_HMMM: {
        int p = ppb(), sbx = sx / p, dbx = dx / p, bn = (nx + p - 1) / p;
        for (int j = 0; j < ny; j++)
            for (int i = 0; i < bn; i++)
                *byte_at_m(&m, dbx + i * dix, dy + j * diy) = *byte_at_m(&m, sbx + i * dix, sy + j * diy);
        set_reg16(34, (sy + ny * diy) & 1023);
        set_reg16(38, (dy + ny * diy) & 1023);
        cmd_end();
        break;
    }
    case C_YMMM: {
        int p = ppb(), bx = dx / p, n = dix > 0 ? m.bpl - bx : bx + 1;
        for (int j = 0; j < ny; j++)
            for (int i = 0; i < n; i++)
                *byte_at_m(&m, bx + i * dix, dy + j * diy) = *byte_at_m(&m, bx + i * dix, sy + j * diy);
        set_reg16(34, (sy + ny * diy) & 1023);
        set_reg16(38, (dy + ny * diy) & 1023);
        cmd_end();
        break;
    }
    case C_HMMC:
        c.nx = (c.nx + ppb() - 1) / ppb();
        c.x = c.dx / ppb();
        c.y = c.dy;
        c.i = c.j = 0;
        transfer_in(clr);               /* the first byte is in CLR */
        break;
    case C_LMMC:
        c.x = c.dx;
        c.y = c.dy;
        c.i = c.j = 0;
        transfer_in(clr);
        break;
    case C_LMCM:
        c.x = c.sx;
        c.y = c.sy;
        c.i = c.j = 0;
        stat[7] = pget(c.x, c.y);
        c.tr = true;
        break;
    default:
        cmd_end();
        break;
    }
}

/* ---- ports ------------------------------------------------------------------- */

static V99_RAM void write_reg(int r, uint8_t v)
{
    if (r >= 47)
        return;
    reg[r] = v;
    switch (r) {
    case 0: case 1: update_mode(); break;
    case 7: case 8: tab_dirty = true; break;
    case 14: addr = (addr & 0x3fff) | (uint32_t)(v & 7) << 14; break;
    case 16: pal_latched = false; break;
    case 44: if (c.ce && (c.cmd == C_HMMC || c.cmd == C_LMMC)) transfer_in(v); break;
    case 46: cmd_start(v); break;
    }
}

static inline void addr_inc(void)
{
    if (tms_mode())
        addr = (addr & 0x1c000) | ((addr + 1) & 0x3fff);
    else
        addr = (addr + 1) & 0x1ffff;
}

V99_RAM uint8_t v99_read_data(void)
{
    uint8_t v = readahead;
    readahead = vram[addr];
    addr_inc();
    latched = false;
    return v;
}

V99_RAM void v99_write_data(uint8_t v)
{
    vram[addr] = v;
    readahead = v;
    addr_inc();
    latched = false;
}

V99_RAM uint8_t v99_read_status(void)
{
    uint8_t v;
    latched = false;
    switch (reg[15] & 15) {
    case 0:
        v = stat[0];
        stat[0] &= 0x1f;
        break;
    case 1:
        v = stat[1];                    /* FH; ID 0: V9938 */
        stat[1] &= (uint8_t)~1;
        break;
    case 2:
        v = (uint8_t)((c.tr ? 0x80 : 0) | (vr ? 0x40 : 0) | (v99_hpos >= 180 ? 0x20 : 0) |
                      (stat[2] & 0x10) | 0x0c | (c.ce ? 1 : 0));
        break;
    case 4: v = 0xfe; break;
    case 6: v = 0xfc; break;
    case 7:
        v = stat[7];
        if (c.ce && c.cmd == C_LMCM) {
            transfer_step();
            if (c.ce)
                stat[7] = pget(c.x, c.y);
        }
        break;
    case 8: v = stat[8]; break;
    case 9: v = stat[9] | 0xfe; break;
    default: v = 0; break;
    }
    return v;
}

V99_RAM void v99_write_ctrl(uint8_t v)
{
    if (!latched) {
        latch = v;
        latched = true;
        return;
    }
    latched = false;
    if (v & 0x80) {
        write_reg(v & 0x3f, latch);
    } else {
        addr = (uint32_t)(reg[14] & 7) << 14 | (uint32_t)(v & 0x3f) << 8 | latch;
        if (!(v & 0x40)) {              /* read: fill the buffer */
            readahead = vram[addr];
            addr_inc();
        }
    }
}

V99_RAM void v99_write_palette(uint8_t v)
{
    if (!pal_latched) {
        pal_latch = v;
        pal_latched = true;
        return;
    }
    pal_latched = false;
    set_pal(reg[16], (pal_latch >> 4) & 7, v & 7, pal_latch & 7);
    reg[16] = (uint8_t)((reg[16] + 1) & 15);
}

V99_RAM void v99_write_indirect(uint8_t v)
{
    int r = reg[17] & 0x3f;
    if (r != 17)
        write_reg(r, v);
    if (!(reg[17] & 0x80))
        reg[17] = (uint8_t)((reg[17] & 0xc0) | ((r + 1) & 0x3f));
}

/* ---- frame ------------------------------------------------------------------- */

void v99_reset(void)
{
    memset(vram, 0, sizeof vram);
    memset(reg, 0, sizeof reg);
    memset(stat, 0, sizeof stat);
    memset(&c, 0, sizeof c);
    addr = 0;
    latched = pal_latched = vr = false;
    for (int i = 0; i < 16; i++)
        set_pal(i, default_pal[i][0], default_pal[i][1], default_pal[i][2]);
    for (int i = 0; i < 256; i++) {
        int b2 = i & 3;
        g7_565[i] = rgb((i >> 2) & 7, i >> 5, b2 << 1 | b2 >> 1);
    }
    for (int i = 0; i < 16; i++)
        spr7_565[i] = rgb(spr7_grb[i][1], spr7_grb[i][0], spr7_grb[i][2]);
    update_mode();
    frames = 0;
}

void v99_vblank(void)
{
    stat[0] |= 0x80;
    vr = true;
}

void v99_frame_start(void)
{
    vr = false;
    frames++;
}

/* ---- drawing ------------------------------------------------------------------- */

/* sprites of this line: colour + 1 per pixel (0: none), valid from spr_lo
 * to spr_hi (the columns the line's sprites cover) */
static uint8_t spr[V99_WIDTH + 64];
static int spr_lo, spr_hi;

static V99_RAM bool sprites(int yy)
{
    if ((reg[8] & 2) || mode == M_T1 || mode == M_T2)
        return false;
    bool m2 = mode >= M_G3 && mode <= M_G7;
    int size = (reg[1] & 2) ? 16 : 8;
    int mag = reg[1] & 1;
    int h = size << mag;
    uint32_t sat = m2 ? ((uint32_t)(reg[11] & 3) << 15 | (uint32_t)(reg[5] & 0xfc) << 7)
                      : ((uint32_t)(reg[11] & 3) << 15 | (uint32_t)(reg[5] & 0xff) << 7);
    uint32_t sct = sat - 0x200;
    uint32_t spg = (uint32_t)(reg[6] & 0x3f) << 11;
    int stop = m2 ? 0xd8 : 0xd0, max = m2 ? 8 : 4;
    int found[8], n = 0;

    for (int i = 0; i < 32; i++) {
        int sy = vram[(sat + i * 4) & 0x1ffff];
        if (sy == stop)
            break;
        int line = (yy - (sy + 1)) & 0xff;
        if (line >= h)
            continue;
        if (n == max) {
            if (!(stat[0] & 0x40))
                stat[0] = (uint8_t)((stat[0] & 0xa0) | 0x40 | i);
            break;
        }
        found[n++] = i;
    }
    if (!n)
        return false;
    /* the columns covered, to clear and to draw only those */
    int lo = V99_WIDTH, hi = 0;
    for (int k = 0; k < n; k++) {
        const uint8_t *a = &vram[(sat + found[k] * 4) & 0x1ffff];
        int line = ((yy - (a[0] + 1)) & 0xff) >> mag;
        int colb = m2 ? vram[(sct + found[k] * 16 + line) & 0x1ffff] : a[3];
        int x0 = a[1] - ((colb & 0x80) ? 32 : 0);
        int x1 = x0 + (size << mag);
        if (x0 < lo) lo = x0;
        if (x1 > hi) hi = x1;
    }
    if (lo < 0) lo = 0;
    if (hi > V99_WIDTH) hi = V99_WIDTH;
    if (lo >= hi)
        return false;
    spr_lo = lo;
    spr_hi = hi;
    memset(spr + lo, 0, (size_t)(hi - lo));
    static uint8_t owner[V99_WIDTH + 64];
    if (m2)
        memset(owner + lo, 0xff, (size_t)(hi - lo));
    int group = -1;                     /* mode 2: the last sprite without CC */

    for (int k = 0; k < n; k++) {
        int i = found[k];
        const uint8_t *a = &vram[(sat + i * 4) & 0x1ffff];
        int line = ((yy - (a[0] + 1)) & 0xff) >> mag;
        int colb = m2 ? vram[(sct + i * 16 + line) & 0x1ffff] : a[3];
        int ec = colb & 0x80, cc = m2 && (colb & 0x40), ic = m2 && (colb & 0x20);
        int col = colb & 15;
        if (m2 && !cc)
            group = k;
        if (cc && group < 0)
            continue;                   /* CC without a sprite to join */
        int x = a[1] - (ec ? 32 : 0) + 32;
        int pat = size == 16 ? (a[2] & 0xfc) : a[2];
        const uint8_t *p = &vram[(spg + pat * 8 + line) & 0x1ffff];
        uint32_t bits = (uint32_t)p[0] << 8;
        if (size == 16)
            bits |= p[16];
        for (int b = 0; b < size; b++) {
            if (!(bits & (0x8000u >> b)))
                continue;
            for (int m = 0; m <= mag; m++) {
                int px = x + (b << mag) + m;
                if (px < 32 || px >= V99_WIDTH + 32)
                    continue;
                uint8_t *s = &spr[px - 32];
                if (m2) {
                    uint8_t *o = &owner[px - 32];
                    if (cc) {
                        if (*o == group) *s = (uint8_t)(((*s - 1) | col) + 1);
                        else if (*o == 0xff) { *s = (uint8_t)(col + 1); *o = (uint8_t)group; }
                        continue;
                    }
                    if (*o != 0xff) {
                        if (!ic) stat[0] |= 0x20;
                        continue;
                    }
                    *o = (uint8_t)k;
                    if (col || (reg[8] & 0x20))
                        *s = (uint8_t)(col + 1);
                } else {
                    if (*s & 0x80) {
                        stat[0] |= 0x20;
                        continue;
                    }
                    *s = (uint8_t)(0x80 | (col ? col + 1 : 0));
                }
            }
        }
    }
    if (!m2)
        for (int x = lo; x < hi; x++)
            spr[x] &= 0x7f;
    return true;
}

/* Drawing tables, rebuilt only when the palette, R7 (backdrop), R8 (TP)
 * or the mode change: the colours with colour 0 as the backdrop unless TP
 * is set, and per VRAM byte its finished pixels -- graphics 4: two,
 * graphics 5: two (each the average of two dots), graphics 6: one (the
 * average of two dots, low half). Lines are written as 32-bit words. */
static uint16_t lp[16];
static uint32_t byte_px[256];
static int tab_mode = -1;

static inline uint16_t avg565(uint16_t p, uint16_t q)
{
    return p == q ? p : (uint16_t)(((p & 0xf7de) >> 1) + ((q & 0xf7de) >> 1));
}

static V99_RAM void build_tables(uint16_t back)
{
    tab_dirty = false;
    tab_mode = mode;
    for (int i = 0; i < 16; i++)
        lp[i] = (i || (reg[8] & 0x20)) ? pal565[i] : back;
    switch (mode) {
    case M_G4:
        for (int b = 0; b < 256; b++)
            byte_px[b] = lp[b >> 4] | (uint32_t)lp[b & 15] << 16;
        break;
    case M_G5:
        for (int b = 0; b < 256; b++)
            byte_px[b] = avg565(lp[(b >> 6) & 3], lp[(b >> 4) & 3]) |
                         (uint32_t)avg565(lp[(b >> 2) & 3], lp[b & 3]) << 16;
        break;
    case M_G6:
        for (int b = 0; b < 256; b++)
            byte_px[b] = avg565(lp[b >> 4], lp[b & 15]);
        break;
    default:
        break;
    }
}

V99_RAM void v99_line(int y, uint16_t *out)
{
    int yy = (y + reg[23]) & 0xff;
    if (yy == reg[19])
        stat[1] |= 1;                   /* FH: line interrupt */

    uint16_t back = mode == M_G7 ? g7_565[reg[7]] : pal565[reg[7] & 15];
    if (tab_dirty || tab_mode != mode)
        build_tables(back);
    uint32_t *o32 = (uint32_t *)out;
    if (!(reg[1] & 0x40)) {
        for (int x = 0; x < V99_WIDTH / 2; x++)
            o32[x] = back * 0x10001u;
        return;
    }

    switch (mode) {
    case M_T1: case M_T2: {
        uint8_t fg = reg[7] >> 4, bg = reg[7] & 15;
        uint16_t f565 = lp[fg], b565 = lp[bg];
        int row = yy >> 3, l = yy & 7;
        if (mode == M_T1) {
            uint32_t nt = (uint32_t)(reg[2] & 0x7f) << 10, pg = (uint32_t)(reg[4] & 0x3f) << 11;
            for (int x = 0; x < 8; x++) out[x] = out[V99_WIDTH - 8 + x] = back;
            uint16_t *o = out + 8;
            for (int cc = 0; cc < 40; cc++) {
                uint8_t bits = vram[(pg + vram[(nt + row * 40 + cc) & 0x1ffff] * 8 + l) & 0x1ffff];
                for (int i = 0; i < 6; i++)
                    *o++ = (bits & (0x80 >> i)) ? f565 : b565;
            }
        } else {
            /* 80 columns of 6 dots: 480 of 512, every pair averaged */
            uint32_t nt = (uint32_t)(reg[2] & 0x7c) << 10;
            uint32_t ct = (uint32_t)(reg[10] & 7) << 14 | (uint32_t)(reg[3] & 0xf8) << 6;
            bool blink_on = false;
            if (reg[13]) {
                uint32_t on = (uint32_t)(reg[13] >> 4) * 10, off = (uint32_t)(reg[13] & 15) * 10;
                if (on + off)
                    blink_on = frames % (on + off) < on;
            }
            uint16_t bf = lp[reg[12] >> 4], bb = lp[reg[12] & 15];
            uint32_t pg = (uint32_t)(reg[4] & 0x3f) << 11;
            static uint16_t wide[512];
            for (int x = 0; x < 16; x++) wide[x] = wide[496 + x] = back;
            uint16_t *o = wide + 16;
            for (int cc = 0; cc < 80; cc++) {
                int ch = row * 80 + cc;
                uint8_t bits = vram[(pg + vram[(nt + ch) & 0x1ffff] * 8 + l) & 0x1ffff];
                bool bl = blink_on && (vram[(ct + (ch >> 3)) & 0x1ffff] & (0x80 >> (ch & 7)));
                uint16_t a = bl ? bf : f565, b = bl ? bb : b565;
                for (int i = 0; i < 6; i++)
                    *o++ = (bits & (0x80 >> i)) ? a : b;
            }
            for (int x = 0; x < V99_WIDTH; x++)
                out[x] = avg565(wide[2 * x], wide[2 * x + 1]);
        }
        return;                         /* no sprites in text modes */
    }
    case M_MC: {
        uint32_t nt = (uint32_t)(reg[2] & 0x7f) << 10, pg = (uint32_t)(reg[4] & 0x3f) << 11;
        int sub = ((yy >> 3) & 3) * 2 + ((yy >> 2) & 1);
        for (int cc = 0; cc < 32; cc++) {
            uint8_t col = vram[(pg + vram[(nt + (yy >> 3) * 32 + cc) & 0x1ffff] * 8 + sub) & 0x1ffff];
            uint32_t a = lp[col >> 4] * 0x10001u, b = lp[col & 15] * 0x10001u;
            uint32_t *o = o32 + cc * 4;
            o[0] = o[1] = a;
            o[2] = o[3] = b;
        }
        break;
    }
    case M_G1: case M_G2: case M_G3: {
        uint32_t nt = (uint32_t)(reg[2] & 0x7f) << 10;
        uint32_t pbase, cbase;
        int pmask, cmask, section;
        if (mode == M_G1) {
            pbase = (uint32_t)(reg[4] & 0x3f) << 11;
            cbase = (uint32_t)(reg[10] & 7) << 14 | (uint32_t)reg[3] << 6;
            pmask = 0xff; cmask = 0; section = 0;
        } else {
            pbase = (uint32_t)(reg[4] & 0x3c) << 11;
            pmask = ((reg[4] & 3) << 8) | 0xff;
            cbase = (uint32_t)(reg[10] & 7) << 14 | (uint32_t)(reg[3] & 0x80) << 6;
            cmask = ((reg[3] & 0x7f) << 3) | 7;
            section = (yy >> 6) << 8;
        }
        int l = yy & 7;
        for (int cc = 0; cc < 32; cc++) {
            int t = vram[(nt + (yy >> 3) * 32 + cc) & 0x1ffff];
            uint8_t bits, col;
            if (mode == M_G1) {
                bits = vram[(pbase + t * 8 + l) & 0x1ffff];
                col = vram[(cbase + (t >> 3)) & 0x1ffff];
            } else {
                int tile = t + section;
                bits = vram[(pbase + (uint32_t)((tile & pmask) << 3) + l) & 0x1ffff];
                col = vram[(cbase + (uint32_t)((tile & cmask) << 3) + l) & 0x1ffff];
            }
            uint32_t f = lp[col >> 4], b = lp[col & 15];
            /* a pixel pair per two bits, the left one in the low half */
            const uint32_t pair[4] = { b | b << 16, b | f << 16, f | b << 16, f | f << 16 };
            uint32_t *o = o32 + cc * 4;
            o[0] = pair[bits >> 6];
            o[1] = pair[(bits >> 4) & 3];
            o[2] = pair[(bits >> 2) & 3];
            o[3] = pair[bits & 3];
        }
        break;
    }
    case M_G4: {
        const uint8_t *s = &vram[((uint32_t)(reg[2] & 0x60) << 10) + yy * 128];
        for (int x = 0; x < 128; x++)
            o32[x] = byte_px[s[x]];
        break;
    }
    case M_G5: {
        const uint8_t *s = &vram[((uint32_t)(reg[2] & 0x60) << 10) + yy * 128];
        for (int x = 0; x < 128; x++)
            o32[x] = byte_px[s[x]];
        break;
    }
    case M_G6: {
        const uint8_t *s = &vram[((uint32_t)(reg[2] & 0x20) << 11) + yy * 256];
        for (int x = 0; x < 128; x++)
            o32[x] = byte_px[s[2 * x]] | byte_px[s[2 * x + 1]] << 16;
        break;
    }
    case M_G7: {
        const uint8_t *s = &vram[((uint32_t)(reg[2] & 0x20) << 11) + yy * 256];
        for (int x = 0; x < 128; x++)
            o32[x] = g7_565[s[2 * x]] | (uint32_t)g7_565[s[2 * x + 1]] << 16;
        break;
    }
    }

    if (sprites(yy)) {
        const uint16_t *sp = mode == M_G7 ? spr7_565 : pal565;
        for (int x = spr_lo; x < spr_hi; x++)
            if (spr[x])
                out[x] = sp[(spr[x] - 1) & 15];
    }
}
