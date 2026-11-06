/* a2600.c -- see a2600.h
 *
 * Every CPU bus cycle (cpu_read/cpu_write) is one CPU clock: three TIA
 * colour clocks, one RIOT timer clock. After 76 of them the line ends.
 * WSYNC stops the CPU until the line ends (the TIA pulls RDY low).
 *
 * Address decoding of the 6507's 13 address lines:
 *   A12 = 1             cartridge (4 KB window, bank switching)
 *   A12 = 0, A7 = 0     TIA (writes: 6 address bits, reads: 4)
 *   A12 = 0, A7 = 1     RIOT: A9 = 0 RAM (128 bytes), A9 = 1 I/O and timer
 */
#include <string.h>
#include "a2600.h"
#include "cpu6502.h"
#include "tia.h"


#ifdef PBC_ON_DEVICE
#define A26_RAM __attribute__((section(".time_critical.a2600")))
#else
#define A26_RAM
#endif

#define LINE_CYCLES 76
#define MAX_LINES   320

uint8_t a26_joy[2];
uint8_t a26_switches;

static uint8_t ram[128];
static uint8_t sc_ram[128];             /* Superchip */

static const uint8_t *rom;
static uint32_t rom_size;
static int cart;
static bool superchip;
static uint32_t bank;                   /* offset of the 4 KB bank (F8/F6/F4) */
static uint32_t slice[4];               /* E0: 1 KB slices; 3F: [0] the 2 KB bank */
static int fe_pending;                  /* FE: an access to 01FEh, the bank follows */
static const uint8_t *page[4];          /* what each 1 KB of the 4 KB window shows */
static uint16_t hot_lo;                 /* lowest bank switching hotspot (0x2000: none) */
static bool is_fe;
static uint16_t fast_lo, fast_n;        /* fast_lo + fast_n: plain ROM, read at once */

static A26_RAM void map_pages(void)
{
    for (int i = 0; i < 4; i++) {
        uint32_t off;
        switch (cart) {
        case CART_2K: off = (uint32_t)(i & 1) << 10; break;
        case CART_4K: off = (uint32_t)i << 10; break;
        case CART_E0: off = slice[i]; break;
        case CART_3F: off = i < 2 ? slice[0] + ((uint32_t)i << 10) : rom_size - 0x800 + ((uint32_t)(i & 1) << 10); break;
        default:      off = bank + ((uint32_t)i << 10); break;
        }
        page[i] = rom + off;
    }
}

/* RIOT */
static uint8_t swcha_out, swacnt, swbcnt;

static int line_cycle;                  /* CPU cycles into the line */
static uint32_t line_base;              /* CPU cycles before this line */
static int lines;
static a26_line_fn line_out;
static int16_t tia_audio[MAX_LINES * 2];
static int audio_n;

/* ---- the line ------------------------------------------------------------ */

static int out_y;

static A26_RAM void end_of_line_cb(const uint8_t *px, bool blank)
{
    if (line_out)
        line_out(out_y, px, blank);
}

static A26_RAM void end_line(void)
{
    tia_end_line(end_of_line_cb);
    if (audio_n < MAX_LINES * 2 - 1)
        audio_n += tia_audio_line(tia_audio + audio_n);
    out_y++;
    lines++;
    line_base += LINE_CYCLES;
    line_cycle = 0;
}

static inline void cycle(void)
{
    if (++line_cycle >= LINE_CYCLES)
        end_line();
}

/* RIOT timer, worked out when it is read: one clock per CPU cycle, the
 * counter steps every 1, 8, 64 or 1024 clocks; after it passed 0 it runs
 * at one per clock from 0xff and sets the flag on every pass of 0. `t` is
 * the clocks since the timer was written (the write's own clock is the
 * first). */
static uint32_t tim_w;                  /* the cycle it was written */
static uint32_t tim_clr;                /* t when the flag was last cleared */
static uint8_t tim_v, tim_s;

static inline uint32_t tim_t(void) { return line_base + (uint32_t)line_cycle - tim_w; }

static A26_RAM uint8_t tim_intim(uint32_t t)
{
    uint32_t t0 = (uint32_t)(tim_v + 1) << tim_s;
    if (t < t0)
        return (uint8_t)(tim_v - (t >> tim_s));
    return (uint8_t)(0xff - (t - t0));
}

static A26_RAM uint8_t tim_flag(uint32_t t)
{
    uint32_t t0 = (uint32_t)(tim_v + 1) << tim_s;
    if (t < t0)
        return 0;
    uint32_t last = t0 + ((t - t0) & ~255u);    /* the last pass of 0 */
    return last > tim_clr ? 0x80 : 0;
}

/* ---- cartridge ----------------------------------------------------------- */

static A26_RAM void bank_switch(uint16_t a)
{
    uint32_t old_bank = bank, s0 = slice[0], s1 = slice[1], s2 = slice[2];
    switch (cart) {
    case CART_F8:
        if (a == 0x1ff8 || a == 0x1ff9) bank = (uint32_t)(a - 0x1ff8) << 12;
        break;
    case CART_F6:
        if (a >= 0x1ff6 && a <= 0x1ff9) bank = (uint32_t)(a - 0x1ff6) << 12;
        break;
    case CART_F4:
        if (a >= 0x1ff4 && a <= 0x1ffb) bank = (uint32_t)(a - 0x1ff4) << 12;
        break;
    case CART_E0:
        if (a >= 0x1fe0 && a <= 0x1ff7)
            slice[(a - 0x1fe0) >> 3] = (uint32_t)(a & 7) << 10;
        break;
    }
    if (bank != old_bank || slice[0] != s0 || slice[1] != s1 || slice[2] != s2)
        map_pages();
}

static inline uint8_t cart_read(uint16_t a)
{
    a &= 0x0fff;
    if (superchip && (a & 0xf80) == 0x80)
        return sc_ram[a & 0x7f];
    return page[a >> 10][a & 0x3ff];
}

static A26_RAM void fe_check(uint16_t a, uint8_t v)
{
    if (fe_pending) {
        bank = (v & 0x20) ? 0 : 0x1000;
        fe_pending = 0;
        map_pages();
    }
    if ((a & 0x1fff) == 0x01fe)
        fe_pending = 1;
}

/* ---- the bus ------------------------------------------------------------- */

static A26_RAM uint8_t riot_read(uint16_t a)
{
    switch (a & 7) {
    case 0: {                                   /* SWCHA: joysticks */
        uint8_t v = 0xff;
        for (int p = 0; p < 2; p++) {
            int j = a26_joy[p], sh = p ? 0 : 4;
            if (j & A26_UP)    v &= (uint8_t)~(0x1 << sh);
            if (j & A26_DOWN)  v &= (uint8_t)~(0x2 << sh);
            if (j & A26_LEFT)  v &= (uint8_t)~(0x4 << sh);
            if (j & A26_RIGHT) v &= (uint8_t)~(0x8 << sh);
        }
        return (uint8_t)((v & ~swacnt) | (swcha_out & swacnt));
    }
    case 1: return swacnt;
    case 2: {                                   /* SWCHB: console switches */
        uint8_t v = 0x0b;
        if (a26_switches & A26_RESET)  v &= (uint8_t)~0x01;
        if (a26_switches & A26_SELECT) v &= (uint8_t)~0x02;
        if (a26_switches & A26_BW)     v &= (uint8_t)~0x08;
        if (a26_switches & A26_DIFF0)  v |= 0x40;
        if (a26_switches & A26_DIFF1)  v |= 0x80;
        return v;
    }
    case 3: return swbcnt;
    case 4: case 6: {                           /* INTIM */
        uint32_t t = tim_t();
        tim_clr = t;
        return tim_intim(t);
    }
    default:                                    /* TIMINT */
        return tim_flag(tim_t());
    }
}

static A26_RAM void riot_write(uint16_t a, uint8_t v)
{
    if (a & 0x10) {                             /* timer */
        static const uint8_t sh[4] = { 0, 3, 6, 10 };
        tim_w = line_base + (uint32_t)line_cycle;
        tim_v = v;
        tim_s = sh[a & 3];
        tim_clr = 0;
        return;
    }
    switch (a & 3) {
    case 0: swcha_out = v; break;
    case 1: swacnt = v; break;
    case 3: swbcnt = v; break;
    }
}

A26_RAM __attribute__((noinline)) uint8_t cpu_read(uint16_t a)
{
    uint8_t v;
    a &= 0x1fff;
    if (a & 0x1000) {
        if (a >= hot_lo)
            bank_switch(a);
        v = cart_read(a);
    } else if (!(a & 0x80)) {
        v = tia_read((uint8_t)a, line_cycle * 3);
    } else if (!(a & 0x200)) {
        v = ram[a & 0x7f];
    } else {
        v = riot_read(a);
    }
    if (is_fe)
        fe_check(a, v);
    cycle();
    return v;
}

A26_RAM __attribute__((noinline)) void cpu_write(uint16_t a, uint8_t v)
{
    a &= 0x1fff;
    if (a & 0x1000) {
        if (a >= hot_lo)
            bank_switch(a);
        if (superchip && (a & 0xfff) < 0x80)
            sc_ram[a & 0x7f] = v;
    } else if (!(a & 0x80)) {
        if (a == 0x3f && cart == CART_3F) {
            uint32_t banks = rom_size >> 11;
            slice[0] = (uint32_t)(v % banks) << 11;
            map_pages();
        }
        tia_write((uint8_t)a, v, line_cycle * 3);
    } else if (!(a & 0x200)) {
        ram[a & 0x7f] = v;
    } else {
        riot_write(a, v);
    }
    if (is_fe)
        fe_check(a, v);
    cycle();
}

/* ---- cartridge type ------------------------------------------------------ */

static int count(const uint8_t *r, uint32_t n, const uint8_t *pat, int len)
{
    int c = 0;
    for (uint32_t i = 0; i + (uint32_t)len <= n; i++)
        if (memcmp(r + i, pat, (size_t)len) == 0)
            c++;
    return c;
}

/* the first 256 bytes of every 4 KB bank alike: room for Superchip RAM */
static bool has_superchip(const uint8_t *r, uint32_t n)
{
    for (uint32_t b = 0; b < n; b += 0x1000)
        for (int i = 1; i < 256; i++)
            if (r[b + i] != r[b])
                return false;
    return true;
}

static int guess_cart(const uint8_t *r, uint32_t n)
{
    if (n <= 2048)
        return CART_2K;
    if (n <= 4096)
        return CART_4K;
    static const uint8_t sta3f[] = { 0x85, 0x3f };
    if (count(r, n, sta3f, 2) > 1 && n % 2048 == 0 && n != 16384 && n != 32768)
        return CART_3F;
    if (n == 8192) {
        int e0 = 0;
        for (uint32_t i = 0; i + 2 < n; i++)
            if ((r[i] == 0x8d || r[i] == 0xad || r[i] == 0x2c) && r[i + 1] >= 0xe0 && r[i + 1] <= 0xf7 &&
                (r[i + 2] == 0x1f || r[i + 2] == 0xff || r[i + 2] == 0x3f || r[i + 2] == 0xbf))
                e0++;
        if (e0 >= 3)
            return CART_E0;
        static const uint8_t fe[] = { 0x20, 0x00, 0xd0, 0xc6, 0xc5 };
        if (count(r, n, fe, 5))
            return CART_FE;
        return CART_F8;
    }
    if (n == 16384)
        return CART_F6;
    if (n == 32768)
        return CART_F4;
    if (count(r, n, sta3f, 2) > 1)
        return CART_3F;
    return -1;
}

const char *a2600_cart_name(int type)
{
    static const char *const n[] = { "2K", "4K", "F8", "F6", "F4", "E0", "FE", "3F" };
    return type >= 0 && type <= CART_3F ? n[type] : "?";
}

__attribute__((optimize("Os"))) int a2600_insert(const uint8_t *r, uint32_t n)
{
    rom = r;
    rom_size = n;
    cart = guess_cart(r, n);
    superchip = (cart == CART_F8 || cart == CART_F6 || cart == CART_F4) && has_superchip(r, n);
    return cart;
}

__attribute__((optimize("Os"))) void a2600_reset(void)
{
    memset(ram, 0, sizeof ram);
    memset(sc_ram, 0, sizeof sc_ram);
    tia_reset();
    swcha_out = swacnt = swbcnt = 0;
    line_cycle = 0;
    line_base = 0;
    tim_w = 0;
    tim_v = 0;
    tim_s = 10;
    tim_clr = 0;
    fe_pending = 0;
    switch (cart) {
    case CART_F8: bank = 0x1000; break;
    case CART_FE: bank = 0; break;
    default:      bank = 0; break;
    }
    slice[0] = 4 << 10;
    slice[1] = 5 << 10;
    slice[2] = 6 << 10;
    slice[3] = 7 << 10;
    if (cart == CART_3F)
        slice[0] = 0;
    switch (cart) {
    case CART_F8: hot_lo = 0x1ff8; break;
    case CART_F6: hot_lo = 0x1ff6; break;
    case CART_F4: hot_lo = 0x1ff4; break;
    case CART_E0: hot_lo = 0x1fe0; break;
    default:      hot_lo = 0x2000; break;
    }
    is_fe = cart == CART_FE;
    fast_lo = superchip ? 0x1100 : 0x1000;     /* above the Superchip RAM */
    fast_n = is_fe ? 0 : (uint16_t)(hot_lo - fast_lo);
    map_pages();
    cpu_reset();
}

A26_RAM int a2600_frame(a26_line_fn line, int16_t *audio, int samples)
{
    line_out = line;
    lines = 0;
    out_y = 0;
    audio_n = 0;
    tia_set_fire(0, a26_joy[0] & A26_FIRE);
    tia_set_fire(1, a26_joy[1] & A26_FIRE);

    while (lines < MAX_LINES) {
        int before = lines;
        cpu_step();
        if (tia_wsync_req) {                    /* wait for the end of the line */
            tia_wsync_req = 0;
            if (lines == before)
                end_line();
        }
        if (lines != before && tia_vsync_started()) {
            /* VSYNC began on the line that just ended: the frame is over,
             * the next one counts from here */
            break;
        }
    }

    /* the frame's TIA samples (two per line) resampled to `samples`, the
     * DC taken out by a one-pole filter */
    if (audio && samples > 0) {
        static int32_t dc;
        int n = audio_n > 0 ? audio_n : 1;
        uint32_t step = ((uint32_t)n << 16) / (uint32_t)samples, pos = 0;
        for (int i = 0; i < samples; i++, pos += step) {
            int32_t v = audio_n > 0 ? tia_audio[pos >> 16] : 0;
            dc += (v * 256 - dc) >> 9;
            v -= dc >> 8;
            audio[i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
        }
    }
    return lines;
}

uint8_t *a2600_ram(void)
{
    return ram;
}

/* The CPU, compiled here: its opcode and operand fetches come from the
 * cartridge nearly always and take the short way. */
static inline uint8_t bus_fetch(uint16_t a)
{
    a &= 0x1fff;
    if ((uint16_t)(a - fast_lo) < fast_n) {
        uint8_t v = page[(a >> 10) & 3][a & 0x3ff];
        cycle();
        return v;
    }
    return cpu_read(a);
}

#define CPU_FETCH(a) bus_fetch(a)
#include "cpu6502.c"
