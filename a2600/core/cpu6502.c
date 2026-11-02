/* cpu6502.c -- see cpu6502.h
 *
 * Every bus cycle of the real chip is a cpu_read() or cpu_write() here,
 * the dummy ones included (the read of the next byte by one-byte
 * instructions, the read at the not yet corrected address when an index
 * crosses a page, the double write of read-modify-write instructions), so
 * the machine sees the right number of cycles and the accesses in the
 * right order. The Atari 2600's video chip depends on both.
 */
#include "cpu6502.h"

#ifdef PBC_ON_DEVICE
#define CPU_RAM __attribute__((section(".time_critical.cpu6502")))
#else
#define CPU_RAM
#endif

cpu6502_t cpu;

enum { FC = 0x01, FZ = 0x02, FI = 0x04, FD = 0x08, FB = 0x10, FU = 0x20, FV = 0x40, FN = 0x80 };

#define RD(a)       cpu_read((uint16_t)(a))
#define WR(a, v)    cpu_write((uint16_t)(a), (uint8_t)(v))
#ifndef CPU_FETCH                       /* a2600.c gives a faster one */
#define CPU_FETCH(a) cpu_read(a)
#endif
#define FETCH()     CPU_FETCH((uint16_t)cpu.pc++)
#define SETNZ(v)    (cpu.p = (uint8_t)((cpu.p & ~(FN | FZ)) | ((v) & FN) | ((v) ? 0 : FZ)))

static inline void push(uint8_t v) { WR(0x100 | cpu.s, v); cpu.s--; }
static inline uint8_t pull(void) { cpu.s++; return RD(0x100 | cpu.s); }

/* ---- addressing: the effective address, with the cycles of each mode ---- */

static inline uint16_t a_zp(void)  { return FETCH(); }
static inline uint16_t a_zpx(void) { uint8_t z = FETCH(); RD(z); return (uint8_t)(z + cpu.x); }
static inline uint16_t a_zpy(void) { uint8_t z = FETCH(); RD(z); return (uint8_t)(z + cpu.y); }
static inline uint16_t a_abs(void) { uint16_t lo = FETCH(); return lo | (uint16_t)(FETCH() << 8); }

/* indexed: `always` for writes and read-modify-write (the dummy read
 * happens whether or not the page changes) */
static inline uint16_t a_absi(uint8_t i, int always)
{
    uint16_t lo = FETCH(), hi = (uint16_t)(FETCH() << 8);
    uint16_t a = (uint16_t)(hi + lo + i);
    if (always || ((lo + i) & 0x100))
        RD(hi | ((lo + i) & 0xff));
    return a;
}

static inline uint16_t a_indx(void)
{
    uint8_t z = FETCH();
    RD(z);
    z = (uint8_t)(z + cpu.x);
    uint16_t lo = RD(z);
    return lo | (uint16_t)(RD((uint8_t)(z + 1)) << 8);
}

static inline uint16_t a_indy(int always)
{
    uint8_t z = FETCH();
    uint16_t lo = RD(z), hi = (uint16_t)(RD((uint8_t)(z + 1)) << 8);
    uint16_t a = (uint16_t)(hi + lo + cpu.y);
    if (always || ((lo + cpu.y) & 0x100))
        RD(hi | ((lo + cpu.y) & 0xff));
    return a;
}

/* ---- arithmetic ---------------------------------------------------------- */

static inline void adc(uint8_t m)
{
    unsigned c = cpu.p & FC;
    if (cpu.p & FD) {
        unsigned lo = (cpu.a & 15) + (m & 15) + c;
        unsigned hi = (cpu.a >> 4) + (m >> 4);
        if (lo > 9) { lo += 6; hi++; }
        uint8_t bin = (uint8_t)(cpu.a + m + c);
        cpu.p &= (uint8_t)~(FC | FZ | FN | FV);
        if (!bin) cpu.p |= FZ;
        if (hi & 8) cpu.p |= FN;
        if (~(cpu.a ^ m) & (cpu.a ^ (hi << 4)) & 0x80) cpu.p |= FV;
        if (hi > 9) hi += 6;
        if (hi > 15) cpu.p |= FC;
        cpu.a = (uint8_t)((hi << 4) | (lo & 15));
    } else {
        unsigned r = cpu.a + m + c;
        cpu.p &= (uint8_t)~(FC | FV);
        if (r > 0xff) cpu.p |= FC;
        if (~(cpu.a ^ m) & (cpu.a ^ r) & 0x80) cpu.p |= FV;
        cpu.a = (uint8_t)r;
        SETNZ(cpu.a);
    }
}

static inline void sbc(uint8_t m)
{
    unsigned c = cpu.p & FC;
    unsigned r = cpu.a - m - (1 - c);
    uint8_t v = (uint8_t)(((cpu.a ^ m) & (cpu.a ^ r) & 0x80) ? FV : 0);
    if (cpu.p & FD) {
        int lo = (cpu.a & 15) - (m & 15) - (int)(1 - c);
        int hi = (cpu.a >> 4) - (m >> 4);
        if (lo < 0) { lo -= 6; hi--; }
        if (hi < 0) hi -= 6;
        cpu.p &= (uint8_t)~(FC | FV);
        cpu.p |= v;
        if (r < 0x100) cpu.p |= FC;
        SETNZ((uint8_t)r);
        cpu.a = (uint8_t)((hi << 4) | (lo & 15));
    } else {
        cpu.p &= (uint8_t)~(FC | FV);
        cpu.p |= v;
        if (r < 0x100) cpu.p |= FC;
        cpu.a = (uint8_t)r;
        SETNZ(cpu.a);
    }
}

static inline void cmp(uint8_t r, uint8_t m)
{
    unsigned t = r - m;
    cpu.p = (uint8_t)((cpu.p & ~FC) | (r >= m ? FC : 0));
    SETNZ((uint8_t)t);
}

static inline uint8_t asl(uint8_t v) { cpu.p = (uint8_t)((cpu.p & ~FC) | (v >> 7)); v <<= 1; SETNZ(v); return v; }
static inline uint8_t lsr(uint8_t v) { cpu.p = (uint8_t)((cpu.p & ~FC) | (v & 1)); v >>= 1; SETNZ(v); return v; }
static inline uint8_t rol(uint8_t v) { uint8_t c = cpu.p & FC; cpu.p = (uint8_t)((cpu.p & ~FC) | (v >> 7)); v = (uint8_t)((v << 1) | c); SETNZ(v); return v; }
static inline uint8_t ror(uint8_t v) { uint8_t c = cpu.p & FC; cpu.p = (uint8_t)((cpu.p & ~FC) | (v & 1)); v = (uint8_t)((v >> 1) | (c << 7)); SETNZ(v); return v; }

static inline void bit(uint8_t m)
{
    cpu.p = (uint8_t)((cpu.p & ~(FN | FV | FZ)) | (m & (FN | FV)) | ((cpu.a & m) ? 0 : FZ));
}

static inline void branch(int cond)
{
    int8_t off = (int8_t)FETCH();
    if (!cond)
        return;
    RD(cpu.pc);
    uint16_t t = (uint16_t)(cpu.pc + off);
    if ((t ^ cpu.pc) & 0xff00)
        RD((cpu.pc & 0xff00) | (t & 0xff));
    cpu.pc = t;
}

/* read-modify-write: read, write the old value back, write the new one */
#define RMW(addr, op) do { uint16_t a_ = (addr); uint8_t v_ = RD(a_); WR(a_, v_); v_ = op(v_); WR(a_, v_); } while (0)
/* the undocumented combinations of a shift with an ALU operation */
#define RMW2(addr, op, then) do { uint16_t a_ = (addr); uint8_t v_ = RD(a_); WR(a_, v_); v_ = op(v_); WR(a_, v_); then; } while (0)

static inline uint8_t dec(uint8_t v) { v--; SETNZ(v); return v; }
static inline uint8_t inc(uint8_t v) { v++; SETNZ(v); return v; }

void cpu_reset(void)
{
    cpu.a = cpu.x = cpu.y = 0;
    cpu.s = 0xfd;
    cpu.p = FU | FI;
    cpu.jammed = 0;
    uint16_t lo = RD(0xfffc);
    cpu.pc = (uint16_t)(lo | (RD(0xfffd) << 8));
}

CPU_RAM void cpu_step(void)
{
    if (cpu.jammed) {
        RD(cpu.pc);
        return;
    }
    uint8_t op = FETCH();
    uint8_t m;
    switch (op) {
    /* ---- loads ---- */
    case 0xa9: cpu.a = FETCH(); SETNZ(cpu.a); break;
    case 0xa5: cpu.a = RD(a_zp()); SETNZ(cpu.a); break;
    case 0xb5: cpu.a = RD(a_zpx()); SETNZ(cpu.a); break;
    case 0xad: cpu.a = RD(a_abs()); SETNZ(cpu.a); break;
    case 0xbd: cpu.a = RD(a_absi(cpu.x, 0)); SETNZ(cpu.a); break;
    case 0xb9: cpu.a = RD(a_absi(cpu.y, 0)); SETNZ(cpu.a); break;
    case 0xa1: cpu.a = RD(a_indx()); SETNZ(cpu.a); break;
    case 0xb1: cpu.a = RD(a_indy(0)); SETNZ(cpu.a); break;
    case 0xa2: cpu.x = FETCH(); SETNZ(cpu.x); break;
    case 0xa6: cpu.x = RD(a_zp()); SETNZ(cpu.x); break;
    case 0xb6: cpu.x = RD(a_zpy()); SETNZ(cpu.x); break;
    case 0xae: cpu.x = RD(a_abs()); SETNZ(cpu.x); break;
    case 0xbe: cpu.x = RD(a_absi(cpu.y, 0)); SETNZ(cpu.x); break;
    case 0xa0: cpu.y = FETCH(); SETNZ(cpu.y); break;
    case 0xa4: cpu.y = RD(a_zp()); SETNZ(cpu.y); break;
    case 0xb4: cpu.y = RD(a_zpx()); SETNZ(cpu.y); break;
    case 0xac: cpu.y = RD(a_abs()); SETNZ(cpu.y); break;
    case 0xbc: cpu.y = RD(a_absi(cpu.x, 0)); SETNZ(cpu.y); break;
    /* ---- stores ---- */
    case 0x85: WR(a_zp(), cpu.a); break;
    case 0x95: WR(a_zpx(), cpu.a); break;
    case 0x8d: WR(a_abs(), cpu.a); break;
    case 0x9d: WR(a_absi(cpu.x, 1), cpu.a); break;
    case 0x99: WR(a_absi(cpu.y, 1), cpu.a); break;
    case 0x81: WR(a_indx(), cpu.a); break;
    case 0x91: WR(a_indy(1), cpu.a); break;
    case 0x86: WR(a_zp(), cpu.x); break;
    case 0x96: WR(a_zpy(), cpu.x); break;
    case 0x8e: WR(a_abs(), cpu.x); break;
    case 0x84: WR(a_zp(), cpu.y); break;
    case 0x94: WR(a_zpx(), cpu.y); break;
    case 0x8c: WR(a_abs(), cpu.y); break;
    /* ---- transfers ---- */
    case 0xaa: RD(cpu.pc); cpu.x = cpu.a; SETNZ(cpu.x); break;
    case 0xa8: RD(cpu.pc); cpu.y = cpu.a; SETNZ(cpu.y); break;
    case 0x8a: RD(cpu.pc); cpu.a = cpu.x; SETNZ(cpu.a); break;
    case 0x98: RD(cpu.pc); cpu.a = cpu.y; SETNZ(cpu.a); break;
    case 0xba: RD(cpu.pc); cpu.x = cpu.s; SETNZ(cpu.x); break;
    case 0x9a: RD(cpu.pc); cpu.s = cpu.x; break;
    /* ---- stack ---- */
    case 0x48: RD(cpu.pc); push(cpu.a); break;
    case 0x08: RD(cpu.pc); push(cpu.p | FB | FU); break;
    case 0x68: RD(cpu.pc); RD(0x100 | cpu.s); cpu.a = pull(); SETNZ(cpu.a); break;
    case 0x28: RD(cpu.pc); RD(0x100 | cpu.s); cpu.p = (uint8_t)((pull() & ~FB) | FU); break;
    /* ---- ALU ---- */
#define ALU(base, OPX) \
    case base + 0x09: m = FETCH(); OPX; break; \
    case base + 0x05: m = RD(a_zp()); OPX; break; \
    case base + 0x15: m = RD(a_zpx()); OPX; break; \
    case base + 0x0d: m = RD(a_abs()); OPX; break; \
    case base + 0x1d: m = RD(a_absi(cpu.x, 0)); OPX; break; \
    case base + 0x19: m = RD(a_absi(cpu.y, 0)); OPX; break; \
    case base + 0x01: m = RD(a_indx()); OPX; break; \
    case base + 0x11: m = RD(a_indy(0)); OPX; break;
    ALU(0x00, cpu.a |= m; SETNZ(cpu.a))
    ALU(0x20, cpu.a &= m; SETNZ(cpu.a))
    ALU(0x40, cpu.a ^= m; SETNZ(cpu.a))
    ALU(0x60, adc(m))
    ALU(0xc0, cmp(cpu.a, m))
    ALU(0xe0, sbc(m))
#undef ALU
    case 0xeb: sbc(FETCH()); break;                     /* undocumented SBC # */
    case 0xe0: cmp(cpu.x, FETCH()); break;
    case 0xe4: cmp(cpu.x, RD(a_zp())); break;
    case 0xec: cmp(cpu.x, RD(a_abs())); break;
    case 0xc0: cmp(cpu.y, FETCH()); break;
    case 0xc4: cmp(cpu.y, RD(a_zp())); break;
    case 0xcc: cmp(cpu.y, RD(a_abs())); break;
    case 0x24: bit(RD(a_zp())); break;
    case 0x2c: bit(RD(a_abs())); break;
    /* ---- increments ---- */
    case 0xe8: RD(cpu.pc); cpu.x++; SETNZ(cpu.x); break;
    case 0xc8: RD(cpu.pc); cpu.y++; SETNZ(cpu.y); break;
    case 0xca: RD(cpu.pc); cpu.x--; SETNZ(cpu.x); break;
    case 0x88: RD(cpu.pc); cpu.y--; SETNZ(cpu.y); break;
    case 0xe6: RMW(a_zp(), inc); break;
    case 0xf6: RMW(a_zpx(), inc); break;
    case 0xee: RMW(a_abs(), inc); break;
    case 0xfe: RMW(a_absi(cpu.x, 1), inc); break;
    case 0xc6: RMW(a_zp(), dec); break;
    case 0xd6: RMW(a_zpx(), dec); break;
    case 0xce: RMW(a_abs(), dec); break;
    case 0xde: RMW(a_absi(cpu.x, 1), dec); break;
    /* ---- shifts ---- */
#define SHIFT(base, fn) \
    case base + 0x0a: RD(cpu.pc); cpu.a = fn(cpu.a); break; \
    case base + 0x06: RMW(a_zp(), fn); break; \
    case base + 0x16: RMW(a_zpx(), fn); break; \
    case base + 0x0e: RMW(a_abs(), fn); break; \
    case base + 0x1e: RMW(a_absi(cpu.x, 1), fn); break;
    SHIFT(0x00, asl)
    SHIFT(0x20, rol)
    SHIFT(0x40, lsr)
    SHIFT(0x60, ror)
#undef SHIFT
    /* ---- jumps ---- */
    case 0x4c: cpu.pc = a_abs(); break;
    case 0x6c: {
        uint16_t p = a_abs();
        uint16_t lo = RD(p);
        cpu.pc = (uint16_t)(lo | (RD((p & 0xff00) | ((p + 1) & 0xff)) << 8));   /* page wrap */
        break;
    }
    case 0x20: {
        uint16_t lo = FETCH();
        RD(0x100 | cpu.s);
        push((uint8_t)(cpu.pc >> 8));
        push((uint8_t)cpu.pc);
        uint16_t hi = FETCH();
        cpu.pc = (uint16_t)(lo | (hi << 8));
        break;
    }
    case 0x60: {
        RD(cpu.pc);
        RD(0x100 | cpu.s);
        uint16_t lo = pull();
        cpu.pc = (uint16_t)(lo | (pull() << 8));
        RD(cpu.pc);
        cpu.pc++;
        break;
    }
    case 0x40: {
        RD(cpu.pc);
        RD(0x100 | cpu.s);
        cpu.p = (uint8_t)((pull() & ~FB) | FU);
        uint16_t lo = pull();
        cpu.pc = (uint16_t)(lo | (pull() << 8));
        break;
    }
    case 0x00: {                                          /* BRK */
        FETCH();
        push((uint8_t)(cpu.pc >> 8));
        push((uint8_t)cpu.pc);
        push(cpu.p | FB | FU);
        cpu.p |= FI;
        uint16_t lo = RD(0xfffe);
        cpu.pc = (uint16_t)(lo | (RD(0xffff) << 8));
        break;
    }
    /* ---- branches ---- */
    case 0x10: branch(!(cpu.p & FN)); break;
    case 0x30: branch(cpu.p & FN); break;
    case 0x50: branch(!(cpu.p & FV)); break;
    case 0x70: branch(cpu.p & FV); break;
    case 0x90: branch(!(cpu.p & FC)); break;
    case 0xb0: branch(cpu.p & FC); break;
    case 0xd0: branch(!(cpu.p & FZ)); break;
    case 0xf0: branch(cpu.p & FZ); break;
    /* ---- flags ---- */
    case 0x18: RD(cpu.pc); cpu.p &= (uint8_t)~FC; break;
    case 0x38: RD(cpu.pc); cpu.p |= FC; break;
    case 0x58: RD(cpu.pc); cpu.p &= (uint8_t)~FI; break;
    case 0x78: RD(cpu.pc); cpu.p |= FI; break;
    case 0xb8: RD(cpu.pc); cpu.p &= (uint8_t)~FV; break;
    case 0xd8: RD(cpu.pc); cpu.p &= (uint8_t)~FD; break;
    case 0xf8: RD(cpu.pc); cpu.p |= FD; break;
    case 0xea: RD(cpu.pc); break;

    /* ---- undocumented ---- */
    /* NOPs of all lengths */
    case 0x1a: case 0x3a: case 0x5a: case 0x7a: case 0xda: case 0xfa: RD(cpu.pc); break;
    case 0x80: case 0x82: case 0x89: case 0xc2: case 0xe2: FETCH(); break;
    case 0x04: case 0x44: case 0x64: RD(a_zp()); break;
    case 0x14: case 0x34: case 0x54: case 0x74: case 0xd4: case 0xf4: RD(a_zpx()); break;
    case 0x0c: RD(a_abs()); break;
    case 0x1c: case 0x3c: case 0x5c: case 0x7c: case 0xdc: case 0xfc: RD(a_absi(cpu.x, 0)); break;
    /* LAX, SAX */
    case 0xa7: cpu.a = cpu.x = RD(a_zp()); SETNZ(cpu.a); break;
    case 0xb7: cpu.a = cpu.x = RD(a_zpy()); SETNZ(cpu.a); break;
    case 0xaf: cpu.a = cpu.x = RD(a_abs()); SETNZ(cpu.a); break;
    case 0xbf: cpu.a = cpu.x = RD(a_absi(cpu.y, 0)); SETNZ(cpu.a); break;
    case 0xa3: cpu.a = cpu.x = RD(a_indx()); SETNZ(cpu.a); break;
    case 0xb3: cpu.a = cpu.x = RD(a_indy(0)); SETNZ(cpu.a); break;
    case 0x87: WR(a_zp(), cpu.a & cpu.x); break;
    case 0x97: WR(a_zpy(), cpu.a & cpu.x); break;
    case 0x8f: WR(a_abs(), cpu.a & cpu.x); break;
    case 0x83: WR(a_indx(), cpu.a & cpu.x); break;
    /* the read-modify-write + ALU combinations */
#define COMBO(base, fn, then) \
    case base + 0x07: RMW2(a_zp(), fn, then); break; \
    case base + 0x17: RMW2(a_zpx(), fn, then); break; \
    case base + 0x0f: RMW2(a_abs(), fn, then); break; \
    case base + 0x1f: RMW2(a_absi(cpu.x, 1), fn, then); break; \
    case base + 0x1b: RMW2(a_absi(cpu.y, 1), fn, then); break; \
    case base + 0x03: RMW2(a_indx(), fn, then); break; \
    case base + 0x13: RMW2(a_indy(1), fn, then); break;
    COMBO(0x00, asl, cpu.a |= v_; SETNZ(cpu.a))           /* SLO */
    COMBO(0x20, rol, cpu.a &= v_; SETNZ(cpu.a))           /* RLA */
    COMBO(0x40, lsr, cpu.a ^= v_; SETNZ(cpu.a))           /* SRE */
    COMBO(0x60, ror, adc(v_))                             /* RRA */
    COMBO(0xc0, (uint8_t)-1 +, cmp(cpu.a, v_))            /* DCP */
    COMBO(0xe0, (uint8_t)1 +, sbc(v_))                    /* ISB */
#undef COMBO
    case 0x0b: case 0x2b:                                 /* ANC */
        cpu.a &= FETCH(); SETNZ(cpu.a);
        cpu.p = (uint8_t)((cpu.p & ~FC) | (cpu.a >> 7));
        break;
    case 0x4b: cpu.a &= FETCH(); cpu.a = lsr(cpu.a); break;          /* ALR */
    case 0x6b: {                                          /* ARR */
        cpu.a &= FETCH();
        uint8_t c = cpu.p & FC;
        cpu.a = (uint8_t)((cpu.a >> 1) | (c << 7));
        SETNZ(cpu.a);
        cpu.p = (uint8_t)((cpu.p & ~(FC | FV)) | ((cpu.a >> 6) & 1) |
                          (((cpu.a >> 6) ^ (cpu.a >> 5)) & 1 ? FV : 0));
        break;
    }
    case 0xcb: {                                          /* SBX */
        unsigned t = (unsigned)(cpu.a & cpu.x) - FETCH();
        cpu.p = (uint8_t)((cpu.p & ~FC) | (t < 0x100 ? FC : 0));
        cpu.x = (uint8_t)t;
        SETNZ(cpu.x);
        break;
    }
    /* KIL: the CPU stops */
    case 0x02: case 0x12: case 0x22: case 0x32: case 0x42: case 0x52:
    case 0x62: case 0x72: case 0x92: case 0xb2: case 0xd2: case 0xf2:
        cpu.jammed = 1;
        break;
    default:                                              /* the rest: as a NOP */
        RD(cpu.pc);
        break;
    }
}
