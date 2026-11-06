/* vectrex.c -- see vectrex.h */
#include <string.h>
#include "vectrex.h"
#include "vectrex_bios.h"
#include "ay8910.h"

#ifdef PBC_ON_DEVICE
#define VEC_RAM __attribute__((section(".time_critical.vectrex")))
#else
#define VEC_RAM
#endif

#ifndef AUDIO_RATE
#define AUDIO_RATE 22222
#endif

#define CLOCK      1500000
#define FRAME_CYC  (CLOCK / 50)

/* ---- memory ------------------------------------------------------------------- */

/* the cartridge and the System ROM, in RAM: from flash, every read that
 * misses the cache costs more than a whole emulated cycle */
static uint8_t cart[0x8000];
static uint8_t bios[0x2000];
static uint8_t ram[1024];

/* VIA 6522 */
static uint8_t orb, ora, ddrb, ddra, acr, pcr, ifr, ier, sr;
static uint8_t t1ll, t1lh, t2ll;
static uint16_t t1c, t2c;
static bool t1on, t1int, t2on, t2int;
static uint8_t t1pb7 = 0x80;            /* PB7 from timer 1: 0 while it runs */
static uint8_t src, srb = 8;            /* shift clock divider, bits shifted */
static bool srclk;
static uint8_t ca2 = 1, cb2h = 1, cb2s;

/* the analog part */
#define ALG_MAX_X 33000
#define ALG_MAX_Y 41000
static int32_t xsh = 0x80, ysh = 0x80, rsh = 0x80, zsh, jsh;
static int32_t jch[4] = { 0x80, 0x80, 0x80, 0x80 };
static uint8_t compare;
static int32_t dx, dy;                  /* integrator inputs */
static int32_t cur_x = ALG_MAX_X / 2, cur_y = ALG_MAX_Y / 2;
static bool vectoring;
static int32_t vx0, vy0, vx1, vy1, vdx, vdy;
static uint8_t vcolor;

/* the sound chip */
static ay8910_t psg;
static uint8_t snd_sel;
static uint8_t buttons_lo = 0xff;       /* AY port A: the buttons, active low */

/* the lines of this frame */
#define MAX_LINES 3000
typedef struct { int16_t x0, y0, x1, y1; uint8_t c; } vline_t;
/* Two lists: core 0 fills one while core 1 draws the other
 * (vectrex_picture). While core 1 is still busy, the next frame's lines join
 * those of the frame before. */
static vline_t lines[2][MAX_LINES];
static int nl[2];
static int lw;                          /* the list being filled */
static volatile bool handed;            /* the other one waits for core 1 */
static int hand_n;

static void add_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t c)
{
    if (nl[lw] >= MAX_LINES || c == 0)
        return;
    /* integrator units -> panel: 280 rows for ALG_MAX_Y, the same scale across */
    vline_t *l = &lines[lw][nl[lw]++];
    const int ox = (VEC_W - ALG_MAX_X * VEC_H / ALG_MAX_Y) / 2;
    l->x0 = (int16_t)(ox + x0 * VEC_H / ALG_MAX_Y);
    l->y0 = (int16_t)(y0 * VEC_H / ALG_MAX_Y);
    l->x1 = (int16_t)(ox + x1 * VEC_H / ALG_MAX_Y);
    l->y1 = (int16_t)(y1 * VEC_H / ALG_MAX_Y);
    l->c = c;
}

static VEC_RAM void alg_update(void)
{
    switch (orb & 0x06) {
    case 0x00:
        jsh = jch[0];
        if (!(orb & 0x01)) ysh = xsh;   /* the multiplexer passes the DAC on */
        break;
    case 0x02:
        jsh = jch[1];
        if (!(orb & 0x01)) rsh = xsh;
        break;
    case 0x04:
        jsh = jch[2];
        if (!(orb & 0x01)) zsh = xsh > 0x80 ? xsh - 0x80 : 0;
        break;
    case 0x06:
        jsh = jch[3];
        break;
    }
    compare = jsh > xsh ? 0x20 : 0;
    dx = xsh - rsh;
    dy = rsh - ysh;
}

static void snd_update(void)
{
    switch (orb & 0x18) {
    case 0x10:                          /* the chip takes data */
        if (snd_sel != 14) {
            ay_write_addr(&psg, snd_sel);
            ay_write_data(&psg, ora);
        }
        break;
    case 0x18:                          /* the chip takes an address */
        if ((ora & 0xf0) == 0)
            snd_sel = ora & 0x0f;
        break;
    }
}

static uint8_t psg_read(void)
{
    if (snd_sel == 14)
        return buttons_lo;
    ay_write_addr(&psg, snd_sel);
    return ay_read_data(&psg, NULL);
}

static void int_update(void)
{
    if (ifr & ier & 0x7f) ifr |= 0x80;
    else ifr &= 0x7f;
}

static __attribute__((noinline)) VEC_RAM uint8_t via_read(uint8_t r)
{
    switch (r & 15) {
    case 0x0:
        if (acr & 0x80) return (uint8_t)((orb & 0x5f) | t1pb7 | compare);
        return (uint8_t)((orb & 0xdf) | compare);
    case 0x1:
        if ((pcr & 0x0e) == 0x08) ca2 = 0;
        /* fall through */
    case 0xf:
        return (orb & 0x18) == 0x08 ? psg_read() : ora;
    case 0x2: return ddrb;
    case 0x3: return ddra;
    case 0x4: ifr &= ~0x40; int_update(); return (uint8_t)t1c;
    case 0x5: return (uint8_t)(t1c >> 8);
    case 0x6: return t1ll;
    case 0x7: return t1lh;
    case 0x8: ifr &= ~0x20; int_update(); return (uint8_t)t2c;
    case 0x9: return (uint8_t)(t2c >> 8);
    case 0xa: ifr &= ~0x04; int_update(); srb = 0; srclk = true; return sr;
    case 0xb: return acr;
    case 0xc: return pcr;
    case 0xd: return ifr;
    default:  return (uint8_t)(ier | 0x80);
    }
}

static __attribute__((noinline)) VEC_RAM void via_write(uint8_t r, uint8_t v)
{
    switch (r & 15) {
    case 0x0:
        orb = v;
        snd_update();
        alg_update();
        if ((pcr & 0xe0) == 0x80) cb2h = 0;
        break;
    case 0x1:
        if ((pcr & 0x0e) == 0x08) ca2 = 0;
        /* fall through */
    case 0xf:
        ora = v;
        snd_update();
        xsh = v ^ 0x80;
        alg_update();
        break;
    case 0x2: ddrb = v; break;
    case 0x3: ddra = v; break;
    case 0x4: t1ll = v; break;
    case 0x5:
        t1lh = v;
        t1c = (uint16_t)(t1lh << 8 | t1ll);
        ifr &= ~0x40;
        int_update();
        t1on = t1int = true;
        t1pb7 = 0;
        break;
    case 0x6: t1ll = v; break;
    case 0x7: t1lh = v; break;
    case 0x8: t2ll = v; break;
    case 0x9:
        t2c = (uint16_t)(v << 8 | t2ll);
        ifr &= ~0x20;
        int_update();
        t2on = t2int = true;
        break;
    case 0xa:
        sr = v;
        ifr &= ~0x04;
        int_update();
        srb = 0;
        srclk = true;
        break;
    case 0xb: acr = v; break;
    case 0xc:
        pcr = v;
        ca2 = (pcr & 0x0e) == 0x0c ? 0 : 1;
        cb2h = (pcr & 0xe0) == 0xc0 ? 0 : 1;
        break;
    case 0xd: ifr &= (uint8_t)~(v & 0x7f); int_update(); break;
    case 0xe:
        if (v & 0x80) ier |= v & 0x7f;
        else ier &= (uint8_t)~(v & 0x7f);
        int_update();
        break;
    }
}

/* not inlined: they are everywhere, and the VIA behind them is big */
static __attribute__((noinline)) VEC_RAM uint8_t rd(uint16_t a)
{
    if ((a & 0xe000) == 0xe000)
        return bios[a & 0x1fff];
    if ((a & 0xe000) == 0xc000) {
        if (a & 0x800) return ram[a & 0x3ff];
        if (a & 0x1000) return via_read((uint8_t)a);
        return 0xff;
    }
    if (a < 0x8000)
        return cart[a];
    return 0xff;
}

static __attribute__((noinline)) VEC_RAM void wr(uint16_t a, uint8_t v)
{
    if ((a & 0xe000) == 0xc000) {
        if (a & 0x800) ram[a & 0x3ff] = v;
        if (a & 0x1000) via_write((uint8_t)a, v);
    }
}

/* ---- one cycle of the VIA and the beam -------------------------------------------- */

static VEC_RAM void cycle(void)
{
    /* timer 1 */
    if (t1on) {
        t1c--;
        if (t1c == 0xffff) {
            if (acr & 0x40) {           /* free running */
                ifr |= 0x40;
                int_update();
                t1pb7 = (uint8_t)(0x80 - t1pb7);
                t1c = (uint16_t)(t1lh << 8 | t1ll);
            } else if (t1int) {         /* one shot */
                ifr |= 0x40;
                int_update();
                t1pb7 = 0x80;
                t1int = false;
            }
        }
    }
    /* timer 2 (one shot; pulse counting is not wired) */
    if (t2on && !(acr & 0x20)) {
        t2c--;
        if (t2c == 0xffff && t2int) {
            ifr |= 0x20;
            int_update();
            t2int = false;
        }
    }
    /* the shift register, clocked by timer 2's low byte or the system */
    bool t2shift = false;
    src--;
    if (src == 0xff) {
        src = t2ll;
        t2shift = srclk;
        srclk = !srclk;
    }
    if (srb < 8) {
        switch (acr & 0x1c) {
        case 0x04: if (t2shift) { sr <<= 1; srb++; } break;
        case 0x08: sr <<= 1; srb++; break;
        case 0x10: if (t2shift) { cb2s = (sr >> 7) & 1; sr = (uint8_t)(sr << 1 | cb2s); } break;
        case 0x14: if (t2shift) { cb2s = (sr >> 7) & 1; sr = (uint8_t)(sr << 1 | cb2s); srb++; } break;
        case 0x18: cb2s = (sr >> 7) & 1; sr = (uint8_t)(sr << 1 | cb2s); srb++; break;
        }
        if (srb == 8) {
            ifr |= 0x04;
            int_update();
        }
    }
    if ((pcr & 0x0e) == 0x0a) ca2 = 1;  /* pulse modes: back to 1 */
    if ((pcr & 0xe0) == 0xa0) cb2h = 1;

    /* the beam */
    uint8_t blank = (acr & 0x10) ? cb2s : cb2h;
    int32_t sx, sy;
    if (ca2 == 0) {                     /* ZERO: back to the middle */
        sx = ALG_MAX_X / 2 - cur_x;
        sy = ALG_MAX_Y / 2 - cur_y;
    } else {
        uint8_t ramp = (acr & 0x80) ? t1pb7 : (orb & 0x80);
        sx = ramp ? 0 : dx;
        sy = ramp ? 0 : dy;
    }
    bool inside = cur_x >= 0 && cur_x < ALG_MAX_X && cur_y >= 0 && cur_y < ALG_MAX_Y;
    if (!vectoring) {
        if (blank && inside) {
            vectoring = true;
            vx0 = vx1 = cur_x;
            vy0 = vy1 = cur_y;
            vdx = sx;
            vdy = sy;
            vcolor = (uint8_t)zsh;
        }
    } else if (!blank) {
        vectoring = false;
        add_line(vx0, vy0, vx1, vy1, vcolor);
    } else if (sx != vdx || sy != vdy || (uint8_t)zsh != vcolor) {
        add_line(vx0, vy0, vx1, vy1, vcolor);
        if (inside) {
            vx0 = vx1 = cur_x;
            vy0 = vy1 = cur_y;
            vdx = sx;
            vdy = sy;
            vcolor = (uint8_t)zsh;
        } else {
            vectoring = false;
        }
    }
    cur_x += sx;
    cur_y += sy;
    if (vectoring && cur_x >= 0 && cur_x < ALG_MAX_X && cur_y >= 0 && cur_y < ALG_MAX_Y) {
        vx1 = cur_x;
        vy1 = cur_y;
    }
}

/* n cycles. Mostly nothing happens in them but counting and moving: no
 * timer runs out, the shift register rests, the beam goes straight on the
 * way it went. Those are taken in one step, the same as cycle by cycle;
 * anything else goes cycle by cycle until it is quiet again. */
static VEC_RAM void cycles(int n)
{
    while (n > 0) {
        if (srb < 8 ||
            (t1on && n > t1c && ((acr & 0x40) || t1int)) ||
            (t2on && !(acr & 0x20) && t2int && n > t2c))
            goto one;
        if ((pcr & 0x0e) == 0x0a) ca2 = 1;
        if ((pcr & 0xe0) == 0xa0) cb2h = 1;
        uint8_t blank = (acr & 0x10) ? cb2s : cb2h;
        int32_t sx, sy;
        if (ca2 == 0) {
            sx = ALG_MAX_X / 2 - cur_x;
            sy = ALG_MAX_Y / 2 - cur_y;
            if (sx || sy)
                goto one;               /* the jump to the middle: one cycle */
        } else {
            uint8_t ramp = (acr & 0x80) ? t1pb7 : (orb & 0x80);
            sx = ramp ? 0 : dx;
            sy = ramp ? 0 : dy;
        }
        int32_t ex = cur_x + sx * n, ey = cur_y + sy * n;
        if (vectoring) {
            /* the line goes on; inside at both ends, it is inside all along */
            if (!blank || sx != vdx || sy != vdy || (uint8_t)zsh != vcolor ||
                cur_x < 0 || cur_x >= ALG_MAX_X || cur_y < 0 || cur_y >= ALG_MAX_Y ||
                ex < 0 || ex >= ALG_MAX_X || ey < 0 || ey >= ALG_MAX_Y)
                goto one;
            vx1 = ex;
            vy1 = ey;
        } else if (blank) {
            goto one;                   /* lit outside: it may come in */
        }
        cur_x = ex;
        cur_y = ey;
        if (t1on) t1c = (uint16_t)(t1c - n);
        if (t2on && !(acr & 0x20)) t2c = (uint16_t)(t2c - n);
        int k = n;                      /* the shift clock divider */
        while (k > src) {
            k -= src + 1;
            src = t2ll;
            srclk = !srclk;
        }
        src = (uint8_t)(src - k);
        return;
    one:
        cycle();
        n--;
    }
}

/* ---- the 6809 ------------------------------------------------------------------------ */

enum { CC_C = 0x01, CC_V = 0x02, CC_Z = 0x04, CC_N = 0x08, CC_I = 0x10, CC_H = 0x20, CC_F = 0x40, CC_E = 0x80 };

static uint8_t A, B, DP, CC;
static uint16_t X, Y, U, S, PC;
static bool waiting;                    /* CWAI or SYNC */
static bool pushed;                     /* ... CWAI: the state is on the stack already */
static int extra;                       /* cycles of the indexed mode */

#define D_REG ((uint16_t)(A << 8 | B))
static inline void set_d(uint16_t v) { A = (uint8_t)(v >> 8); B = (uint8_t)v; }

static inline uint16_t rd16(uint16_t a) { return (uint16_t)(rd(a) << 8 | rd((uint16_t)(a + 1))); }
static inline void wr16(uint16_t a, uint16_t v) { wr(a, (uint8_t)(v >> 8)); wr((uint16_t)(a + 1), (uint8_t)v); }
static inline uint8_t imm8(void) { return rd(PC++); }
static inline uint16_t imm16(void) { uint16_t v = rd16(PC); PC += 2; return v; }

static inline void push8(uint16_t *sp, uint8_t v) { wr(--*sp, v); }
static inline void push16(uint16_t *sp, uint16_t v) { wr(--*sp, (uint8_t)v); wr(--*sp, (uint8_t)(v >> 8)); }
static inline uint8_t pull8(uint16_t *sp) { return rd((*sp)++); }
static inline uint16_t pull16(uint16_t *sp) { uint16_t v = (uint16_t)(rd(*sp) << 8 | rd((uint16_t)(*sp + 1))); *sp += 2; return v; }

static inline void nz8(uint8_t v)
{
    CC = (uint8_t)((CC & ~(CC_N | CC_Z)) | (v & 0x80 ? CC_N : 0) | (v ? 0 : CC_Z));
}
static inline void nz16(uint16_t v)
{
    CC = (uint8_t)((CC & ~(CC_N | CC_Z)) | (v & 0x8000 ? CC_N : 0) | (v ? 0 : CC_Z));
}

static uint16_t *idx_reg(uint8_t pb)
{
    switch ((pb >> 5) & 3) {
    case 0:  return &X;
    case 1:  return &Y;
    case 2:  return &U;
    default: return &S;
    }
}

static VEC_RAM uint16_t ea_indexed(void)
{
    uint8_t pb = imm8();
    uint16_t *r = idx_reg(pb);
    uint16_t ea;
    if (!(pb & 0x80)) {                 /* 5-bit offset */
        extra = 1;
        return (uint16_t)(*r + ((pb & 0x10) ? (int)(pb & 0x1f) - 32 : (pb & 0x0f)));
    }
    bool ind = pb & 0x10;
    switch (pb & 0x0f) {
    case 0x0: ea = *r; *r += 1; extra = 2; break;
    case 0x1: ea = *r; *r += 2; extra = 3; break;
    case 0x2: *r -= 1; ea = *r; extra = 2; break;
    case 0x3: *r -= 2; ea = *r; extra = 3; break;
    case 0x4: ea = *r; extra = 0; break;
    case 0x5: ea = (uint16_t)(*r + (int8_t)B); extra = 1; break;
    case 0x6: ea = (uint16_t)(*r + (int8_t)A); extra = 1; break;
    case 0x8: ea = (uint16_t)(*r + (int8_t)imm8()); extra = 1; break;
    case 0x9: ea = (uint16_t)(*r + imm16()); extra = 4; break;
    case 0xb: ea = (uint16_t)(*r + D_REG); extra = 4; break;
    case 0xc: { int8_t o = (int8_t)imm8(); ea = (uint16_t)(PC + o); extra = 1; break; }
    case 0xd: { uint16_t o = imm16(); ea = (uint16_t)(PC + o); extra = 5; break; }
    case 0xf: ea = imm16(); extra = 5; break;   /* [n16] */
    default:  ea = *r; extra = 0; break;        /* undefined */
    }
    if (ind) {
        ea = rd16(ea);
        extra += 3;
    }
    return ea;
}

static inline uint16_t ea_direct(void) { return (uint16_t)(DP << 8 | imm8()); }

/* ALU */
static inline uint8_t op_add8(uint8_t a, uint8_t b, int c)
{
    unsigned r = (unsigned)a + b + c;
    CC = (uint8_t)(CC & ~(CC_H | CC_N | CC_Z | CC_V | CC_C));
    if ((a ^ b ^ r) & 0x10) CC |= CC_H;
    if (r & 0x100) CC |= CC_C;
    if (~(a ^ b) & (a ^ r) & 0x80) CC |= CC_V;
    nz8((uint8_t)r);
    return (uint8_t)r;
}
static inline uint8_t op_sub8(uint8_t a, uint8_t b, int c)
{
    unsigned r = (unsigned)a - b - c;
    CC = (uint8_t)(CC & ~(CC_N | CC_Z | CC_V | CC_C));
    if (r & 0x100) CC |= CC_C;
    if ((a ^ b) & (a ^ r) & 0x80) CC |= CC_V;
    nz8((uint8_t)r);
    return (uint8_t)r;
}
static inline uint16_t op_add16(uint16_t a, uint16_t b)
{
    uint32_t r = (uint32_t)a + b;
    CC = (uint8_t)(CC & ~(CC_N | CC_Z | CC_V | CC_C));
    if (r & 0x10000) CC |= CC_C;
    if (~(a ^ b) & (a ^ r) & 0x8000) CC |= CC_V;
    nz16((uint16_t)r);
    return (uint16_t)r;
}
static inline uint16_t op_sub16(uint16_t a, uint16_t b)
{
    uint32_t r = (uint32_t)a - b;
    CC = (uint8_t)(CC & ~(CC_N | CC_Z | CC_V | CC_C));
    if (r & 0x10000) CC |= CC_C;
    if ((a ^ b) & (a ^ r) & 0x8000) CC |= CC_V;
    nz16((uint16_t)r);
    return (uint16_t)r;
}
static inline uint8_t op_logic(uint8_t v) { CC &= (uint8_t)~CC_V; nz8(v); return v; }

/* read-modify-write group (NEG .. CLR): op 0..15 */
static VEC_RAM uint8_t rmw(uint8_t op, uint8_t v)
{
    unsigned r;
    switch (op) {
    case 0x0: r = op_sub8(0, v, 0); return (uint8_t)r;                     /* NEG */
    case 0x3: v = (uint8_t)~v; CC = (uint8_t)((CC & ~CC_V) | CC_C); nz8(v); return v;   /* COM */
    case 0x4: CC = (uint8_t)((CC & ~CC_C) | (v & 1)); v >>= 1; nz8(v); return v;      /* LSR */
    case 0x6: r = (unsigned)(v >> 1) | ((CC & CC_C) ? 0x80 : 0);                      /* ROR */
              CC = (uint8_t)((CC & ~CC_C) | (v & 1)); nz8((uint8_t)r); return (uint8_t)r;
    case 0x7: CC = (uint8_t)((CC & ~CC_C) | (v & 1)); v = (uint8_t)((v >> 1) | (v & 0x80)); nz8(v); return v;  /* ASR */
    case 0x8: r = (unsigned)v << 1;                                                   /* ASL */
              CC = (uint8_t)(CC & ~(CC_C | CC_V));
              if (v & 0x80) CC |= CC_C;
              if ((v ^ (v << 1)) & 0x80) CC |= CC_V;
              nz8((uint8_t)r); return (uint8_t)r;
    case 0x9: r = (unsigned)(v << 1) | (CC & CC_C);                                   /* ROL */
              CC = (uint8_t)(CC & ~(CC_C | CC_V));
              if (v & 0x80) CC |= CC_C;
              if ((v ^ (v << 1)) & 0x80) CC |= CC_V;
              nz8((uint8_t)r); return (uint8_t)r;
    case 0xa: CC = (uint8_t)((CC & ~CC_V) | (v == 0x80 ? CC_V : 0)); v--; nz8(v); return v;   /* DEC */
    case 0xc: CC = (uint8_t)((CC & ~CC_V) | (v == 0x7f ? CC_V : 0)); v++; nz8(v); return v;   /* INC */
    case 0xd: CC &= (uint8_t)~CC_V; nz8(v); return v;                                 /* TST */
    case 0xf: CC = (uint8_t)((CC & ~(CC_N | CC_V | CC_C)) | CC_Z); return 0;          /* CLR */
    }
    return v;
}

static bool branch(uint8_t op)
{
    bool n = CC & CC_N, z = CC & CC_Z, v = CC & CC_V, c = CC & CC_C;
    switch (op & 0x0f) {
    case 0x0: return true;
    case 0x1: return false;
    case 0x2: return !c && !z;
    case 0x3: return c || z;
    case 0x4: return !c;
    case 0x5: return c;
    case 0x6: return !z;
    case 0x7: return z;
    case 0x8: return !v;
    case 0x9: return v;
    case 0xa: return !n;
    case 0xb: return n;
    case 0xc: return n == v;
    case 0xd: return n != v;
    case 0xe: return !z && n == v;
    default:  return z || n != v;
    }
}

static uint16_t reg_get(int r)
{
    switch (r) {
    case 0x0: return D_REG;
    case 0x1: return X;
    case 0x2: return Y;
    case 0x3: return U;
    case 0x4: return S;
    case 0x5: return PC;
    case 0x8: return (uint16_t)(0xff00 | A);
    case 0x9: return (uint16_t)(0xff00 | B);
    case 0xa: return (uint16_t)(0xff00 | CC);
    case 0xb: return (uint16_t)(0xff00 | DP);
    default:  return 0xffff;
    }
}

static void reg_set(int r, uint16_t v)
{
    switch (r) {
    case 0x0: set_d(v); break;
    case 0x1: X = v; break;
    case 0x2: Y = v; break;
    case 0x3: U = v; break;
    case 0x4: S = v; break;
    case 0x5: PC = v; break;
    case 0x8: A = (uint8_t)v; break;
    case 0x9: B = (uint8_t)v; break;
    case 0xa: CC = (uint8_t)v; break;
    case 0xb: DP = (uint8_t)v; break;
    }
}

/* PSHS/PSHU and PULS/PULU: `sp` the stack, `other` the other stack register */
static int psh(uint16_t *sp, uint16_t other, uint8_t m)
{
    int n = 0;
    if (m & 0x80) { push16(sp, PC); n += 2; }
    if (m & 0x40) { push16(sp, other); n += 2; }
    if (m & 0x20) { push16(sp, Y); n += 2; }
    if (m & 0x10) { push16(sp, X); n += 2; }
    if (m & 0x08) { push8(sp, DP); n++; }
    if (m & 0x04) { push8(sp, B); n++; }
    if (m & 0x02) { push8(sp, A); n++; }
    if (m & 0x01) { push8(sp, CC); n++; }
    return n;
}

static int pul(uint16_t *sp, uint16_t *other, uint8_t m)
{
    int n = 0;
    if (m & 0x01) { CC = pull8(sp); n++; }
    if (m & 0x02) { A = pull8(sp); n++; }
    if (m & 0x04) { B = pull8(sp); n++; }
    if (m & 0x08) { DP = pull8(sp); n++; }
    if (m & 0x10) { X = pull16(sp); n += 2; }
    if (m & 0x20) { Y = pull16(sp); n += 2; }
    if (m & 0x40) { *other = pull16(sp); n += 2; }
    if (m & 0x80) { PC = pull16(sp); n += 2; }
    return n;
}

static __attribute__((noinline)) void push_all(void)
{
    push16(&S, PC);
    push16(&S, U);
    push16(&S, Y);
    push16(&S, X);
    push8(&S, DP);
    push8(&S, B);
    push8(&S, A);
    push8(&S, CC);
}

/* base cycles of page 0 (indexed: + extra) */
static const uint8_t cyc0[256] = {
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,3,6,   0,0,2,4,2,2,5,9,2,2,3,2,3,2,8,6,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,   4,4,4,4,5,5,5,5,2,5,3,6,20,11,2,19,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,   2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,3,6,   7,7,7,7,7,7,7,7,7,7,7,7,7,7,4,7,
    2,2,2,4,2,2,2,2,2,2,2,2,4,7,3,3,   4,4,4,6,4,4,4,4,4,4,4,4,6,7,5,5,
    4,4,4,6,4,4,4,4,4,4,4,4,6,7,5,5,   5,5,5,7,5,5,5,5,5,5,5,5,7,8,6,6,
    2,2,2,4,2,2,2,2,2,2,2,2,3,3,3,3,   4,4,4,6,4,4,4,4,4,4,4,4,5,5,5,5,
    4,4,4,6,4,4,4,4,4,4,4,4,5,5,5,5,   5,5,5,7,5,5,5,5,5,5,5,5,6,6,6,6,
};

/* the operand's address for 0x80..0xFF (and 0x00..0x7F RMW): mode by bits 4..5 */
static inline uint16_t ea_for(uint8_t op)
{
    switch (op & 0x30) {
    case 0x10: return ea_direct();
    case 0x20: return ea_indexed();
    default:   return imm16();           /* 0x30: extended */
    }
}

/* 16-bit loads/stores/compares of pages 2 and 3, `op` the low opcode byte */
static VEC_RAM int page23(uint8_t pre, uint8_t op)
{
    int c;
    uint16_t v = 0, ea = 0;
    extra = 0;
    if (pre == 0x10 && op >= 0x21 && op <= 0x2f) {     /* long branches */
        uint16_t o = imm16();
        if (branch(op)) { PC += o; return 6; }
        return 5;
    }
    if (op == 0x3f) {                   /* SWI2 / SWI3 */
        CC |= CC_E;
        push_all();
        PC = rd16(pre == 0x10 ? 0xfff4 : 0xfff2);
        return 20;
    }
    int mode = op & 0x30;               /* 0x00 imm, 0x10 dir, 0x20 idx, 0x30 ext */
    int m = mode >> 4;
    uint8_t lo = op & 0x0f;
    bool hi = op >= 0xc0;
    if (op < 0x80)
        return 2;                       /* undefined */
    static const uint8_t cmp_c[4] = { 5, 7, 7, 8 };     /* CMPD/Y/U/S imm/dir/idx/ext */
    static const uint8_t ld_c[4] = { 4, 6, 6, 7 };      /* LDY/LDS/STY/STS */
    if (lo == 0x03 && !hi) {            /* CMPD / CMPU */
        v = mode == 0x00 ? imm16() : rd16(ea_for(op));
        op_sub16(pre == 0x10 ? D_REG : U, v);
        c = cmp_c[m];
    } else if (lo == 0x0c && !hi) {     /* CMPY / CMPS */
        v = mode == 0x00 ? imm16() : rd16(ea_for(op));
        op_sub16(pre == 0x10 ? Y : S, v);
        c = cmp_c[m];
    } else if (lo == 0x0e && pre == 0x10) {             /* LDY / LDS */
        v = mode == 0x00 ? imm16() : rd16(ea_for(op));
        if (hi) S = v; else Y = v;
        CC &= (uint8_t)~CC_V;
        nz16(v);
        c = ld_c[m];
    } else if (lo == 0x0f && pre == 0x10 && mode != 0x00) {   /* STY / STS */
        ea = ea_for(op);
        v = hi ? S : Y;
        wr16(ea, v);
        CC &= (uint8_t)~CC_V;
        nz16(v);
        c = ld_c[m];
    } else {
        return 2;
    }
    return c + (mode == 0x20 ? extra : 0);
}

static VEC_RAM int step(void)
{
    if (waiting)
        return 1;
    uint8_t op = imm8();
    int c = cyc0[op];
    extra = 0;
    uint16_t ea, v16;
    uint8_t v;

    if (op < 0x10 || (op >= 0x60 && op < 0x80)) {      /* RMW memory, JMP */
        ea = op < 0x10 ? ea_direct() : (op < 0x70 ? ea_indexed() : imm16());
        uint8_t k = op & 0x0f;
        if (k == 0x0e) {
            PC = ea;
        } else if (k == 0x0f) {
            wr(ea, rmw(k, 0));
        } else if (k == 0x0d) {
            rmw(k, rd(ea));
        } else {
            wr(ea, rmw(k, rd(ea)));
        }
        return c + extra;
    }
    if (op >= 0x40 && op < 0x60) {      /* RMW on A or B */
        if (op < 0x50) A = rmw(op & 0x0f, A);
        else B = rmw(op & 0x0f, B);
        return c;
    }
    if (op >= 0x20 && op < 0x30) {      /* short branches */
        int8_t o = (int8_t)imm8();
        if (branch(op)) PC = (uint16_t)(PC + o);
        return c;
    }
    if (op >= 0x80) {
        bool bside = op >= 0xc0;
        uint8_t k = op & 0x0f;
        int mode = op & 0x30;
        uint8_t *R = bside ? &B : &A;
        /* 16-bit and special ones first */
        switch (op) {
        case 0x8d: { int8_t o = (int8_t)imm8(); push16(&S, PC); PC = (uint16_t)(PC + o); return c; }   /* BSR */
        case 0x9d: case 0xad: case 0xbd:                                    /* JSR */
            ea = ea_for(op);
            push16(&S, PC);
            PC = ea;
            return c + extra;
        }
        if (k == 0x03 || k == 0x0c || k == 0x0e || (bside && k == 0x0c)) {
            /* SUBD/ADDD, CMPX/LDD, LDX/LDU */
            v16 = mode == 0x00 ? imm16() : rd16(ea_for(op));
            if (k == 0x03) {
                if (bside) set_d(op_add16(D_REG, v16));
                else set_d(op_sub16(D_REG, v16));
            } else if (k == 0x0c) {
                if (bside) { set_d(v16); CC &= (uint8_t)~CC_V; nz16(v16); }   /* LDD */
                else op_sub16(X, v16);                                       /* CMPX */
            } else {
                if (bside) U = v16; else X = v16;
                CC &= (uint8_t)~CC_V;
                nz16(v16);
            }
            return c + extra;
        }
        if ((k == 0x0d || k == 0x0f) && mode != 0x00) {   /* STD, STU, STX */
            ea = ea_for(op);
            v16 = k == 0x0d ? D_REG : bside ? U : X;
            if (k == 0x0d && !bside) return c + extra;     /* (JSR handled above) */
            wr16(ea, v16);
            CC &= (uint8_t)~CC_V;
            nz16(v16);
            return c + extra;
        }
        if (k == 0x07) {                /* STA / STB */
            if (mode == 0x00) return 2;
            ea = ea_for(op);
            wr(ea, *R);
            CC &= (uint8_t)~CC_V;
            nz8(*R);
            return c + extra;
        }
        v = mode == 0x00 ? imm8() : rd(ea_for(op));
        switch (k) {
        case 0x0: *R = op_sub8(*R, v, 0); break;                    /* SUB */
        case 0x1: op_sub8(*R, v, 0); break;                         /* CMP */
        case 0x2: *R = op_sub8(*R, v, CC & CC_C); break;            /* SBC */
        case 0x4: *R = op_logic((uint8_t)(*R & v)); break;          /* AND */
        case 0x5: op_logic((uint8_t)(*R & v)); break;               /* BIT */
        case 0x6: *R = op_logic(v); break;                          /* LD */
        case 0x8: *R = op_logic((uint8_t)(*R ^ v)); break;          /* EOR */
        case 0x9: *R = op_add8(*R, v, CC & CC_C); break;            /* ADC */
        case 0xa: *R = op_logic((uint8_t)(*R | v)); break;          /* OR */
        case 0xb: *R = op_add8(*R, v, 0); break;                    /* ADD */
        }
        return c + extra;
    }

    switch (op) {
    case 0x10: case 0x11: {
        uint8_t o2 = imm8();
        return page23(op, o2);
    }
    case 0x12: return c;                                           /* NOP */
    case 0x13: waiting = true; pushed = false; return c;           /* SYNC */
    case 0x16: { uint16_t o = imm16(); PC += o; return c; }        /* LBRA */
    case 0x17: { uint16_t o = imm16(); push16(&S, PC); PC += o; return c; }   /* LBSR */
    case 0x19: {                                                   /* DAA */
        unsigned cf = 0, lo = A & 0x0f, hi = A >> 4;
        if (lo > 9 || (CC & CC_H)) cf |= 0x06;
        if (hi > 9 || (CC & CC_C) || (hi > 8 && lo > 9)) cf |= 0x60;
        unsigned r = (unsigned)A + cf;
        CC = (uint8_t)(CC & ~(CC_V));
        if (r & 0x100) CC |= CC_C;
        A = (uint8_t)r;
        nz8(A);
        return c;
    }
    case 0x1a: CC |= imm8(); return c;                             /* ORCC */
    case 0x1c: CC &= imm8(); return c;                             /* ANDCC */
    case 0x1d: A = (B & 0x80) ? 0xff : 0; nz16(D_REG); return c;   /* SEX */
    case 0x1e: {                                                   /* EXG */
        uint8_t pb = imm8();
        uint16_t a = reg_get(pb >> 4), b = reg_get(pb & 15);
        reg_set(pb >> 4, b);
        reg_set(pb & 15, a);
        return c;
    }
    case 0x1f: { uint8_t pb = imm8(); reg_set(pb & 15, reg_get(pb >> 4)); return c; }   /* TFR */
    case 0x30: X = ea_indexed(); CC = (uint8_t)((CC & ~CC_Z) | (X ? 0 : CC_Z)); return c + extra;   /* LEAX */
    case 0x31: Y = ea_indexed(); CC = (uint8_t)((CC & ~CC_Z) | (Y ? 0 : CC_Z)); return c + extra;   /* LEAY */
    case 0x32: S = ea_indexed(); return c + extra;                 /* LEAS */
    case 0x33: U = ea_indexed(); return c + extra;                 /* LEAU */
    case 0x34: { uint8_t m = imm8(); return c + psh(&S, U, m); }   /* PSHS */
    case 0x35: { uint8_t m = imm8(); return c + pul(&S, &U, m); }  /* PULS */
    case 0x36: { uint8_t m = imm8(); return c + psh(&U, S, m); }   /* PSHU */
    case 0x37: { uint8_t m = imm8(); return c + pul(&U, &S, m); }  /* PULU */
    case 0x39: PC = pull16(&S); return c;                          /* RTS */
    case 0x3a: X = (uint16_t)(X + B); return c;                    /* ABX */
    case 0x3b:                                                     /* RTI */
        CC = pull8(&S);
        if (CC & CC_E) {
            A = pull8(&S);
            B = pull8(&S);
            DP = pull8(&S);
            X = pull16(&S);
            Y = pull16(&S);
            U = pull16(&S);
            c += 9;
        }
        PC = pull16(&S);
        return c;
    case 0x3c:                                                     /* CWAI */
        CC &= imm8();
        CC |= CC_E;
        push_all();
        waiting = true;
        pushed = true;
        return c;
    case 0x3d: {                                                   /* MUL */
        uint16_t r = (uint16_t)(A * B);
        set_d(r);
        CC = (uint8_t)((CC & ~(CC_Z | CC_C)) | (r ? 0 : CC_Z) | ((r & 0x80) ? CC_C : 0));
        return c;
    }
    case 0x3f:                                                     /* SWI */
        CC |= CC_E;
        push_all();
        CC |= CC_I | CC_F;
        PC = rd16(0xfffa);
        return c;
    }
    return 2;                           /* undefined: as a NOP */
}

/* the VIA's interrupt on the IRQ line */
static VEC_RAM int irq_check(void)
{
    if ((ifr & 0x80) && !(CC & CC_I)) {
        if (!(waiting && pushed)) {     /* CWAI has pushed already */
            CC |= CC_E;
            push_all();
        }
        waiting = pushed = false;
        CC |= CC_I;
        PC = rd16(0xfff8);
        return 19;
    }
    if (waiting && (ifr & 0x80))
        waiting = false;                /* SYNC with the interrupt masked */
    return 0;
}

/* ---- the machine ------------------------------------------------------------------- */

__attribute__((optimize("Os"))) bool vectrex_insert(const uint8_t *rom, uint32_t size)
{
    if (!rom || size < 16 || size > 0x8000)
        return false;
    memset(cart, 0xff, sizeof cart);
    memcpy(cart, rom, size);
    memcpy(bios, vectrex_bios, sizeof bios);
    return true;
}

__attribute__((optimize("Os"))) void vectrex_reset(void)
{
    memset(ram, 0, sizeof ram);
    orb = ora = ddrb = ddra = acr = pcr = ifr = ier = sr = 0;
    t1ll = t1lh = t2ll = 0;
    t1c = t2c = 0;
    t1on = t1int = t2on = t2int = false;
    t1pb7 = 0x80;
    src = 0;
    srb = 8;
    srclk = false;
    ca2 = cb2h = 1;
    cb2s = 0;
    xsh = ysh = rsh = 0x80;
    zsh = 0;
    jch[0] = jch[1] = jch[2] = jch[3] = 0x80;
    cur_x = ALG_MAX_X / 2;
    cur_y = ALG_MAX_Y / 2;
    vectoring = false;
    alg_update();
    snd_sel = 0;
    ay_init(&psg, CLOCK, AUDIO_RATE);
    ay_reset(&psg);
    A = B = X = Y = U = S = 0;
    DP = 0;
    CC = CC_I | CC_F;
    waiting = false;
    PC = rd16(0xfffe);
    nl[0] = nl[1] = 0;
    lw = 0;
    handed = false;
}

void vectrex_input(uint8_t buttons, int x, int y)
{
    buttons_lo = (uint8_t)~(buttons & 0x0f);
    jch[0] = x + 0x80 < 0 ? 0 : x + 0x80 > 255 ? 255 : x + 0x80;
    jch[1] = y + 0x80 < 0 ? 0 : y + 0x80 > 255 ? 255 : y + 0x80;
}

uint8_t *vectrex_ram(void) { return ram; }

/* ---- the picture --------------------------------------------------------------------- */

static uint8_t __attribute__((aligned(4))) pic[VEC_H][VEC_W];

static inline void plot(int x, int y, unsigned c)
{
    if ((unsigned)x < VEC_W && (unsigned)y < VEC_H) {
        unsigned v = pic[y][x] + c;
        pic[y][x] = (uint8_t)(v > 255 ? 255 : v);
    }
}

static VEC_RAM void draw_line(const vline_t *l)
{
    /* brightness 0..127 -> 0..255, a little brighter than linear */
    unsigned c = l->c >= 0x70 ? 255 : (unsigned)l->c * 2 + 32;
    int x0 = l->x0, y0 = l->y0, x1 = l->x1, y1 = l->y1;
    int ddx = x1 > x0 ? x1 - x0 : x0 - x1, ddy = y1 > y0 ? y1 - y0 : y0 - y1;
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = ddx - ddy;
    unsigned g = c / 3;                 /* a soft glow around the beam, like the tube */
    for (;;) {
        plot(x0, y0, c);
        plot(x0 + 1, y0, g);
        plot(x0 - 1, y0, g);
        plot(x0, y0 + 1, g);
        plot(x0, y0 - 1, g);
        if (x0 == x1 && y0 == y1)
            break;
        int e2 = 2 * err;
        if (e2 > -ddy) { err -= ddy; x0 += sx; }
        if (e2 < ddx) { err += ddx; y0 += sy; }
    }
}

VEC_RAM bool vectrex_picture(void)
{
    if (!handed)
        return false;
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    /* the phosphor: what was there fades by half, the new lines on top */
    uint32_t *w = (uint32_t *)pic;
    for (int i = 0; i < VEC_H * VEC_W / 4; i++)
        w[i] = (w[i] >> 1) & 0x7f7f7f7fu;
    const vline_t *l = lines[lw ^ 1];
    for (int i = 0; i < hand_n; i++)
        draw_line(&l[i]);
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    handed = false;
    return true;
}

const uint8_t *vectrex_row(int y) { return pic[y]; }

VEC_RAM void vectrex_frame(int16_t *audio, int samples)
{
    int left = FRAME_CYC;
    while (left > 0) {
        int c = irq_check();
        c += step();
        cycles(c);
        left -= c;
    }
    /* the lines to core 1, if it is done with the last ones; else they
     * wait for the next frame's */
    if (!handed) {
        hand_n = nl[lw];
        lw ^= 1;
        nl[lw] = 0;
        __atomic_thread_fence(__ATOMIC_SEQ_CST);
        handed = true;
    }

    /* sound: the chip's three channels, the DC taken out */
    static int32_t mix[1024];
    static int32_t dc;
    if (samples > 1024) samples = 1024;
    memset(mix, 0, (size_t)samples * sizeof mix[0]);
    ay_render(&psg, mix, samples);
    for (int i = 0; i < samples; i++) {
        dc += (mix[i] - dc) >> 8;
        int32_t s = (mix[i] - dc) * 2;
        audio[i] = (int16_t)(s > 32767 ? 32767 : s < -32768 ? -32768 : s);
    }
}
