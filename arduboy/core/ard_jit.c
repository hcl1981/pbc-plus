/*
 * ard_jit.c -- the AVR program translated to Thumb-2, included by arduboy.c
 * (ARD_JIT). Part of PBC Arduboy, GPL-3.0-or-later.
 *
 * The interpreter in exec() costs some 10 clock cycles per AVR instruction
 * before the instruction does anything (fetch the slot, jump, test the
 * cycle budget). Translated, MOV is two ARM instructions.
 *
 * - Lazy: the start of every block (a branch target, the instruction after
 *   a jump, call or skip, the interrupt vectors) gets the slot op O_JITX.
 *   When the interpreter reaches it, the block is translated (until the
 *   next block start or a jump) and the slot becomes O_JIT, which enters
 *   the ARM code. Data in the flash never runs and is never translated.
 * - Exact as the interpreter: a block runs only if the whole of it fits the
 *   cycle budget (the interpreter would run all its instructions: the
 *   budget left >= the cycles of all but the last); else the interpreter
 *   does it instruction by instruction. I/O goes through helpers that
 *   synchronise like SYNC_OUT/SYNC_IN; if that lowers the limit, the block
 *   is left right after the instruction. Interrupts are taken outside, as
 *   ever.
 * - SREG lives in a register, exact at every block boundary except for
 *   flags no instruction reads before overwriting them (liveness over the
 *   whole program, with RET/IJMP/ICALL counting as "all read").
 * - Blocks branch to each other directly once both are translated (a
 *   branch to a block not yet translated goes out through a stub that is
 *   patched when that block appears).
 *
 * Registers in translated code:
 *   r4 data space (mem)  r5 cycle budget  r6 SREG  r7 AVR SP
 *   r8 jit_ctx_t         r9 code buffer   r10 code[]  r11 flash
 *   r0-r3, r12 scratch
 * r5 between blocks is the interpreter's `left`; inside a block it is less
 * by adj = (cycles of the block but its last instruction) - (cycles before
 * the current instruction).
 */

/* ---- the shared state, at offsets the generated code knows ---------------------- */

typedef struct {
    uint8_t tab_add[16];                /*  0: NZCV -> AVR C Z N V S after an add */
    uint8_t tab_sub[16];                /* 16: ... after a subtract */
    int32_t left;                       /* 32 */
    uint32_t base;                      /* 36 */
    uint8_t f;                          /* 40 */
    uint8_t stop;                       /* 41: leave exec() after this (STOP) */
    uint16_t sp;                        /* 42 */
    uint8_t *dm;                        /* 44 */
    uint16_t *jb;                       /* 48 */
    slot_t *code;                       /* 52 */
    const uint8_t *flash;               /* 56 */
    uint32_t (*h_rd)(uint32_t a, uint32_t d, int32_t adj);      /* 60 */
    void (*h_wr)(uint32_t a, uint32_t v, int32_t adj);          /* 64 */
    void (*h_bit)(uint32_t a, uint32_t op, int32_t adj);        /* 68 */
    const bool *irq_pending;            /* 72 */
    uint8_t *irq_hold;                  /* 76 */
    bool *sleeping;                     /* 80 */
    const uint8_t *pin_keep;            /* 84 */
    void (*h_push2)(uint32_t v, uint32_t unused, int32_t adj);  /* 88 */
    uint32_t (*h_pop2)(uint32_t unused, uint32_t u2, int32_t adj);  /* 92 */
    void (*h_push1)(uint32_t d, uint32_t unused, int32_t adj);  /* 96 */
    void (*h_pop1)(uint32_t d, uint32_t unused, int32_t adj);   /* 100 */
    uint32_t tmp;                       /* 104: a value back from jit_slow */
    uint32_t (*slow)(uint32_t a, const uint16_t *rec);          /* 108 */
} jit_ctx_t;

enum {
    C_LEFT = 32, C_BASE = 36, C_F = 40, C_STOP = 41, C_SP = 42, C_DM = 44, C_JB = 48,
    C_CODE = 52, C_FLASH = 56, C_HRD = 60, C_HWR = 64, C_HBIT = 68, C_IRQP = 72,
    C_IRQH = 76, C_SLEEP = 80, C_PINK = 84, C_PUSH2 = 88, C_POP2 = 92, C_PUSH1 = 96,
    C_POP1 = 100, C_TMP = 104, C_SLOW = 108
};
_Static_assert(offsetof(jit_ctx_t, left) == C_LEFT, "jit_ctx_t");
_Static_assert(offsetof(jit_ctx_t, sp) == C_SP, "jit_ctx_t");
_Static_assert(offsetof(jit_ctx_t, flash) == C_FLASH, "jit_ctx_t");
_Static_assert(offsetof(jit_ctx_t, pin_keep) == C_PINK, "jit_ctx_t");
_Static_assert(offsetof(jit_ctx_t, h_pop1) == C_POP1, "jit_ctx_t");
_Static_assert(offsetof(jit_ctx_t, slow) == C_SLOW, "jit_ctx_t");

static jit_ctx_t jc;
static uint16_t *jbuf, *jp, *jend;      /* the code buffer, where the next code goes, its end */
static bool jfull;
static uint8_t *jlive;                  /* per instruction: the flags read after it */
/* shared code at the start of the buffer (offsets in halfwords) */
static uint32_t jit_exit_off, jit_exit_stop_off, jit_ind_off, jit_tramp_off, jit_link_off;
static uint32_t jit_t_off[4];           /* flags: add, adc, sub, sbc */
static uint32_t jit_push2_off, jit_pop2_off, jit_push1_off, jit_pop1_off;
static bool jit_on;
#ifdef ARD_JIT_STATS                    /* host tests: blocks, failed, entries, entries refused */
uint32_t jit_stat[4];
#define JIT_STAT(k) (jit_stat[k]++)
#else
#define JIT_STAT(k) ((void)0)
#endif

/* ---- helpers the generated code calls (I/O and the rare cases) --------------------- */

static ARD_RAM void jit_sync_out(int32_t adj)
{
    sreg = jc.f;
    sp = jc.sp;
    cyc = jc.base - (uint32_t)(jc.left + adj) - 1;
}

static ARD_RAM void jit_sync_in(int32_t adj)
{
    jc.f = sreg;
    jc.sp = sp;
    jc.base = run_limit;
    jc.left = (int32_t)(run_limit - cyc) - 1 - adj;
}

/* a read of the data space; into R[d] unless d >= 32; returns the byte */
static ARD_RAM uint32_t jit_h_rd(uint32_t a, uint32_t d, int32_t adj)
{
    uint8_t v;
    a &= 0xFFFF;
    if (a - 0x100u < 0xA00u || a < 0x20)
        v = mem[a];
    else if (a >= 0xB00)
        v = 0;
    else {
        jit_sync_out(adj);
        v = io_read((uint16_t)a);
        jit_sync_in(adj);
    }
    if (d < 32)
        mem[d] = v;
    return v;
}

static ARD_RAM void jit_h_wr(uint32_t a, uint32_t v, int32_t adj)
{
    a &= 0xFFFF;
    if (a - 0x100u < 0xA00u || a < 0x20)
        mem[a] = (uint8_t)v;
    else if (a < 0x100) {
        jit_sync_out(adj);
        io_write((uint16_t)a, (uint8_t)v);
        jit_sync_in(adj);
    }
}

/* SBI / CBI: op = bit | 8 for set */
static ARD_RAM void jit_h_bit(uint32_t a, uint32_t op, int32_t adj)
{
    jit_sync_out(adj);
    uint8_t v = io_read((uint16_t)a), m = (uint8_t)(1u << (op & 7));
    io_write((uint16_t)a, (uint8_t)((op & 8) ? v | m : v & ~m));
    jit_sync_in(adj);
}

/* the stack where SP is not in the SRAM: as PUSH/POP in exec() */
static ARD_RAM void jit_h_push2(uint32_t v, uint32_t unused, int32_t adj)
{
    (void)unused;
    jit_h_wr(jc.sp, v & 0xFF, adj);
    jc.sp--;
    jit_h_wr(jc.sp, (v >> 8) & 0xFF, adj);
    jc.sp--;
}

static ARD_RAM uint32_t jit_h_pop2(uint32_t unused, uint32_t u2, int32_t adj)
{
    (void)unused; (void)u2;
    jc.sp++;
    uint32_t h = jit_h_rd(jc.sp, 32, adj);
    jc.sp++;
    uint32_t l = jit_h_rd(jc.sp, 32, adj);
    return h << 8 | l;
}

static ARD_RAM void jit_h_push1(uint32_t d, uint32_t unused, int32_t adj)
{
    (void)unused;
    jit_h_wr(jc.sp, mem[d], adj);
    jc.sp--;
}

static ARD_RAM void jit_h_pop1(uint32_t d, uint32_t unused, int32_t adj)
{
    (void)unused;
    jc.sp++;
    jit_h_rd(jc.sp, d, adj);
}

/* The rare paths of translated code, through the shared trampoline: `rec`
 * the 4 halfwords after its BL: kind | d << 8, adj | pointer << 8 (post-
 * increment), the PC to leave to if the limit came down (0xFFFF: none),
 * the adj there (or PUSH2's value). 0: go on; else the PC + 1 to leave to. */
enum { CK_RD, CK_WR, CK_BIT, CK_PUSH1, CK_POP1, CK_PUSH2, CK_POP2, CK_LINK };

static ARD_RAM uint32_t jit_slow(uint32_t a, const uint16_t *rec)
{
    unsigned kind = rec[0] & 0xFF, d = rec[0] >> 8, adj = rec[1] & 0xFF, p = rec[1] >> 8;
    switch (kind) {
    case CK_RD: jc.tmp = jit_h_rd(a, d, (int32_t)adj); break;
    case CK_WR: jit_h_wr(a, mem[d], (int32_t)adj); break;
    case CK_BIT: jit_h_bit(a, d, (int32_t)adj); break;
    case CK_PUSH1: jit_h_push1(d, 0, (int32_t)adj); break;
    case CK_POP1: jit_h_pop1(d, 0, (int32_t)adj); jc.tmp = mem[d]; break;
    case CK_PUSH2: jit_h_push2(rec[3], 0, (int32_t)adj); break;
    case CK_POP2: jc.tmp = jit_h_pop2(0, 0, (int32_t)adj); break;
    default: break;
    }
    if (p) {
        unsigned v = (mem[p] | mem[p + 1] << 8) + 1u;
        mem[p] = (uint8_t)v;
        mem[p + 1] = (uint8_t)(v >> 8);
    }
    if (rec[2] != 0xFFFF && jc.left < 0) {
        jc.left += (int16_t)rec[3];
        return rec[2] + 1u;
    }
    return 0;
}

/* into the translated code: r0 = &jc, r1 = entry (Thumb bit set); back with
 * the AVR PC where it left off */
__attribute__((naked, noinline)) static uint32_t jit_enter(jit_ctx_t *c, uint32_t entry)
{
    (void)c; (void)entry;
    __asm__ volatile(
        "push {r3-r11, lr}\n\t"
        "mov r8, r0\n\t"
        "ldr r4, [r8, #44]\n\t"
        "ldr r5, [r8, #32]\n\t"
        "ldrb r6, [r8, #40]\n\t"
        "ldrh r7, [r8, #42]\n\t"
        "ldr r9, [r8, #48]\n\t"
        "ldr r10, [r8, #52]\n\t"
        "ldr r11, [r8, #56]\n\t"
        "bx r1\n\t");
}

/* the translator itself runs seldom: small rather than fast (the flash is tight) */
#pragma GCC push_options
#pragma GCC optimize("Os")

/* ---- Thumb-2 encodings ------------------------------------------------------------ */

enum { R0, R1, R2, R3, RDM, RLEFT, RF, RSP, RCTX, RJB, RCODE, RFLASH, R12 };
enum { EQ, NE, CS, CC, MI, PL, VS, VC, HI, LS, GE, LT, GT, LE };

static void E16(unsigned h)
{
    if (jp < jend)
        *jp++ = (uint16_t)h;
    else
        jfull = true;
}

static void E32(unsigned a, unsigned b)
{
    E16(a);
    E16(b);
}

/* ThumbExpandImm in reverse: the 12-bit field for v, or -1 */
static int t2imm(uint32_t v)
{
    if (v < 256)
        return (int)v;
    uint32_t b = v & 0xFF;
    if (v == (b | b << 16)) return (int)(0x100 | b);
    b = (v >> 8) & 0xFF;
    if (v == (b << 8 | b << 24)) return (int)(0x200 | b);
    b = v & 0xFF;
    if (v == b * 0x01010101u) return (int)(0x300 | b);
    for (int r = 8; r < 32; r++) {
        uint32_t u = (v << r) | (v >> (32 - r));
        if (u < 256 && (u & 0x80))
            return r << 7 | (int)(u & 0x7F);
    }
    return -1;
}

enum { DP_AND = 0, DP_BIC = 1, DP_ORR = 2, DP_ORN = 3, DP_EOR = 4, DP_ADD = 8, DP_ADC = 10,
       DP_SBC = 11, DP_SUB = 13, DP_RSB = 14 };

static void movw(int rd, unsigned imm);
static void dpr(int op, int s, int rd, int rn, int rm, int type, int n);

/* <op>{s} rd, rn, #imm (imm must be encodable) */
static void dpi(int op, int s, int rd, int rn, uint32_t imm)
{
    int e = t2imm(imm);
    if (e < 0) {
        if ((op == DP_ADD || op == DP_SUB) && !s && imm < 4096) {   /* addw / subw */
            E32((op == DP_ADD ? 0xF200 : 0xF2A0) | (imm >> 11 & 1) << 10 | rn,
                (imm >> 8 & 7) << 12 | rd << 8 | (imm & 0xFF));
            return;
        }
        if (op == DP_SUB && s && rd == 15 && imm < 65536 && rn != R12) {   /* cmp via r12 */
            movw(R12, imm);
            dpr(DP_SUB, 1, 15, rn, R12, 0, 0);
            return;
        }
        jfull = true;                   /* never meant to happen: our constants are small */
        return;
    }
    E32(0xF000 | (e >> 11 & 1) << 10 | op << 5 | s << 4 | rn,
        (e >> 8 & 7) << 12 | rd << 8 | (e & 0xFF));
}

/* <op>{s} rd, rn, rm, <shift> #n (type 0 LSL, 1 LSR, 2 ASR, 3 ROR) */
static void dpr(int op, int s, int rd, int rn, int rm, int type, int n)
{
    E32(0xEA00 | op << 5 | s << 4 | rn, (n >> 2 & 7) << 12 | rd << 8 | (n & 3) << 6 | type << 4 | rm);
}

static void ldrb_i(int rt, int rn, unsigned imm)
{
    if (rt < 8 && rn < 8 && imm < 32) E16(0x7800 | imm << 6 | rn << 3 | rt);
    else E32(0xF890 | rn, rt << 12 | imm);
}

static void strb_i(int rt, int rn, unsigned imm)
{
    if (rt < 8 && rn < 8 && imm < 32) E16(0x7000 | imm << 6 | rn << 3 | rt);
    else E32(0xF880 | rn, rt << 12 | imm);
}

static void ldrh_i(int rt, int rn, unsigned imm)
{
    if (rt < 8 && rn < 8 && imm < 64 && !(imm & 1)) E16(0x8800 | (imm >> 1) << 6 | rn << 3 | rt);
    else E32(0xF8B0 | rn, rt << 12 | imm);
}

static void strh_i(int rt, int rn, unsigned imm)
{
    if (rt < 8 && rn < 8 && imm < 64 && !(imm & 1)) E16(0x8000 | (imm >> 1) << 6 | rn << 3 | rt);
    else E32(0xF8A0 | rn, rt << 12 | imm);
}

static void ldr_i(int rt, int rn, unsigned imm)
{
    if (rt < 8 && rn < 8 && imm < 128 && !(imm & 3)) E16(0x6800 | (imm >> 2) << 6 | rn << 3 | rt);
    else E32(0xF8D0 | rn, rt << 12 | imm);
}

static void str_i(int rt, int rn, unsigned imm)
{
    if (rt < 8 && rn < 8 && imm < 128 && !(imm & 3)) E16(0x6000 | (imm >> 2) << 6 | rn << 3 | rt);
    else E32(0xF8C0 | rn, rt << 12 | imm);
}

static void ldrb_r(int rt, int rn, int rm)
{
    if (rt < 8 && rn < 8 && rm < 8) E16(0x5C00 | rm << 6 | rn << 3 | rt);
    else E32(0xF810 | rn, rt << 12 | rm);
}

static void strb_r(int rt, int rn, int rm)
{
    if (rt < 8 && rn < 8 && rm < 8) E16(0x5400 | rm << 6 | rn << 3 | rt);
    else E32(0xF800 | rn, rt << 12 | rm);
}

/* movs rd, #imm (low rd, imm < 256; sets the flags) or movw */
static void movi(int rd, unsigned imm)
{
    if (rd < 8 && imm < 256) E16(0x2000 | rd << 8 | imm);
    else E32(0xF240 | (imm >> 11 & 1) << 10 | (imm >> 12 & 0xF), (imm >> 8 & 7) << 12 | rd << 8 | (imm & 0xFF));
}

static void movw(int rd, unsigned imm)
{
    E32(0xF240 | (imm >> 11 & 1) << 10 | (imm >> 12 & 0xF), (imm >> 8 & 7) << 12 | rd << 8 | (imm & 0xFF));
}

/* the 16-bit two-register ALU forms (low registers, set flags): op rdn, rm */
enum { A_AND = 0x4000, A_EOR = 0x4040, A_ADC = 0x4140, A_SBC = 0x4180, A_TST = 0x4200,
       A_NEG = 0x4240, A_CMP = 0x4280, A_ORR = 0x4300, A_MUL = 0x4340, A_BIC = 0x4380, A_MVN = 0x43C0 };
static void alu(unsigned op, int rdn, int rm) { E16(op | rm << 3 | rdn); }

static void adds_r(int rd, int rn, int rm) { E16(0x1800 | rm << 6 | rn << 3 | rd); }
static void subs_r(int rd, int rn, int rm) { E16(0x1A00 | rm << 6 | rn << 3 | rd); }
static void adds_i8(int rdn, unsigned imm) { E16(0x3000 | rdn << 8 | imm); }
static void subs_i8(int rdn, unsigned imm) { E16(0x3800 | rdn << 8 | imm); }
static void cmp_i8(int rn, unsigned imm) { E16(0x2800 | rn << 8 | imm); }
static void lsls_i(int rd, int rm, unsigned n) { E16(0x0000 | n << 6 | rm << 3 | rd); }
static void lsrs_i(int rd, int rm, unsigned n) { E16(0x0800 | (n & 31) << 6 | rm << 3 | rd); }
static void uxtb(int rd, int rm) { E16(0xB2C0 | rm << 3 | rd); }
static void uxth(int rd, int rm) { E16(0xB280 | rm << 3 | rd); }
static void sxtb(int rd, int rm) { E16(0xB240 | rm << 3 | rd); }
static void mov_r(int rd, int rm) { E16(0x4600 | (rd & 8) << 4 | rm << 3 | (rd & 7)); }
static void bfi(int rd, int rn, int lsb, int w) { E32(0xF360 | rn, (lsb >> 2) << 12 | rd << 8 | (lsb & 3) << 6 | (lsb + w - 1)); }
static void ubfx(int rd, int rn, int lsb, int w) { E32(0xF3C0 | rn, (lsb >> 2) << 12 | rd << 8 | (lsb & 3) << 6 | (w - 1)); }
static void mrs_apsr(int rd) { E32(0xF3EF, 0x8000 | rd << 8); }
static void it(int cond, unsigned mask) { E16(0xBF00 | cond << 4 | mask); }   /* mask 8: IT, 4|..: ITE.. */
static void blx_r(int rm) { E16(0x4780 | rm << 3); }
static void bx_r(int rm) { E16(0x4700 | rm << 3); }
static void clz(int rd, int rm) { E32(0xFAB0 | rm, 0xF080 | rd << 8 | rm); }
static void mul(int rd, int rn, int rm) { E32(0xFB00 | rn, 0xF000 | rd << 8 | rm); }

/* r6 bit = r (0 or 1) */
static void setflag(int r, int bit) { bfi(RF, r, bit, 1); }

/* ---- branches ---- */

static void put_bw(uint16_t *at, const uint16_t *to, int cond)    /* cond < 0: B.W */
{
    int32_t off = (int32_t)((const char *)to - ((const char *)at + 4));
    uint32_t s = off < 0;
    if (cond < 0) {
        uint32_t i1 = (off >> 23) & 1, i2 = (off >> 22) & 1;
        uint32_t j1 = (~(i1 ^ s)) & 1, j2 = (~(i2 ^ s)) & 1;
        at[0] = (uint16_t)(0xF000 | s << 10 | ((off >> 12) & 0x3FF));
        at[1] = (uint16_t)(0x9000 | j1 << 13 | j2 << 11 | ((off >> 1) & 0x7FF));
    } else {
        at[0] = (uint16_t)(0xF000 | s << 10 | cond << 6 | ((off >> 12) & 0x3F));
        at[1] = (uint16_t)(0x8000 | ((off >> 18) & 1) << 13 | ((off >> 19) & 1) << 11 | ((off >> 1) & 0x7FF));
    }
}

static int bw_cond(const uint16_t *at)             /* the condition of a branch put there, -1: B.W */
{
    return (at[1] & 0x1000) ? -1 : (at[0] >> 6) & 0xF;
}

/* a 32-bit branch to an address known now */
static void b_to(const uint16_t *to, int cond)
{
    if (jp + 2 > jend) { jfull = true; return; }
    put_bw(jp, to, cond);
    jp += 2;
}

/* forward branches inside a block: a site now, its target later */
static uint16_t *b_fwd(int cond)
{
    uint16_t *at = jp;
    E32(0, 0);
    if (jfull) return NULL;
    at[1] = (uint16_t)(cond < 0 ? 0x1000 : 0);  /* remembered for land() */
    at[0] = (uint16_t)(cond < 0 ? 0 : cond);
    return at;
}

static void land(uint16_t *at)                      /* the forward branch at `at` comes here */
{
    if (!at || jfull) return;
    int cond = (at[1] & 0x1000) ? -1 : at[0];
    put_bw(at, jp, cond);
}

/* 16-bit forward conditional branch over at most 254 bytes */
static uint16_t *b16_fwd(int cond)
{
    uint16_t *at = jp;
    E16(0xD000 | cond << 8);
    return jfull ? NULL : at;
}

static void land16(uint16_t *at)
{
    if (!at || jfull) return;
    int32_t off = (int32_t)((char *)jp - ((char *)at + 4));
    if (off < 0 || off > 254) { jfull = true; return; }    /* would be a bug: block too long */
    *at = (uint16_t)((*at & 0xFF00) | (off >> 1));
}

/* ---- program analysis --------------------------------------------------------------- */

/* the instruction at i (a block start's slot holds O_JIT/O_JITX: decoded again) */
static uint32_t jit_w(int i)
{
    uint32_t w = code[i & 0x3FFF].w;
    uint8_t op = (uint8_t)w;
    if (op == O_JIT || op == O_JITX) {
        insn_t c;
        decode_one(i & 0x3FFF, &c);
        w = (uint32_t)c.op | (uint32_t)c.d << 8 | (uint32_t)c.x << 16;
    }
    return w;
}
static uint8_t jit_op(int i) { return (uint8_t)jit_w(i); }
static unsigned jit_d(int i) { return (jit_w(i) >> 8) & 0xFF; }
static unsigned jit_x(int i) { return jit_w(i) >> 16; }
static int jit_len(int i) { return two_words(i) ? 2 : 1; }

/* flags an instruction reads / writes */
static void jit_usedef(int i, uint8_t *use, uint8_t *def)
{
    uint8_t u = 0, df = 0;
    switch (jit_op(i)) {
    case O_ADD: df = 0x3F; break;
    case O_ADC: df = 0x3F; u = FC; break;
    case O_SUB: case O_SUBI: case O_CP: case O_CPI: case O_NEG: df = 0x3F; break;
    case O_SBC: case O_SBCI: case O_CPC: df = 0x3F; u = FC | FZ; break;
    case O_AND: case O_OR: case O_EOR: case O_ANDI: case O_ORI: df = FS | FV | FN | FZ; break;
    case O_INC: case O_DEC: df = FS | FV | FN | FZ; break;
    case O_COM: case O_ASR: case O_LSR: case O_ADIW: case O_SBIW: df = 0x1F; break;
    case O_ROR: df = 0x1F; u = FC; break;
    case O_MUL: case O_MULS: case O_MULSU: case O_FMUL: case O_FMULS: case O_FMULSU: df = FC | FZ; break;
    case O_BSET: case O_BCLR: df = (uint8_t)(1u << jit_d(i)); break;
    case O_BST: df = FT; break;
    case O_BLD: u = FT; break;
    case O_BRBS: case O_BRBC: u = (uint8_t)(1u << jit_d(i)); break;
    case O_IN: if (jit_x(i) == SREG) u = 0xFF; break;
    case O_LDS: if (jit_x(i) == SREG) u = 0xFF; break;
    case O_OUT: if (jit_x(i) == SREG) df = 0xFF; break;
    case O_STS: if (jit_x(i) == SREG) { u = 0xFF; df = 0xFF; } break;  /* through io_write */
    case O_RETI: df = FI; break;
    default: break;
    }
    *use = u;
    *def = df;
}

/* the instructions that can follow; -1 for "unknown" (all flags read) */
static int jit_succ(int i, int s[2])
{
    int n1 = (i + jit_len(i)) & 0x3FFF;
    switch (jit_op(i)) {
    case O_BRBS: case O_BRBC: s[0] = (i + 1) & 0x3FFF; s[1] = (i + 1 + (int16_t)jit_x(i)) & 0x3FFF; return 2;
    case O_RJMP: case O_RCALL: s[0] = (i + 1 + (int16_t)jit_x(i)) & 0x3FFF; return 1;
    case O_JMP: case O_CALL: s[0] = jit_x(i) & 0x3FFF; return 1;
    case O_RET: case O_RETI: case O_IJMP: case O_ICALL: return -1;
    case O_CPSE: case O_SBRC: case O_SBRS: case O_SBIC: case O_SBIS:
        s[0] = (i + 1) & 0x3FFF; s[1] = (i + 1 + jit_len(i + 1)) & 0x3FFF; return 2;
    default: s[0] = n1; return 1;
    }
}

static void jit_liveness(void)
{
    bool changed = true;
    memset(jlive, 0, 0x4000);
    for (int pass = 0; changed && pass < 200; pass++) {
        changed = false;
        for (int i = 0x3FFF; i >= 0; i--) {
            int s[2], n = jit_succ(i, s);
            uint8_t out = FI;
            if (n < 0)
                out = 0xFF;
            for (int k = 0; k < n; k++) {
                uint8_t u, df;
                jit_usedef(s[k], &u, &df);
                out |= u | (jlive[s[k]] & ~df);
            }
            if (out != jlive[i]) {
                jlive[i] |= out;
                changed = true;
            }
        }
    }
}

/* block starts: targets, after jumps/calls/skips, the vectors */
static bool jit_is_target_start(int i)
{
    return (uint8_t)code[i].w == O_JIT || (uint8_t)code[i].w == O_JITX;
}

static void jit_mark(int i)
{
    i &= 0x3FFF;
    uint8_t op = (uint8_t)code[i].w;
    if (op == O_JIT || op == O_JITX)
        return;
    code[i].w = O_JITX;                 /* the instruction itself: decode_one() again */
    code[i].h = exec_ops[O_JITX];
}

static void jit_leaders(void)
{
    static uint8_t mark[0x4000 / 8];
    memset(mark, 0, sizeof mark);
#define MARK(v) (mark[((v) & 0x3FFF) >> 3] |= (uint8_t)(1u << ((v) & 7)))
    for (int v = 0; v < 43; v++)
        MARK(2 * v);
    for (int i = 0; i < 0x4000; i++) {
        int s[2], n = jit_succ(i, s);
        uint8_t op = jit_op(i);
        switch (op) {
        case O_BRBS: case O_BRBC: case O_RJMP: case O_JMP: case O_RCALL: case O_CALL:
        case O_CPSE: case O_SBRC: case O_SBRS: case O_SBIC: case O_SBIS:
            for (int k = 0; k < n; k++) MARK(s[k]);
            MARK(i + jit_len(i));
            break;
        case O_RET: case O_RETI: case O_IJMP: case O_ICALL: case O_SLEEP:
            MARK(i + 1);
            break;
        default:
            break;
        }
    }
    for (int i = 0; i < 0x4000; i++)
        if (mark[i >> 3] & (1u << (i & 7)))
            jit_mark(i);
#undef MARK
}

/* ---- translating a block ------------------------------------------------------------ */

static int jit_cost(uint8_t op)                 /* the fixed cycles (the last of a block may vary) */
{
    switch (op) {
    case O_MUL: case O_MULS: case O_MULSU: case O_FMUL: case O_FMULS: case O_FMULSU:
    case O_LDDZ: case O_LDDY: case O_STDZ: case O_STDY: case O_LDS: case O_STS:
    case O_LDZP: case O_LDMZ: case O_LDYP: case O_LDMY: case O_LDX: case O_LDXP: case O_LDMX:
    case O_STZP: case O_STMZ: case O_STYP: case O_STMY: case O_STX: case O_STXP: case O_STMX:
    case O_POP: case O_PUSH: case O_ADIW: case O_SBIW: case O_CBI: case O_SBI:
        return 2;
    case O_LPMZ: case O_LPMZP: case O_LPM0:
        return 3;
    default:
        return 1;
    }
}

static bool jit_terminator(uint8_t op)
{
    switch (op) {
    case O_BRBS: case O_BRBC: case O_RJMP: case O_JMP: case O_RCALL: case O_CALL:
    case O_RET: case O_RETI: case O_IJMP: case O_ICALL:
    case O_CPSE: case O_SBRC: case O_SBRS: case O_SBIC: case O_SBIS:
        return true;
    default:
        return false;
    }
}

/* what the translator leaves to the interpreter */
static bool jit_can(int i)
{
    uint8_t op = jit_op(i);
    unsigned d = jit_d(i);
    switch (op) {
    case O_FMUL: case O_FMULS: case O_FMULSU: case O_SPM: case O_ILLEGAL: case O_WRAP:
        return false;
    case O_LDXP: case O_LDMX: case O_STXP: case O_STMX: return d != 26 && d != 27;
    case O_LDYP: case O_LDMY: case O_STYP: case O_STMY: return d != 28 && d != 29;
    case O_LDZP: case O_LDMZ: case O_STZP: case O_STMZ: return d != 30 && d != 31;
    default:
        return op < O_ILLEGAL;          /* base instructions only */
    }
}

/* the current block */
static struct {
    int start, pre, s;                  /* first word, cycles of all but the last, before this one */
    int adj;                            /* pre - s */
    int next;                           /* the instruction after this one */
    int cost;                           /* its cycles (fixed: not the last) */
    bool last;
    /* cold paths, put after the block */
    int ncold;
    struct cold {
        uint16_t *from;                 /* the branch into it */
        uint16_t *resume;               /* where it goes back to */
        int kind, d, p, adj, exit_pc, exit_adj;
    } cold[48];
    /* branches to blocks not translated yet: hooked into their lists on success */
    int nlink;
    struct { uint16_t *site; int pc; } link[64];
} jb;

/* BL from here to a shared routine */
static void bl_to(uint32_t off)
{
    if (jp + 2 > jend) { jfull = true; return; }
    put_bw(jp, jbuf + off, -1);
    jp[1] |= 0x4000;                    /* B.W -> BL */
    jp += 2;
}

/* the record after a BL to the trampoline (see jit_slow) */
static void rec(int kind, unsigned d, int adj, unsigned p, int exit_pc, int val)
{
    E16((unsigned)kind | d << 8);
    E16((unsigned)adj | p << 8);
    E16(exit_pc < 0 ? 0xFFFF : (unsigned)exit_pc);
    E16((unsigned)val & 0xFFFF);
}

/* the exit after this instruction if the limit came down (not for the last) */
static int exit_pc_here(void) { return jb.last ? -1 : jb.next; }
static int exit_adj_here(void) { return jb.adj - jb.cost; }

/* a helper through the trampoline, inline (r0 = the address) */
static void call_slow(int kind, unsigned d, bool check)
{
    bl_to(jit_tramp_off);
    rec(kind, d, jb.adj, 0, check ? exit_pc_here() : -1, exit_adj_here());
}

/* left the block after an instruction: r5 back to the interpreter's left */
static void exit_to(int pc, int adj, bool stop)
{
    if (adj > 0) adds_i8(RLEFT, (unsigned)adj);
    else if (adj < 0) subs_i8(RLEFT, (unsigned)-adj);
    movw(R0, (unsigned)pc);
    b_to(jbuf + (stop ? jit_exit_stop_off : jit_exit_off), -1);
}

static struct cold *add_cold(uint16_t *from, int kind, unsigned d, unsigned p)
{
    if (jb.ncold >= (int)(sizeof jb.cold / sizeof jb.cold[0])) { jfull = true; return &jb.cold[0]; }
    struct cold *c = &jb.cold[jb.ncold++];
    c->from = from; c->resume = NULL; c->kind = kind; c->d = (int)d; c->p = (int)p;
    c->adj = jb.adj;
    c->exit_pc = exit_pc_here();
    c->exit_adj = exit_adj_here();
    return c;
}

/* the address in r0: if it is in the SRAM, fast; else the helper (cold) */
static uint16_t *sram_check(void)
{
    dpi(DP_SUB, 0, R1, R0, 0x100);
    dpi(DP_SUB, 1, 15, R1, 0xA00);          /* cmp r1, #0xA00 */
    return b_fwd(CS);
}

/* ---- flags ---- */

static void z_from(int r, bool keep)            /* Z from the low byte of r (scratch r2) */
{
    lsls_i(R2, r, 24);
    if (keep) {
        it(NE, 8);
        dpi(DP_BIC, 0, RF, RF, FZ);
    } else {
        it(EQ, 0xC);                    /* ITE EQ */
        dpi(DP_ORR, 0, RF, RF, FZ);
        dpi(DP_BIC, 0, RF, RF, FZ);
    }
}

/* r0 = a, r1 = b; result in r3 (low byte), flags `need` exact; carry: use C */
static void gen_add(bool carry, uint8_t need)
{
    if (!(need & ~(FC | FZ))) {
        if (carry) {
            dpi(DP_AND, 0, R2, RF, 1);
            adds_r(R3, R0, R1);
            adds_r(R3, R3, R2);
        } else
            adds_r(R3, R0, R1);
        if (need & FC) { lsrs_i(R2, R3, 8); setflag(R2, 0); }
        if (need & FZ) z_from(R3, false);
        return;
    }
    bl_to(jit_t_off[carry ? 1 : 0]);    /* all of H S V N Z C (as f_add) */
}

/* r0 = a, r1 = b: a - b (- C); result in r3; keep: Z only cleared */
static void gen_sub(bool carry, bool keep, uint8_t need)
{
    if (!(need & ~(FC | FZ))) {
        if (carry) {
            dpi(DP_AND, 0, R2, RF, 1);
            subs_r(R3, R0, R1);
            subs_r(R3, R3, R2);
        } else
            subs_r(R3, R0, R1);
        if (need & FC) { lsrs_i(R2, R3, 31); setflag(R2, 0); }
        if (need & FZ) z_from(R3, keep);
        return;
    }
    (void)keep;                         /* with carry always keep, without never */
    bl_to(jit_t_off[carry ? 3 : 2]);    /* as f_sub */
}

/* the four flag routines (shared): r0 = a, r1 = b -> r3 = the result byte,
 * r6 with H S V N Z C as f_add / f_sub; clobber r0, r2, r12 */
static void gen_flag_routine(bool sub, bool carry)
{
    if (!sub) {
        if (carry) {
            dpi(DP_AND, 0, R12, RF, 1);
            dpr(DP_ORR, 0, R2, R12, R0, 0, 24);
            dpi(DP_RSB, 0, R3, R12, 0);
            lsrs_i(R3, R3, 8);
            dpr(DP_ORR, 0, R3, R3, R1, 0, 24);
        } else {
            lsls_i(R2, R0, 24);
            lsls_i(R3, R1, 24);
        }
        adds_r(R3, R2, R3);
    } else if (carry) {
        dpi(DP_AND, 0, R12, RF, 1);
        dpi(DP_EOR, 0, R12, R12, 1);            /* nc */
        dpr(DP_ORR, 0, R2, R12, R0, 0, 24);     /* A = a << 24 | nc */
        dpi(DP_RSB, 0, R12, R12, 0);
        dpr(DP_ORR, 0, R12, 15, R12, 1, 8);     /* mov r12, r12, lsr #8 */
        dpi(DP_EOR, 0, R3, R1, 0xFF);
        dpr(DP_ORR, 0, R3, R12, R3, 0, 24);     /* B = (~b & 0xFF) << 24 | (0 - nc) >> 8 */
        adds_r(R3, R2, R3);
    } else {
        lsls_i(R2, R0, 24);
        dpr(DP_SUB, 1, R3, R2, R1, 0, 24);      /* subs r3, r2, r1, lsl #24 */
    }
    mrs_apsr(R2);
    lsrs_i(R3, R3, 24);
    lsrs_i(R2, R2, 28);
    if (sub) adds_i8(R2, 16);
    E32(0xF810 | RCTX, R2 << 12 | R2);          /* ldrb r2, [r8, r2]: tab_add / tab_sub */
    if (sub && carry) {                         /* SBC, SBCI, CPC: Z only cleared */
        dpi(DP_AND, 1, 15, RF, FZ);
        it(EQ, 8);
        dpi(DP_BIC, 0, R2, R2, FZ);
    }
    alu(A_EOR, R0, R1);
    alu(A_EOR, R0, R3);
    dpi(DP_AND, 0, R0, R0, 0x10);
    dpr(DP_ORR, 0, R2, R2, R0, 0, 1);
    dpi(DP_AND, 0, RF, RF, 0xC0);
    alu(A_ORR, RF, R2);
    bx_r(14);
}

/* N, S = N ^ V, Z of the byte in r (V as given in vreg, or -1: V is 0); scratch r2, r12 */
static void gen_nsz(int r, int vreg, uint8_t need)
{
    if (need & (FN | FS)) {
        lsrs_i(R2, r, 7);
        if (need & FN) setflag(R2, 2);
        if (need & FS) {
            if (vreg >= 0) {
                dpr(DP_EOR, 0, R12, R2, vreg, 0, 0);
                setflag(R12, 4);
            } else
                setflag(R2, 4);
        }
    }
    if (need & FZ) z_from(r, false);
}

/* ---- one instruction ---- */

static void ld_r(int r, unsigned avr) { ldrb_i(r, RDM, avr); }
static void st_r(int r, unsigned avr) { strb_i(r, RDM, avr); }

/* the data-space load/store at r0 (helper if not SRAM); rd: AVR register;
 * p: the pointer to post-increment (done here; 0: none) */
static void gen_mem(bool store, unsigned rd, unsigned p)
{
    uint16_t *slow = sram_check();
    if (store) {
        ld_r(R2, rd);
        strb_r(R2, RDM, R0);
    } else {
        ldrb_r(R2, RDM, R0);
        st_r(R2, rd);
    }
    struct cold *c = add_cold(slow, store ? CK_WR : CK_RD, rd, p);
    if (p) {
        adds_i8(R0, 1);
        strh_i(R0, RDM, p);
    }
    c->resume = jp;
}

/* to another block (through the link routine while that is not translated
 * yet); r5 is the interpreter's left */
static void link_to(int pc)
{
    pc &= 0x3FFF;
    slot_t *s = &code[pc];
    if ((uint8_t)s->w == O_JIT) {
        b_to(jbuf + 2 * (s->w >> 16), -1);
        return;
    }
    /* bl link; .hword pc; .hword next site waiting for the same block */
    while (((uintptr_t)jp & 3) && !jfull) E16(0xBF00);
    uint16_t *site = jp;
    bl_to(jit_link_off);
    E16((unsigned)pc);
    E16(0);
    if (jfull) return;
    if (jb.nlink >= (int)(sizeof jb.link / sizeof jb.link[0])) { jfull = true; return; }
    jb.link[jb.nlink].site = site;
    jb.link[jb.nlink].pc = pc;
    jb.nlink++;
}

static void gen_skip_tail(int i, int cond_noskip)  /* after the test: skip or not */
{
    int n1 = (i + 1) & 0x3FFF, l1 = jit_len(i + 1), n2 = (i + 1 + l1) & 0x3FFF;
    uint16_t *ns = b16_fwd(cond_noskip);
    subs_i8(RLEFT, (unsigned)(1 + l1));
    link_to(n2);
    land16(ns);
    subs_i8(RLEFT, 1);
    link_to(n1);
}

static bool pin_port(unsigned a) { return PIN_PORT(a); }

/* R[d] = the PIN port a, inline (as L_IN) */
static void gen_pin(int r, unsigned a)
{
    ld_r(r, a + 2);
    ldr_i(R3, RCTX, C_PINK);
    ldrb_i(R12, R3, a - PINB);
    dpr(DP_AND, 0, r, r, R12, 0, 0);
}

/* the stop after SEI / OUT SREG enabling interrupts with one waiting */
static void gen_irq_enable_check(int newreg)
{
    /* if (new & I) && !(f & I) && irq_pending: irq_hold = 1, f = new, STOP */
    dpi(DP_AND, 1, 15, newreg, FI);
    uint16_t *n1 = b16_fwd(EQ);
    dpi(DP_AND, 1, 15, RF, FI);
    uint16_t *n2 = b16_fwd(NE);
    ldr_i(R1, RCTX, C_IRQP);
    ldrb_i(R1, R1, 0);
    cmp_i8(R1, 0);
    uint16_t *n3 = b16_fwd(EQ);
    ldr_i(R1, RCTX, C_IRQH);
    movi(R2, 1);
    strb_i(R2, R1, 0);
    mov_r(RF, newreg);
    exit_to(jb.next, jb.adj - jb.cost, true);
    land16(n1); land16(n2); land16(n3);
    mov_r(RF, newreg);
}

static void gen_insn(int i)
{
    uint8_t op = jit_op(i);
    unsigned d = jit_d(i), x = jit_x(i);
    uint8_t need = jlive[i];
    uint8_t u, df;
    jit_usedef(i, &u, &df);
    need &= df;
#ifdef ARD_JIT_NOLIVE
    need = df;
#endif
    switch (op) {
    case O_NOP: case O_BREAK: case O_WDR:
        break;
    case O_LDI: movi(R0, x); st_r(R0, d); break;
    case O_MOV: ld_r(R0, x); st_r(R0, d); break;
    case O_MOVW: ldrh_i(R0, RDM, x); strh_i(R0, RDM, d); break;
    case O_SWAP: ld_r(R0, d); lsls_i(R1, R0, 4); lsrs_i(R0, R0, 4); alu(A_ORR, R0, R1); st_r(R0, d); break;

    case O_ADD: case O_ADC:
        ld_r(R0, d); ld_r(R1, x);
        gen_add(op == O_ADC, need);
        st_r(R3, d);
        break;
    case O_SUB: case O_SBC: case O_CP: case O_CPC:
        ld_r(R0, d); ld_r(R1, x);
        gen_sub(op == O_SBC || op == O_CPC, op == O_SBC || op == O_CPC, need);
        if (op == O_SUB || op == O_SBC) st_r(R3, d);
        break;
    case O_SUBI: case O_SBCI: case O_CPI:
        ld_r(R0, d); movi(R1, x);
        gen_sub(op == O_SBCI, op == O_SBCI, need);
        if (op != O_CPI) st_r(R3, d);
        break;
    case O_NEG:
        movi(R0, 0); ld_r(R1, d);
        gen_sub(false, false, need);
        st_r(R3, d);
        break;

    case O_AND: case O_OR: case O_EOR: case O_ANDI: case O_ORI:
        ld_r(R0, d);
        if (op == O_ANDI || op == O_ORI) movi(R1, x); else ld_r(R1, x);
        alu(op == O_AND || op == O_ANDI ? A_AND : op == O_EOR ? A_EOR : A_ORR, R0, R1);
        st_r(R0, d);
        if (need & FV) dpi(DP_BIC, 0, RF, RF, FV);
        gen_nsz(R0, -1, need);
        break;
    case O_COM:
        ld_r(R0, d);
        dpi(DP_EOR, 0, R0, R0, 0xFF);
        st_r(R0, d);
        if (need & FC) dpi(DP_ORR, 0, RF, RF, FC);
        if (need & FV) dpi(DP_BIC, 0, RF, RF, FV);
        gen_nsz(R0, -1, need);
        break;
    case O_INC: case O_DEC:
        ld_r(R0, d);
        if (op == O_INC) adds_i8(R0, 1); else subs_i8(R0, 1);
        uxtb(R0, R0);
        st_r(R0, d);
        if (need & (FV | FS)) {         /* V: r == 0x80 (INC) / 0x7F (DEC) */
            dpi(DP_EOR, 0, R1, R0, op == O_INC ? 0x80 : 0x7F);
            clz(R1, R1);
            lsrs_i(R1, R1, 5);
            if (need & FV) setflag(R1, 3);
        }
        gen_nsz(R0, (need & FS) ? R1 : -1, need);
        break;
    case O_ASR: case O_LSR: case O_ROR:
        ld_r(R0, d);
        if (op == O_ROR) {
            dpi(DP_AND, 0, R1, RF, 1);
            lsls_i(R1, R1, 7);
        }
        dpi(DP_AND, 0, R3, R0, 1);              /* C */
        if (op == O_ASR) { sxtb(R0, R0); E16(0x1000 | 1 << 6 | R0 << 3 | R0); uxtb(R0, R0); }  /* asrs r0, r0, #1 */
        else lsrs_i(R0, R0, 1);
        if (op == O_ROR) alu(A_ORR, R0, R1);
        st_r(R0, d);
        if (need & FC) setflag(R3, 0);
        if (need & FS) setflag(R3, 4);          /* S = N ^ V = C */
        if (need & (FN | FV)) {
            lsrs_i(R2, R0, 7);
            if (need & FN) setflag(R2, 2);
            if (need & FV) { alu(A_EOR, R2, R3); setflag(R2, 3); }
        }
        if (need & FZ) z_from(R0, false);
        break;
    case O_ADIW: case O_SBIW:
        ldrh_i(R0, RDM, d);
        if (op == O_ADIW) { movi(R1, x); adds_r(R1, R0, R1); }
        else { movi(R1, x); subs_r(R1, R0, R1); }
        uxth(R1, R1);
        strh_i(R1, RDM, d);
        if (need) {
            /* ADIW: C = ~r & v, V = r & ~v (bit 15); SBIW: C = r & ~v, V = v & ~r */
            if (need & (FC | FV | FS)) {
                dpr(DP_BIC, 0, R2, op == O_ADIW ? R0 : R1, op == O_ADIW ? R1 : R0, 0, 0);  /* C word */
                dpr(DP_BIC, 0, R3, op == O_ADIW ? R1 : R0, op == O_ADIW ? R0 : R1, 0, 0);  /* V word */
                ubfx(R2, R2, 15, 1);
                ubfx(R3, R3, 15, 1);
                if (need & FC) setflag(R2, 0);
                if (need & FV) setflag(R3, 3);
            }
            if (need & (FN | FS)) {
                ubfx(R12, R1, 15, 1);
                if (need & FN) setflag(R12, 2);
                if (need & FS) { dpr(DP_EOR, 0, R12, R12, R3, 0, 0); setflag(R12, 4); }
            }
            if (need & FZ) {
                cmp_i8(R1, 0);
                it(EQ, 0xC);
                dpi(DP_ORR, 0, RF, RF, FZ);
                dpi(DP_BIC, 0, RF, RF, FZ);
            }
        }
        break;
    case O_MUL: case O_MULS: case O_MULSU:
        ld_r(R0, d); ld_r(R1, x);
        if (op != O_MUL) sxtb(R0, R0);
        if (op == O_MULS) sxtb(R1, R1);
        mul(R0, R0, R1);
        uxth(R0, R0);
        strh_i(R0, RDM, 0);
        if (need & FC) { lsrs_i(R2, R0, 15); setflag(R2, 0); }
        if (need & FZ) {
            cmp_i8(R0, 0);
            it(EQ, 0xC);
            dpi(DP_ORR, 0, RF, RF, FZ);
            dpi(DP_BIC, 0, RF, RF, FZ);
        }
        break;

    case O_BSET:
        if (d == 7) {
            dpi(DP_ORR, 0, R0, RF, FI);
            gen_irq_enable_check(R0);
        } else
            dpi(DP_ORR, 0, RF, RF, 1u << d);
        break;
    case O_BCLR: dpi(DP_BIC, 0, RF, RF, 1u << d); break;
    case O_BST: ld_r(R0, d); ubfx(R1, R0, (int)x, 1); setflag(R1, 6); break;
    case O_BLD: ld_r(R0, d); ubfx(R1, RF, 6, 1); bfi(R0, R1, (int)x, 1); st_r(R0, d); break;

    case O_LDDY: case O_LDDZ: case O_STDY: case O_STDZ:
        ldrh_i(R0, RDM, (op == O_LDDY || op == O_STDY) ? 28 : 30);
        if (x) adds_i8(R0, x);
        gen_mem(op == O_STDY || op == O_STDZ, d, 0);
        break;
    case O_LDX: case O_STX:
        ldrh_i(R0, RDM, 26);
        gen_mem(op == O_STX, d, 0);
        break;
    case O_LDXP: case O_LDYP: case O_LDZP: case O_STXP: case O_STYP: case O_STZP: {
        unsigned p = (op == O_LDXP || op == O_STXP) ? 26 : (op == O_LDYP || op == O_STYP) ? 28 : 30;
        ldrh_i(R0, RDM, p);
        gen_mem(op == O_STXP || op == O_STYP || op == O_STZP, d, p);
        break;
    }
    case O_LDMX: case O_LDMY: case O_LDMZ: case O_STMX: case O_STMY: case O_STMZ: {
        unsigned p = (op == O_LDMX || op == O_STMX) ? 26 : (op == O_LDMY || op == O_STMY) ? 28 : 30;
        ldrh_i(R0, RDM, p);
        subs_i8(R0, 1);
        uxth(R0, R0);
        strh_i(R0, RDM, p);
        gen_mem(op == O_STMX || op == O_STMY || op == O_STMZ, d, 0);
        break;
    }
    case O_LDS: case O_STS:
        if (x - 0x100u < 0xA00u || x < 0x20) {
            if (op == O_LDS) { ldrb_i(R0, RDM, x); st_r(R0, d); }
            else { ld_r(R0, d); strb_i(R0, RDM, x); }
        } else if (x >= 0xB00) {
            if (op == O_LDS) { movi(R0, 0); st_r(R0, d); }
        } else if (op == O_LDS && pin_port(x)) {
            gen_pin(R0, x); st_r(R0, d);
        } else if (op == O_LDS && x == SREG) {
            st_r(RF, d);
        } else {
            movw(R0, x);
            call_slow(op == O_LDS ? CK_RD : CK_WR, d, true);
        }
        break;
    case O_IN:
        if (pin_port(x)) { gen_pin(R0, x); st_r(R0, d); }
        else if (x == SREG) st_r(RF, d);
        else if (x == SPL) st_r(RSP, d);
        else if (x == SPH) { lsrs_i(R0, RSP, 8); st_r(R0, d); }
        else { movi(R0, x); call_slow(CK_RD, d, true); }
        break;
    case O_OUT:
        if (x == SREG) { ld_r(R0, d); gen_irq_enable_check(R0); }
        else if (x == SPL) { ld_r(R0, d); bfi(RSP, R0, 0, 8); uxth(RSP, RSP); }
        else if (x == SPH) { ld_r(R0, d); bfi(RSP, R0, 8, 8); uxth(RSP, RSP); }
        else { movi(R0, x); call_slow(CK_WR, d, true); }
        break;
    case O_SBI: case O_CBI:
        movi(R0, d);
        call_slow(CK_BIT, x | (op == O_SBI ? 8 : 0), true);
        break;
    case O_LPMZ: case O_LPMZP: case O_LPM0:
        ldrh_i(R0, RDM, 30);
        ubfx(R1, R0, 0, 15);
        ldrb_r(R1, RFLASH, R1);
        st_r(R1, op == O_LPM0 ? 0 : d);
        if (op == O_LPMZP) { adds_i8(R0, 1); strh_i(R0, RDM, 30); }
        break;
    case O_PUSH:                        /* shared routines; the record for their rare path */
        ld_r(R1, d);
        bl_to(jit_push1_off);
        rec(CK_PUSH1, d, jb.adj, 0, exit_pc_here(), exit_adj_here());
        break;
    case O_POP:
        bl_to(jit_pop1_off);
        rec(CK_POP1, d, jb.adj, 0, exit_pc_here(), exit_adj_here());
        st_r(R0, d);
        break;
    case O_SLEEP: {
        ldrb_i(R0, RDM, SMCR);
        dpi(DP_AND, 1, 15, R0, 1);
        uint16_t *no = b16_fwd(EQ);
        ldr_i(R1, RCTX, C_SLEEP);
        movi(R2, 1);
        strb_i(R2, R1, 0);
        exit_to(jb.next, jb.adj - jb.cost, true);
        land16(no);
        break;
    }

    /* ---- the block's end ---- */
    case O_BRBS: case O_BRBC: {
        dpi(DP_AND, 1, 15, RF, 1u << d);        /* tst r6, #bit */
        uint16_t *nt = b16_fwd(op == O_BRBS ? EQ : NE);
        subs_i8(RLEFT, 2);
        link_to(i + 1 + (int16_t)x);
        land16(nt);
        subs_i8(RLEFT, 1);
        link_to(i + 1);
        break;
    }
    case O_RJMP: subs_i8(RLEFT, 2); link_to(i + 1 + (int16_t)x); break;
    case O_JMP: subs_i8(RLEFT, 3); link_to((int)x); break;
    case O_RCALL: case O_CALL: case O_ICALL: {
        int ret = (i + (op == O_CALL ? 2 : 1)) & 0x3FFF;
        movw(R1, (unsigned)ret);
        bl_to(jit_push2_off);           /* the last instruction: adj 0, no exit */
        if (op == O_ICALL) {
            subs_i8(RLEFT, 3);
            ldrh_i(R0, RDM, 30);
            b_to(jbuf + jit_ind_off, -1);
        } else {
            subs_i8(RLEFT, op == O_CALL ? 4 : 3);
            link_to(op == O_CALL ? (int)x : i + 1 + (int16_t)x);
        }
        break;
    }
    case O_IJMP:
        subs_i8(RLEFT, 2);
        ldrh_i(R0, RDM, 30);
        b_to(jbuf + jit_ind_off, -1);
        break;
    case O_RET: case O_RETI:
        bl_to(jit_pop2_off);            /* r0 = the PC */
        subs_i8(RLEFT, 4);
        if (op == O_RETI) {
            dpi(DP_ORR, 0, RF, RF, FI);
            ldr_i(R1, RCTX, C_IRQH);
            movi(R2, 1);
            strb_i(R2, R1, 0);
            ubfx(R0, R0, 0, 14);
            b_to(jbuf + jit_exit_stop_off, -1);
        } else
            b_to(jbuf + jit_ind_off, -1);
        break;
    case O_CPSE:
        ld_r(R0, d); ld_r(R1, x);
        alu(A_CMP, R0, R1);
        gen_skip_tail(i, NE);
        break;
    case O_SBRC: case O_SBRS:
        ld_r(R0, d);
        dpi(DP_AND, 1, 15, R0, 1u << x);
        gen_skip_tail(i, op == O_SBRC ? NE : EQ);
        break;
    case O_SBIC: case O_SBIS:
        if (pin_port(d)) gen_pin(R0, d);
        else { movi(R0, d); call_slow(CK_RD, 32, false); }   /* r0 = the byte */
        dpi(DP_AND, 1, 15, R0, 1u << x);
        gen_skip_tail(i, op == O_SBIC ? NE : EQ);
        break;
    default:
        jfull = true;                   /* jit_can() let something through it cannot do */
        break;
    }
}

/* the rare paths: bl tramp; record; b.w back */
static void gen_cold(void)
{
    for (int k = 0; k < jb.ncold; k++) {
        struct cold *c = &jb.cold[k];
        if (jfull) return;
        land(c->from);
        bl_to(jit_tramp_off);
        rec(c->kind, (unsigned)c->d, c->adj, (unsigned)c->p, c->exit_pc, c->exit_adj);
        b_to(c->resume, -1);
    }
}

/* translate the block at `start`; false if it cannot (the slot keeps its op) */
static bool jit_translate(int start)
{
    if (jfull || !jit_on)
        return false;
    /* the extent and the cycles */
    int pc = start, n = 0, s = 0, pre = 0, last_pc = start;
    bool term = false, stop_untrans = false;
    for (;;) {
        if (n && jit_is_target_start(pc)) break;
        if (!jit_can(pc)) { stop_untrans = true; break; }
        uint8_t op = jit_op(pc);
        last_pc = pc;
        pre = s;
        n++;
        if (jit_terminator(op)) { term = true; break; }
        s += jit_cost(op);
        pc = (pc + jit_len(pc)) & 0x3FFF;
        if (pc == 0 || n >= 40 || s > 200) break;
    }
    if (!n) {
        JIT_STAT(1);
        return false;
    }
    uint16_t *save = jp;
    /* bail-out stub, then the entry (4-byte aligned) */
    adds_i8(RLEFT, (unsigned)pre);
    movw(R0, (unsigned)start);
    b_to(jbuf + jit_exit_off, -1);
    uint16_t *bail = save;
    while (((uintptr_t)jp & 3) && !jfull) E16(0xBF00);
    uint16_t *entry = jp;
    subs_i8(RLEFT, (unsigned)pre);
    {
        int32_t off = (int32_t)((char *)bail - ((char *)jp + 4));
        E16(0xD000 | MI << 8 | ((off >> 1) & 0xFF));    /* bmi bail */
    }
    jb.start = start; jb.pre = pre; jb.ncold = 0; jb.nlink = 0;
    int cs = 0;
    for (pc = start;;) {
        int len = jit_len(pc);
        jb.s = cs;
        jb.adj = pre - cs;
        jb.next = (pc + len) & 0x3FFF;
        jb.last = pc == last_pc;
        jb.cost = jit_cost(jit_op(pc));
        gen_insn(pc);
        if (jfull) break;
        if (jb.last) break;
        cs += jit_cost(jit_op(pc));
        pc = jb.next;
    }
    if (!term && !jfull) {
        /* the block ran into the next one (or something to interpret) */
        subs_i8(RLEFT, (unsigned)jit_cost(jit_op(last_pc)));
        int nx = (last_pc + jit_len(last_pc)) & 0x3FFF;
        if (stop_untrans) {
            movw(R0, (unsigned)nx);
            b_to(jbuf + jit_exit_off, -1);
            int after = (nx + jit_len(nx)) & 0x3FFF;
            jit_mark(after);            /* back into translated code soon after it */
        } else {
            jit_mark(nx);
            link_to(nx);
        }
    }
    gen_cold();
    if (jfull) {
        jp = save;
        JIT_STAT(1);
        return false;
    }
    JIT_STAT(0);
    /* the slot enters the block; branches waiting for it now come straight here */
    slot_t *sl = &code[start];
    uint32_t links = (uint8_t)sl->w == O_JITX ? sl->w >> 16 : 0;
    sl->w = O_JIT | (uint32_t)pre << 8 | (uint32_t)((entry - jbuf) / 2) << 16;
    sl->h = exec_ops[O_JIT];
    while (links) {
        uint16_t *site = jbuf + 2 * links;
        links = site[3];
        put_bw(site, entry, -1);
    }
    /* this block's own branches to blocks not translated: into their lists */
    for (int k = 0; k < jb.nlink; k++) {
        uint16_t *site = jb.link[k].site;
        slot_t *t = &code[jb.link[k].pc];
        if ((uint8_t)t->w == O_JIT)
            put_bw(site, jbuf + 2 * (t->w >> 16), -1);
        else if ((uint8_t)t->w == O_JITX) {
            site[3] = (uint16_t)(t->w >> 16);
            t->w = O_JITX | (uint32_t)((site - jbuf) / 2) << 16;
        }
    }
#if defined(__arm__)
    __asm__ volatile("dsb\n\tisb" ::: "memory");
#endif
    return true;
}

/* ---- building --------------------------------------------------------------------------- */

static void jit_stubs(void)
{
    jp = jbuf;
    /* exit: r0 = PC */
    jit_exit_off = 0;
    str_i(RLEFT, RCTX, C_LEFT);
    strb_i(RF, RCTX, C_F);
    strh_i(RSP, RCTX, C_SP);
    E32(0xE8BD, 0x8FF8);                /* pop {r3-r11, pc} */
    /* exit with STOP */
    jit_exit_stop_off = (uint32_t)(jp - jbuf);
    movi(R1, 1);
    strb_i(R1, RCTX, C_STOP);
    b_to(jbuf + jit_exit_off, -1);
    /* indirect: r0 = PC (16 bits) */
    while (((uintptr_t)jp & 3)) E16(0xBF00);
    jit_ind_off = (uint32_t)(jp - jbuf);
    ubfx(R0, R0, 0, 14);
    dpr(DP_ADD, 0, R1, RCODE, R0, 0, 3);
    ldr_i(R1, R1, 4);
    uxtb(R2, R1);
    cmp_i8(R2, O_JIT);
    b_to(jbuf + jit_exit_off, NE);
    lsrs_i(R1, R1, 16);
    dpr(DP_ADD, 0, R1, RJB, R1, 0, 2);
    adds_i8(R1, 1);
    bx_r(R1);

    /* trampoline to jit_slow: lr = the record after the BL (Thumb bit set) */
    jit_tramp_off = (uint32_t)(jp - jbuf);
    str_i(RLEFT, RCTX, C_LEFT);
    strb_i(RF, RCTX, C_F);
    strh_i(RSP, RCTX, C_SP);
    E16(0xB508);                        /* push {r3, lr} */
    dpi(DP_SUB, 0, R1, 14, 1);
    ldr_i(R3, RCTX, C_SLOW);
    blx_r(R3);
    E32(0xE8BD, 0x4008);                /* pop {r3, lr} */
    ldr_i(RLEFT, RCTX, C_LEFT);
    ldrb_i(RF, RCTX, C_F);
    ldrh_i(RSP, RCTX, C_SP);
    cmp_i8(R0, 0);
    uint16_t *out = b16_fwd(NE);
    ldr_i(R0, RCTX, C_TMP);
    dpi(DP_ADD, 0, 14, 14, 8);          /* past the record */
    bx_r(14);
    land16(out);
    subs_i8(R0, 1);
    b_to(jbuf + jit_exit_off, -1);

    /* a branch to a block not translated yet: bl here; .hword pc */
    jit_link_off = (uint32_t)(jp - jbuf);
    E32(0xF830 | 14, R0 << 12 | 0xC00 | 1);     /* ldrh r0, [lr, #-1] */
    b_to(jbuf + jit_exit_off, -1);

    /* the flags of add / adc / sub / sbc */
    for (int k = 0; k < 4; k++) {
        jit_t_off[k] = (uint32_t)(jp - jbuf);
        gen_flag_routine(k >= 2, k & 1);
    }

    /* CALL's push (r1 = the return address; the rare path straight to the helper) */
    jit_push2_off = (uint32_t)(jp - jbuf);
    dpi(DP_SUB, 0, R0, RSP, 0x101);
    dpi(DP_SUB, 1, 15, R0, 0x9FF);
    uint16_t *slow = b16_fwd(CS);
    strb_r(R1, RDM, RSP);
    lsrs_i(R1, R1, 8);
    subs_i8(RSP, 1);
    strb_r(R1, RDM, RSP);
    subs_i8(RSP, 1);
    bx_r(14);
    land16(slow);
    for (int k = 0; k < 2; k++) {       /* push2's rare path, then pop2's */
        E16(0xB508);                    /* push {r3, lr} */
        str_i(RLEFT, RCTX, C_LEFT);
        strb_i(RF, RCTX, C_F);
        strh_i(RSP, RCTX, C_SP);
        if (k == 0) mov_r(R0, R1); else movi(R0, 0);
        movi(R1, 0);
        movi(R2, 0);
        ldr_i(R3, RCTX, k == 0 ? C_PUSH2 : C_POP2);
        blx_r(R3);
        ldr_i(RLEFT, RCTX, C_LEFT);
        ldrb_i(RF, RCTX, C_F);
        ldrh_i(RSP, RCTX, C_SP);
        E32(0xE8BD, 0x4008);            /* pop {r3, lr} */
        bx_r(14);
        if (k == 0) {
            /* RET's pop -> r0 */
            jit_pop2_off = (uint32_t)(jp - jbuf);
            dpi(DP_SUB, 0, R0, RSP, 0xFF);
            dpi(DP_SUB, 1, 15, R0, 0x9FF);
            slow = b16_fwd(CS);
            adds_i8(RSP, 1);
            ldrb_r(R1, RDM, RSP);
            adds_i8(RSP, 1);
            ldrb_r(R0, RDM, RSP);
            dpr(DP_ORR, 0, R0, R0, R1, 0, 8);
            bx_r(14);
            land16(slow);
        }
    }

    /* PUSH (r1 = the byte) and POP (-> r0); a record follows the BL */
    jit_push1_off = (uint32_t)(jp - jbuf);
    dpi(DP_SUB, 0, R0, RSP, 0x100);
    dpi(DP_SUB, 1, 15, R0, 0xA00);
    slow = b16_fwd(CS);
    strb_r(R1, RDM, RSP);
    subs_i8(RSP, 1);
    dpi(DP_ADD, 0, 14, 14, 8);
    bx_r(14);
    land16(slow);
    b_to(jbuf + jit_tramp_off, -1);
    jit_pop1_off = (uint32_t)(jp - jbuf);
    dpi(DP_SUB, 0, R0, RSP, 0xFF);
    dpi(DP_SUB, 1, 15, R0, 0xA00);
    slow = b16_fwd(CS);
    adds_i8(RSP, 1);
    ldrb_r(R0, RDM, RSP);
    dpi(DP_ADD, 0, 14, 14, 8);
    bx_r(14);
    land16(slow);
    b_to(jbuf + jit_tramp_off, -1);
}

/* after ard_load_hex(): the interpreter without its pairs and patterns, the
 * block starts marked; buf (halfword aligned, executable) takes the code */
bool ard_jit_init(void *buf, size_t size)
{
    static uint8_t live[0x4000];
    jit_on = false;
    if (!exec_ops) exec(0);
    for (int k = 0; k < 0x4000; k++) {
        decode(k);
        code[k].h = exec_ops[code[k].w & 0xFF];
    }
    jlive = live;
    jit_liveness();
    jbuf = (uint16_t *)(((uintptr_t)buf + 3) & ~(uintptr_t)3);
    jend = (uint16_t *)((char *)buf + size);
    jfull = false;
    jit_stubs();
    if (jfull) return false;
    jit_leaders();
    memcpy(jc.tab_add, nzcv_add, 16);
    memcpy(jc.tab_sub, nzcv_sub, 16);
    jc.dm = mem;
    jc.jb = jbuf;
    jc.code = code;
    jc.flash = flash;
    jc.h_rd = jit_h_rd;
    jc.h_wr = jit_h_wr;
    jc.h_bit = jit_h_bit;
    jc.irq_pending = &irq_pending;
    jc.irq_hold = &irq_hold;
    jc.sleeping = &sleeping;
    jc.pin_keep = pin_keep;
    jc.h_push2 = jit_h_push2;
    jc.h_pop2 = jit_h_pop2;
    jc.h_push1 = jit_h_push1;
    jc.h_pop1 = jit_h_pop1;
    jc.slow = jit_slow;
    jit_on = true;
    return true;
}

size_t ard_jit_used(void) { return jbuf ? (size_t)((char *)jp - (char *)jbuf) : 0; }

#pragma GCC pop_options
