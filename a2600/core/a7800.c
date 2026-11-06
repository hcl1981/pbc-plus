/* a7800.c -- see a7800.h
 *
 * Time runs in MARIA clocks (7.16 MHz, 454 a line): a CPU cycle takes 4 of
 * them, 6 for the TIA and the RIOT. On a shown line MARIA first does its DMA
 * -- fetching the display list and the graphics into the line, which halts
 * the CPU for as long -- and the CPU then has the rest of the line.
 */
#include <string.h>
#include "a7800.h"
#include "a2600.h"
#include "tia.h"
#include "pokey.h"

#ifdef PBC_ON_DEVICE
#define A78_RAM __attribute__((section(".time_critical.a7800")))
#else
#define A78_RAM
#endif

#define LINE_CLK   454
#define FIRST_SHOWN 16                  /* MARIA's first DMA line (NTSC and PAL) */

/* ---- the machine ------------------------------------------------------------------- */

static uint8_t ram[0x1000];             /* 1800h-27FFh */
static uint8_t riot_ram[0x80];
static uint8_t cart_ram[0x4000];        /* SuperGame RAM at 4000h */
static uint8_t maria[0x20];             /* 20h-3Fh */
static const uint8_t *rom;
static uint32_t rom_size;
static const uint8_t *page[12];         /* 4000h-FFFFh, 4 KB pages; NULL: nothing there */
static uint8_t *wpage[12];              /* writable ones (cartridge RAM) */
static bool supergame, sg_ram, sg_rom4000, sg_bank6, pal;
static bool absolute;                   /* F-18 Hornet */
static bool activision;                 /* Double Dragon, Rampage */
static int sg_banks, sg_base;
static bool pokey4000;                  /* POKEY at 4000h-7FFFh */
static uint16_t pokey_lo;               /* or at 450h / 440h (0: none) */
static int pokey_acc;                   /* MARIA clocks not yet run by POKEY */
static const uint8_t open_bus[0x1000] = { [0 ... 0xfff] = 0xff };

static int clk;                         /* clocks into the line */
static int line_no, lines_total;
static uint32_t frame_clk;              /* clocks into the frame, for the timer */
static bool wsync, dma_on, nmi_pending;

/* RIOT */
static uint8_t swacnt, swbcnt, swcha_out;
static uint32_t tim_start;
static int tim_shift;
static uint8_t tim_val;

/* MARIA's display list state */
static uint16_t dll, dl;
static int offset;
static bool h16, h8, dli, wm;

/* sound */
#define MAX_LINES 313
static int16_t tia_audio[MAX_LINES * 2];
static int audio_n;

/* ---- cartridge ----------------------------------------------------------------------- */

static const uint8_t *bank_ptr(int b)
{
    uint32_t off = (uint32_t)b * 0x4000;
    return off + 0x4000 <= rom_size ? rom + off : open_bus;
}

static void map_16k(int slot, const uint8_t *p, uint8_t *w)
{
    for (int i = 0; i < 4; i++) {
        page[slot * 4 + i] = p == open_bus ? open_bus : p + i * 0x1000;
        wpage[slot * 4 + i] = w ? w + i * 0x1000 : NULL;
    }
}

static void sg_select(int v)
{
    map_16k(1, bank_ptr(sg_base + (v & (sg_banks - 1))), NULL);
}

/* 8 KB of the image at offset `off` to address a (a multiple of 4 KB) */
static void map_8k(uint16_t a, uint32_t off)
{
    for (int i = 0; i < 2; i++) {
        uint32_t o = off + (uint32_t)i * 0x1000;
        page[(a >> 12) - 4 + i] = o + 0x1000 <= rom_size ? rom + o : open_bus;
        wpage[(a >> 12) - 4 + i] = NULL;
    }
}

/* Activision (128 KB): 16 KB bank n at A000h-DFFFh, chosen by a write to
 * FF80h-FF8Fh (n: the low three address bits); around it fixed 8 KB parts
 * of banks 6 and 7 (map_cart). */
static void activision_select(int n)
{
    map_8k(0xa000, (uint32_t)n * 0x4000);
    map_8k(0xc000, (uint32_t)n * 0x4000 + 0x2000);
}

bool a7800_insert(const uint8_t *r, uint32_t n)
{
    uint16_t type = 0;
    pal = false;
    pokey4000 = false;
    pokey_lo = 0;
    absolute = activision = false;
    if (n >= 128 && memcmp(r + 1, "ATARI7800", 9) == 0) {
        type = (uint16_t)(r[53] << 8 | r[54]);
        pal = r[57] & 1;
        /* type bits 8, 9: Absolute and Activision bank switching */
        absolute = type & 0x0100;
        activision = type & 0x0200;
        /* cartridge type bits 0, 6, 10: a POKEY at 4000h, 450h, 440h */
        pokey4000 = type & 0x0001;
        pokey_lo = (type & 0x0040) ? 0x450 : (type & 0x0400) ? 0x440 : 0;
        r += 128;
        n -= 128;
    }
    if (n < 0x1000)
        return false;
    rom = r;
    rom_size = n;
    supergame = !absolute && !activision && ((type & 0x02) || n > 0xc000);
    sg_ram = type & 0x04;
    sg_rom4000 = type & 0x08;
    sg_bank6 = type & 0x10;
    sg_base = sg_rom4000 ? 1 : 0;
    sg_banks = 1;
    while (sg_banks * 2 <= (int)((n / 0x4000) - (uint32_t)sg_base))
        sg_banks *= 2;
    return true;
}

bool a7800_pal(void) { return pal; }
uint8_t *a7800_ram(void) { return ram; }

static void map_cart(void)
{
    if (absolute) {
        /* 64 KB: bank 0 or 1 at 4000h (a write of 1 or 2 to 8000h), 2 and 3 fixed */
        map_16k(0, bank_ptr(0), NULL);
        map_16k(1, bank_ptr(2), NULL);
        map_16k(2, bank_ptr(3), NULL);
        return;
    }
    if (activision) {
        map_8k(0x4000, 0x1a000);
        map_8k(0x6000, 0x18000);
        map_8k(0x8000, 0x1e000);
        activision_select(0);
        map_8k(0xe000, 0x1c000);
        return;
    }
    if (!supergame) {
        /* flat: the image ends at FFFFh */
        for (int i = 0; i < 12; i++) {
            uint32_t a = 0x4000 + (uint32_t)i * 0x1000;
            uint32_t start = 0x10000 - rom_size;
            page[i] = a >= start ? rom + (a - start) : open_bus;
            wpage[i] = NULL;
        }
        return;
    }
    int last = (int)(rom_size / 0x4000) - 1;
    if (sg_ram)
        map_16k(0, cart_ram, cart_ram);
    else if (sg_rom4000)
        map_16k(0, bank_ptr(0), NULL);
    else if (sg_bank6)
        map_16k(0, bank_ptr(sg_base + 6), NULL);
    else
        map_16k(0, open_bus, NULL);
    sg_select(0);
    map_16k(2, bank_ptr(last), NULL);
}

/* ---- the RIOT ------------------------------------------------------------------------ */

/* the timer runs at 1.19 MHz: a tick every 6 clocks */
static inline uint32_t riot_ticks(void) { return (frame_clk + (uint32_t)clk - tim_start) / 6; }

static uint8_t riot_read(uint16_t a)
{
    if ((a & 0x280) == 0x80)            /* 0480h-04FFh: RAM */
        return riot_ram[a & 0x7f];
    switch (a & 0x07) {
    case 0: {                           /* SWCHA: joysticks, low active */
        uint8_t v = 0xff;
        for (int p = 0; p < 2; p++) {
            uint8_t j = a26_joy[p], sh = p ? 0 : 4;
            if (j & A26_UP)    v &= (uint8_t)~(0x1 << sh);
            if (j & A26_DOWN)  v &= (uint8_t)~(0x2 << sh);
            if (j & A26_LEFT)  v &= (uint8_t)~(0x4 << sh);
            if (j & A26_RIGHT) v &= (uint8_t)~(0x8 << sh);
        }
        return (uint8_t)((v & ~swacnt) | (swcha_out & swacnt));
    }
    case 1: return swacnt;
    case 2: {                           /* SWCHB: reset, select, pause, difficulties */
        uint8_t v = 0xff;
        if (a26_switches & A26_RESET)  v &= ~0x01;
        if (a26_switches & A26_SELECT) v &= ~0x02;
        if (a26_switches & A26_BW)     v &= ~0x08;     /* PAUSE */
        if (!(a26_switches & A26_DIFF0)) v &= ~0x40;
        if (!(a26_switches & A26_DIFF1)) v &= ~0x80;
        return v;
    }
    case 3: return swbcnt;
    case 4: case 6: {                   /* INTIM */
        /* as the real chip: the first step one tick after the write, then
         * one per interval; past zero one per tick */
        uint32_t t = riot_ticks();
        if (t == 0)
            return tim_val;
        uint32_t n = 1 + ((t - 1) >> tim_shift);
        if (n <= tim_val)
            return (uint8_t)(tim_val - n);
        return (uint8_t)(0xff - ((t - 1 - ((uint32_t)tim_val << tim_shift)) & 0xff));
    }
    default: {                          /* TIMINT */
        uint32_t t = riot_ticks();
        return t && 1 + ((t - 1) >> tim_shift) > tim_val ? 0x80 : 0x00;
    }
    }
}

static void riot_write(uint16_t a, uint8_t v)
{
    if ((a & 0x280) == 0x80) {
        riot_ram[a & 0x7f] = v;
        return;
    }
    if (a & 0x10) {                     /* TIM1T TIM8T TIM64T T1024T */
        static const int shift[4] = { 0, 3, 6, 10 };
        tim_shift = shift[a & 3];
        tim_val = v;
        tim_start = frame_clk + (uint32_t)clk;
        return;
    }
    switch (a & 3) {
    case 0: swcha_out = v; break;
    case 1: swacnt = v; break;
    case 3: swbcnt = v; break;
    }
}

/* ---- the bus -------------------------------------------------------------------------- */

static A78_RAM uint8_t a78_read(uint16_t a);
static A78_RAM void a78_write(uint16_t a, uint8_t v);

static inline uint8_t mem_read(uint16_t a)     /* MARIA's view: no I/O, no time */
{
    if (a >= 0x4000)
        return page[(a >> 12) - 4][a & 0xfff];
    if (a >= 0x1800)
        return a < 0x2000 ? ram[a - 0x1800] : ram[0x800 + (a & 0x7ff)];
    if ((a & 0xfc00) == 0 && (a & 0xff) >= 0x40)
        return ram[0x800 + (a & 0x1ff)];
    return 0xff;
}

static A78_RAM uint8_t a78_read(uint16_t a)
{
    if (a >= 0x4000) {
        clk += 4;
        if (pokey4000 && a < 0x8000)
            return pokey_read((uint8_t)a);
        return page[(a >> 12) - 4][a & 0xfff];
    }
    if (a >= 0x1800) {
        clk += 4;
        return a < 0x2000 ? ram[a - 0x1800] : ram[0x800 + (a & 0x7ff)];
    }
    if (a < 0x400) {
        uint8_t lo = a & 0xff;
        if ((a & 0x200) == 0 && lo >= 0x40) {        /* 40h-FFh, 140h-1FFh: RAM */
            clk += 4;
            return ram[0x800 + (a & 0x1ff)];
        }
        if ((a & 0x2e0) == 0) {                      /* TIA */
            clk += 6;
            switch (lo & 0x0f) {
            case 0x08: return (a26_joy[0] & A26_FIRE2) ? 0x80 : 0x00;   /* INPT0: right button */
            case 0x09: return (a26_joy[0] & A26_FIRE) ? 0x80 : 0x00;    /* INPT1: left button */
            case 0x0a: return (a26_joy[1] & A26_FIRE2) ? 0x80 : 0x00;
            case 0x0b: return (a26_joy[1] & A26_FIRE) ? 0x80 : 0x00;
            default:   return tia_read(lo, 0);
            }
        }
        if ((a & 0x2e0) == 0x20) {                   /* MARIA */
            clk += 4;
            if ((lo & 0x1f) == 0x08)                 /* MSTAT: bit 7 VBLANK */
                return (line_no < FIRST_SHOWN || line_no >= FIRST_SHOWN + 243) ? 0x80 : 0x00;
            return maria[lo & 0x1f];
        }
        if ((a & 0x380) == 0x280) {                  /* RIOT */
            clk += 6;
            return riot_read(a);
        }
    }
    if ((a & 0xff80) == 0x0480) {
        clk += 6;
        return riot_read(a);
    }
    if (pokey_lo && (a & 0xfff0) == pokey_lo) {
        clk += 4;
        return pokey_read((uint8_t)a);
    }
    clk += 4;
    return 0xff;
}

static A78_RAM void a78_write(uint16_t a, uint8_t v)
{
    if (a >= 0x4000) {
        clk += 4;
        int pg = (a >> 12) - 4;
        if (pokey4000 && a < 0x8000)
            pokey_write((uint8_t)a, v);
        else if (absolute && a >= 0x8000 && a < 0xc000) {
            if (v & 3)
                map_16k(0, bank_ptr((v & 1) ? 0 : 1), NULL);
        } else if (activision && (a & 0xfff0) == 0xff80)
            activision_select(a & 7);
        else if (wpage[pg])
            wpage[pg][a & 0xfff] = v;
        else if (supergame && a >= 0x8000 && a < 0xc000)
            sg_select(v);
        return;
    }
    if (a >= 0x1800) {
        clk += 4;
        if (a < 0x2000) ram[a - 0x1800] = v;
        else ram[0x800 + (a & 0x7ff)] = v;
        return;
    }
    if (a < 0x400) {
        uint8_t lo = a & 0xff;
        if ((a & 0x200) == 0 && lo >= 0x40) {
            clk += 4;
            ram[0x800 + (a & 0x1ff)] = v;
            return;
        }
        if ((a & 0x2e0) == 0) {                      /* TIA: sound */
            clk += 6;
            if ((lo & 0x1f) >= 0x15)
                tia_write(lo & 0x1f, v, 0);
            return;
        }
        if ((a & 0x2e0) == 0x20) {                   /* MARIA */
            clk += 4;
            int r = lo & 0x1f;
            if (r == 0x04)                           /* WSYNC */
                wsync = true;
            else
                maria[r] = v;
            return;
        }
        if ((a & 0x380) == 0x280) {
            clk += 6;
            riot_write(a, v);
            return;
        }
    }
    if ((a & 0xff80) == 0x0480) {
        clk += 6;
        riot_write(a, v);
        return;
    }
    if (pokey_lo && (a & 0xfff0) == pokey_lo) {
        clk += 4;
        pokey_write((uint8_t)a, v);
        return;
    }
    clk += 4;
}

/* the CPU, a second copy under its own names */
#define cpu       a78_cpu
#define cpu_reset a78_cpu_reset
#define cpu_step  a78_cpu_step
#define cpu_read  a78_read
#define cpu_write a78_write
#include "cpu6502.c"

static void nmi(void)
{
    a78_write((uint16_t)(0x100 | cpu.s--), (uint8_t)(cpu.pc >> 8));
    a78_write((uint16_t)(0x100 | cpu.s--), (uint8_t)cpu.pc);
    a78_write((uint16_t)(0x100 | cpu.s--), (uint8_t)((cpu.p | 0x20) & ~0x10));
    cpu.p |= 0x04;
    cpu.pc = (uint16_t)(a78_read(0xfffa) | a78_read(0xfffb) << 8);
    clk += 4;
}

/* ---- MARIA ------------------------------------------------------------------------- */

static uint8_t line_buf[A78_WIDTH];
static int dma_clk;                     /* this line's DMA */

static inline uint8_t colour(int palette, int c)
{
    return maria[0x01 + palette * 4 + (c - 1)];
}

/* one byte of graphics at 160-pixel column x */
static A78_RAM void draw_byte(int x, uint8_t b, int palette)
{
    int rm = maria[0x1c] & 3;
    bool kangaroo = maria[0x1c] & 4;
    uint8_t bg = maria[0x00];
#define PUT(px, c) do { int _x = (px) & 0x1ff; if (_x < A78_WIDTH) line_buf[_x] = (c); } while (0)
    if (rm == 0 && !wm) {               /* 160A: four pixels, two bits */
        for (int i = 0; i < 4; i++) {
            int v = (b >> (6 - 2 * i)) & 3;
            int px = ((x + i) & 0xff) * 2;
            if (v) { uint8_t c = colour(palette, v); PUT(px, c); PUT(px + 1, c); }
            else if (kangaroo) { PUT(px, bg); PUT(px + 1, bg); }
        }
    } else if (rm == 0) {               /* 160B: two pixels, two colour and two palette bits */
        for (int i = 0; i < 2; i++) {
            int v = (b >> (6 - 2 * i)) & 3;
            int p = (palette & 4) | ((b >> (2 - 2 * i)) & 3);
            int px = ((x + i) & 0xff) * 2;
            if (v) { uint8_t c = colour(p, v); PUT(px, c); PUT(px + 1, c); }
            else if (kangaroo) { PUT(px, bg); PUT(px + 1, bg); }
        }
    } else if (rm == 3 && !wm) {        /* 320A: eight pixels, one bit, colour 2 */
        uint8_t c = colour(palette, 2);
        for (int i = 0; i < 8; i++) {
            int px = (x & 0xff) * 2 + i;
            if (b & (0x80 >> i)) PUT(px, c);
            else if (kangaroo) PUT(px, bg);
        }
    } else if (rm == 3) {               /* 320C: four pixel pairs, colour 2, palette bits per pair */
        for (int i = 0; i < 4; i++) {
            int p = (palette & 4) | ((b >> (i < 2 ? 2 : 0)) & 3);
            int px = (x & 0xff) * 2 + 2 * i;
            if (b & (0x80 >> i)) { uint8_t c = colour(p, 2); PUT(px, c); PUT(px + 1, c); }
            else if (kangaroo) { PUT(px, bg); PUT(px + 1, bg); }
        }
    } else if (!wm) {                   /* 320D: eight pixels, one bit, palette bit 2 picks the colour */
        for (int i = 0; i < 8; i++) {
            int px = (x & 0xff) * 2 + i;
            int v = ((b >> (7 - i)) & 1) << 1 | ((palette >> (i & 1 ? 0 : 1)) & 1);
            if (v) PUT(px, colour(palette & 4, v));
            else if (kangaroo) PUT(px, bg);
        }
    } else {                            /* 320B: four pixel pairs of two bits */
        for (int i = 0; i < 4; i++) {
            int v = ((b >> (7 - i)) & 1) << 1 | ((b >> (3 - i)) & 1);
            int px = (x & 0xff) * 2 + 2 * i;
            if (v) { uint8_t c = colour(palette & 4, v); PUT(px, c); PUT(px + 1, c); }
            else if (kangaroo) { PUT(px, bg); PUT(px + 1, bg); }
        }
    }
#undef PUT
}

/* pixels per byte, in 160-wide columns */
static inline int byte_cols(void)
{
    int rm = maria[0x1c] & 3;
    return rm == 0 ? (wm ? 2 : 4) : 4;
}

static void load_dll(void)
{
    uint8_t b = mem_read(dll);
    offset = b & 15;
    h16 = b & 0x40;
    h8 = b & 0x20;
    dli = b & 0x80;
    if (dli)                            /* the interrupt comes as MARIA fetches the entry */
        nmi_pending = true;
    dl = (uint16_t)(mem_read((uint16_t)(dll + 1)) << 8 | mem_read((uint16_t)(dll + 2)));
    dll = (uint16_t)(dll + 3);
}

/* the DMA of a shown line: the display list into line_buf */
static A78_RAM void maria_line(void)
{
    memset(line_buf, maria[0x00], sizeof line_buf);
    dma_clk = 16;
    uint16_t p = dl;
    bool two = maria[0x1c] & 0x10;      /* character width 2 bytes */
    for (int guard = 0; guard < 64; guard++) {
        uint8_t b0 = mem_read(p), b1 = mem_read((uint16_t)(p + 1));
        if ((b1 & 0x5f) == 0) {
            if (b1 == 0) {               /* end of the list */
                dma_clk += 4;
                break;
            }
        }
        uint8_t hi, pw, hpos;
        bool ind = false;
        if ((b1 & 0x1f) == 0) {          /* five-byte header */
            wm = b1 & 0x80;
            ind = b1 & 0x20;
            hi = mem_read((uint16_t)(p + 2));
            pw = mem_read((uint16_t)(p + 3));
            hpos = mem_read((uint16_t)(p + 4));
            p = (uint16_t)(p + 5);
            dma_clk += 10;
        } else {
            pw = b1;
            hi = mem_read((uint16_t)(p + 2));
            hpos = mem_read((uint16_t)(p + 3));
            p = (uint16_t)(p + 4);
            dma_clk += 8;
        }
        int palette = pw >> 5;
        int width = (-(int)pw) & 0x1f;
        if (!width) width = 32;
        int cols = byte_cols();
        if (!ind) {
            uint16_t a = (uint16_t)(((hi + offset) & 0xff) << 8 | b0);
            if ((h16 && (a & 0x1000)) || (h8 && (a & 0x0800))) {
                dma_clk += 3 * width;
                continue;
            }
            for (int i = 0; i < width; i++)
                draw_byte(hpos + i * cols, mem_read((uint16_t)(a + i)), palette);
            dma_clk += 3 * width;
        } else {
            uint16_t list = (uint16_t)(hi << 8 | b0);
            uint16_t base = (uint16_t)(((maria[0x14] + offset) & 0xff) << 8);
            int x = hpos;
            for (int i = 0; i < width; i++) {
                uint8_t ch = mem_read((uint16_t)(list + i));
                uint16_t a = (uint16_t)(base + ch);
                if ((h16 && (a & 0x1000)) || (h8 && (a & 0x0800))) {
                    x += cols * (two ? 2 : 1);
                    continue;
                }
                draw_byte(x, mem_read(a), palette);
                x += cols;
                if (two) {
                    draw_byte(x, mem_read((uint16_t)(a + 1)), palette);
                    x += cols;
                }
            }
            dma_clk += (two ? 9 : 6) * width;
        }
    }
    /* the zone's lines count down; after its last one the next DLL entry */
    if (--offset < 0) {
        load_dll();
        dma_clk += 24;
    }
}

/* ---- the frame ------------------------------------------------------------------------ */

__attribute__((optimize("Os"))) void a7800_reset(void)     /* once: small, not fast */
{
    memset(ram, 0, sizeof ram);
    memset(riot_ram, 0, sizeof riot_ram);
    memset(cart_ram, 0, sizeof cart_ram);
    memset(maria, 0, sizeof maria);
    tia_reset();
    pokey_reset();
    pokey_acc = 0;
    swacnt = swbcnt = swcha_out = 0;
    tim_start = 0;
    tim_shift = 10;
    tim_val = 0;
    clk = 0;
    frame_clk = 0;
    line_no = 0;
    wsync = nmi_pending = false;
    wm = false;
    lines_total = pal ? 313 : 263;
    map_cart();
    cpu_reset();
}

A78_RAM void a7800_frame(a78_line_fn line, int16_t *audio, int samples)
{
    static uint8_t out[A78_WIDTH];
    audio_n = 0;
    tia_set_fire(0, a26_joy[0] & (A26_FIRE | A26_FIRE2));
    tia_set_fire(1, a26_joy[1] & (A26_FIRE | A26_FIRE2));
    frame_clk = 0;
    for (line_no = 0; line_no < lines_total; line_no++) {
        clk = 0;
        bool shown = line_no >= FIRST_SHOWN && line_no < FIRST_SHOWN + 243;
        if (line_no == FIRST_SHOWN - 1) {
            dma_on = (maria[0x1c] & 0x60) == 0x40;
            if (dma_on) {
                dll = (uint16_t)(maria[0x0c] << 8 | maria[0x10]);
                load_dll();
            }
        }
        if (shown && (maria[0x1c] & 0x60) == 0x40 && dma_on) {
            maria_line();
            clk = dma_clk < LINE_CLK - 40 ? dma_clk : LINE_CLK - 40;
        } else {
            memset(line_buf, maria[0x00], sizeof line_buf);
        }
        if (nmi_pending) {
            nmi_pending = false;
            nmi();
        }
        int y = line_no - FIRST_SHOWN - 1;
        if (line && y >= 0 && y < A78_LINES) {
            for (int i = 0; i < A78_WIDTH; i++)
                out[i] = line_buf[i] >> 1;
            line(y, out);
        }
        wsync = false;
        while (clk < LINE_CLK && !wsync)
            cpu_step();
        if (audio_n < MAX_LINES * 2 - 1) {
            int16_t *s = tia_audio + audio_n;
            int k = tia_audio_line(s);
            audio_n += k;
            /* POKEY: as many samples as the TIA (two a line), 1.79 MHz */
            for (int i = 0; (pokey4000 || pokey_lo) && i < k; i++) {
                pokey_acc += LINE_CLK / 2;
                int v = s[i] + (pokey_run(pokey_acc >> 2) * 500 >> 8);
                pokey_acc &= 3;
                s[i] = (int16_t)(v > 32767 ? 32767 : v);
            }
        }
        frame_clk += LINE_CLK;
    }
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
}
