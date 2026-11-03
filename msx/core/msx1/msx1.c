/* msx1.c -- see msx1.h
 *
 * Timing: 3.579545 MHz, 228 Z80 cycles per line, 262 lines (60 Hz); the
 * VDP's frame interrupt at the end of line 191. Sound is made up to the
 * current cycle before every PSG or SCC write, so sample playback through
 * the volume registers keeps its timing.
 */
#include <string.h>
#include "msx1.h"
#include "tms9918.h"
#include "v9938.h"
#include "ay8910.h"
#include "scc.h"
#include "Z80.h"

#ifdef PBC_ON_DEVICE
#define MSX_RAM __attribute__((section(".time_critical.msx1")))
#else
#define MSX_RAM
#endif

#define CLOCK        3579545
#define LINE_CYCLES  228
#define LINES        262
#define FRAME_CYCLES (LINE_CYCLES * LINES)

/* ---- state --------------------------------------------------------------- */

uint8_t msx_keys[11];
uint8_t msx_joy[2];

uint8_t *msx_readmap[8];                /* redcode reads through it (Z80_PBC_MAP) */

static Z80 cpu;
static uint8_t ram[0x20000];            /* MSX1: 64 KB in slot 3; MSX2: the 128 KB mapper */
static uint8_t empty[0x2000];
static uint8_t scc_page[0x2000];        /* 8000h-9FFFh with the SCC switched in */

static const uint8_t *bios_main[4], *bios_logo;
static const uint8_t *cart;
static uint32_t cart_size, cart_banks;  /* banks of 8 KB, a power of two */
static int mapper;
static const uint8_t *plain[8];         /* plain ROM: what each 8 KB page shows */
static uint8_t bank[4];
static bool scc_on;

static uint8_t slots;                   /* PPI port A: primary slot per page */
static uint8_t ppi_c;                   /* PPI port C: keyboard row, ... */

/* MSX2: slot 3 expanded -- 3-0 and 3-1 the sub-ROM at 0000h, 3-2 the RAM
 * mapper, 3-3 the disk ROM at 4000h */
static bool msx2;
static const uint8_t *sub_rom[2], *disk_rom;
static uint8_t sub3;                    /* secondary slot register of slot 3 */
static uint8_t mapper_seg[4];           /* ports FCh-FFh */
static uint8_t ffpage[0x2000];          /* E000h-FFFFh of slot 3 without RAM */
static uint32_t frame_total;

/* RP5C01 clock: blocks 1-3 of 13 nibbles; block 0 is the time */
static uint8_t rtc_sel, rtc_mode, rtc_mem[4][13];

/* the disk in drive A: (MSX2 with a disk ROM) */
static msx_disk_io_fn disk_io;
static uint32_t disk_sectors;
static bool disk_changed;

static ay8910_t psg;
static scc_t scc;

static int line_start;                  /* cycle of the current line in the frame */
static int debt;                        /* cycles the last line ran over */

/* sound up to a cycle of the frame */
static int32_t mix[1024];
static int frame_samples, made;

/* ---- memory -------------------------------------------------------------- */

static void sound_to(int cycle);
static int now(void);

static const uint8_t *cart_bank(int b)
{
    return cart + ((uint32_t)(b & (cart_banks - 1)) << 13) % cart_size;
}

static void scc_page_fill(void)
{
    memcpy(scc_page, cart_bank(bank[2]), 0x1800);
    for (int i = 0x1800; i < 0x2000; i++)
        scc_page[i] = scc_read(&scc, (uint16_t)i);
}

static const uint8_t *slot1_page(int p)
{
    if (!cart)
        return empty;
    if (mapper == MAP_PLAIN)
        return plain[p] ? plain[p] : empty;
    if (p < 2 || p > 5)
        return empty;
    if (mapper == MAP_ASCII16) {
        int b = bank[(p - 2) >> 1] * 2 + (p & 1);
        return cart_bank(b);
    }
    if (p == 4 && scc_on)
        return scc_page;
    return cart_bank(bank[p - 2]);
}

static inline uint8_t *mapper_ram(int page16)
{
    return ram + (uint32_t)(mapper_seg[page16] & 7) * 0x4000;
}

static const uint8_t *slot3_page(int p)
{
    if (!msx2)
        return ram + p * 0x2000;
    switch ((sub3 >> ((p >> 1) * 2)) & 3) {
    case 0: case 1: return p < 2 && sub_rom[p] ? sub_rom[p] : empty;
    case 2:         return mapper_ram(p >> 1) + (p & 1) * 0x2000;
    default:        return (p == 2 || p == 3) && disk_rom ? disk_rom + (p - 2) * 0x2000 : empty;
    }
}

static void map_update(void)
{
    for (int p = 0; p < 8; p++) {
        int slot = (slots >> ((p >> 1) * 2)) & 3;
        const uint8_t *m;
        switch (slot) {
        case 0:  m = p < 4 ? bios_main[p] : p == 4 && bios_logo ? bios_logo : empty; break;
        case 1:  m = slot1_page(p); break;
        case 3:  m = slot3_page(p); break;
        default: m = empty; break;
        }
        msx_readmap[p] = (uint8_t *)m;
    }
    /* MSX2: FFFFh of slot 3 reads back the secondary slot register,
     * inverted (the RAM byte under it is out of reach) */
    if (msx2 && (slots >> 6) == 3) {
        uint8_t inv = (uint8_t)~sub3;
        if (((sub3 >> 6) & 3) == 2) {
            mapper_ram(3)[0x3fff] = inv;
        } else {
            memcpy(ffpage, msx_readmap[7], 0x2000);
            ffpage[0x1fff] = inv;
            msx_readmap[7] = ffpage;
        }
    }
}

static void cart_write(int a, int v)
{
    switch (mapper) {
    case MAP_KONAMI:
        if (a >= 0x6000 && a < 0xc000) {
            bank[(a - 0x4000) >> 13] = (uint8_t)v;
            map_update();
        }
        break;
    case MAP_KONAMI_SCC:
        if ((a & 0xf800) == 0x9800 && scc_on) {
            sound_to(now());
            scc_write(&scc, (uint16_t)a, (uint8_t)v);
            scc_page[0x1800 | (a & 0x7ff)] = scc_read(&scc, (uint16_t)a);
            /* the register area mirrors every 256 bytes */
            for (int i = 0x1800 + (a & 0xff); i < 0x2000; i += 0x100)
                scc_page[i] = scc_read(&scc, (uint16_t)i);
            return;
        }
        if ((a & 0x1800) == 0x1000 && a >= 0x4000 && a < 0xc000) {
            int i = (a - 0x4000) >> 13;
            bank[i] = (uint8_t)v;
            if (i == 2)
                scc_on = (v & 0x3f) == 0x3f;
            if (scc_on)
                scc_page_fill();
            map_update();
        }
        break;
    case MAP_ASCII8:
        if (a >= 0x6000 && a < 0x8000) {
            bank[(a >> 11) & 3] = (uint8_t)v;
            map_update();
        }
        break;
    case MAP_ASCII16:
        if (a >= 0x6000 && a < 0x7800 && !(a & 0x0800)) {
            bank[(a >> 12) & 1] = (uint8_t)v;
            map_update();
        }
        break;
    }
}

MSX_RAM void msx_write(int a, int v)
{
    int slot = (slots >> ((a >> 14) * 2)) & 3;
    if (slot == 3) {
        if (!msx2) {
            ram[a] = (uint8_t)v;
        } else if (a == 0xffff) {
            sub3 = (uint8_t)v;
            map_update();
        } else if (((sub3 >> ((a >> 14) * 2)) & 3) == 2) {
            mapper_ram(a >> 14)[a & 0x3fff] = (uint8_t)v;
        }
    } else if (slot == 1 && cart) {
        cart_write(a, v);
    }
}

/* ---- sound --------------------------------------------------------------- */

static void sound_to(int cycle)
{
    if (!frame_samples)
        return;
    int want = (int)((int64_t)cycle * frame_samples / FRAME_CYCLES);
    if (want > frame_samples) want = frame_samples;
    if (want > made) {
        ay_render(&psg, mix + made, want - made);
        scc_render(&scc, mix + made, want - made);
        made = want;
    }
}

static int now(void)
{
    return line_start + (int)cpu.cycles;
}

/* ---- I/O ----------------------------------------------------------------- */

static uint8_t joy_port(void)
{
    int j = msx_joy[(psg.reg[15] >> 6) & 1];
    uint8_t v = 0x3f;
    if (j & MSX_JOY_UP)    v &= ~0x01;
    if (j & MSX_JOY_DOWN)  v &= ~0x02;
    if (j & MSX_JOY_LEFT)  v &= ~0x04;
    if (j & MSX_JOY_RIGHT) v &= ~0x08;
    if (j & MSX_JOY_A)     v &= ~0x10;
    if (j & MSX_JOY_B)     v &= ~0x20;
    return v | 0x40;                    /* bit 7: cassette in (0) */
}

static void irq_update(void)
{
    z80_int(&cpu, msx2 ? v99_irq() : tms_irq());
}

/* RP5C01: the time runs from 1 January 2026, 12:00, with the frames (both
 * link devices see the same) */
static uint8_t rtc_read(void)
{
    int block = rtc_mode & 3;
    if (rtc_sel == 13)
        return rtc_mode | 0xf0;
    if (rtc_sel > 13)
        return 0xff;
    if (block == 0) {
        uint32_t s = 12 * 3600 + frame_total / 60;
        uint32_t sec = s % 60, min = s / 60 % 60, hour = s / 3600 % 24, day = 1 + s / 86400 % 28;
        static const uint8_t fixed[13] = { 0, 0, 0, 0, 0, 0, 4, 0, 0, 1, 0, 6, 4 };   /* Thursday, January, 2026 = 1980 + 46 */
        uint8_t v;
        switch (rtc_sel) {
        case 0: v = sec % 10; break;
        case 1: v = sec / 10; break;
        case 2: v = min % 10; break;
        case 3: v = min / 10; break;
        case 4: v = hour % 10; break;
        case 5: v = hour / 10; break;
        case 7: v = day % 10; break;
        case 8: v = day / 10; break;
        default: v = fixed[rtc_sel]; break;
        }
        return v | 0xf0;
    }
    return rtc_mem[block][rtc_sel] | 0xf0;
}

static void rtc_write(uint8_t v)
{
    if (rtc_sel == 13)
        rtc_mode = v & 15;
    else if (rtc_sel < 13 && (rtc_mode & 3))
        rtc_mem[rtc_mode & 3][rtc_sel] = v & 15;
}

static zuint8 io_in(void *ctx, zuint16 port)
{
    (void)ctx;
    if (msx2) {
        switch (port & 0xff) {
        case 0x98: return v99_read_data();
        case 0x99: {
            v99_hpos = (int)cpu.cycles % LINE_CYCLES;
            uint8_t s = v99_read_status();
            irq_update();
            return s;
        }
        case 0xb5: return rtc_read();
        case 0xfc: case 0xfd: case 0xfe: case 0xff: return mapper_seg[(port & 0xff) - 0xfc] | 0xf8;
        }
    }
    switch (port & 0xff) {
    case 0x98: return tms_read_data();
    case 0x99: { uint8_t s = tms_read_status(); irq_update(); return s; }
    case 0xa2: return ay_read_data(&psg, joy_port);
    case 0xa8: return slots;
    case 0xa9: return (ppi_c & 15) < 11 ? (uint8_t)~msx_keys[ppi_c & 15] : 0xff;
    case 0xaa: return ppi_c;
    default:   return 0xff;
    }
}

static void io_out(void *ctx, zuint16 port, zuint8 v)
{
    (void)ctx;
    if (msx2) {
        switch (port & 0xff) {
        case 0x98: v99_write_data(v); return;
        case 0x99: v99_write_ctrl(v); irq_update(); return;
        case 0x9a: v99_write_palette(v); return;
        case 0x9b: v99_write_indirect(v); irq_update(); return;
        case 0xb4: rtc_sel = v & 15; return;
        case 0xb5: rtc_write(v); return;
        case 0xfc: case 0xfd: case 0xfe: case 0xff:
            mapper_seg[(port & 0xff) - 0xfc] = v & 7;
            map_update();
            return;
        }
    }
    switch (port & 0xff) {
    case 0x98: tms_write_data(v); break;
    case 0x99: tms_write_ctrl(v); irq_update(); break;
    case 0xa0: ay_write_addr(&psg, v); break;
    case 0xa1: sound_to(now()); ay_write_data(&psg, v); break;
    case 0xa8: slots = v; map_update(); break;
    case 0xaa: ppi_c = v; break;
    case 0xab:                          /* bit set/reset on port C */
        if (!(v & 0x80)) {
            int b = (v >> 1) & 7;
            ppi_c = (uint8_t)((v & 1) ? ppi_c | (1 << b) : ppi_c & ~(1 << b));
        }
        break;
    }
}

static zuint8 rd(void *ctx, zuint16 a)
{
    (void)ctx;
    return msx_readmap[a >> 13][a & 0x1fff];
}

static void wr(void *ctx, zuint16 a, zuint8 v)
{
    (void)ctx;
    msx_write(a, v);
}

static zuint8 inta(void *ctx, zuint16 a)
{
    (void)ctx;
    (void)a;
    return 0xff;                        /* IM 1; an RST 38h in IM 0 */
}


/* ---- disk ------------------------------------------------------------------
 *
 * The disk ROM's driver entries (4010h DSKIO, 4013h DSKCHG, 4016h GETDPB,
 * 4019h CHOICE, 401Ch DSKFMT, 401Fh MTOFF) hold the trap opcode 64h (its
 * copy in flash is patched so); the emulator does the work and returns.
 * Works with any disk ROM: these entries are the same in all of them. */

static void wr8(uint16_t a, uint8_t v) { msx_write(a, v); }
static uint8_t rd8(uint16_t a) { return msx_readmap[a >> 13][a & 0x1fff]; }

/* the drive parameter block from the boot sector (or by media byte) */
static void fill_dpb(uint16_t hl, uint8_t media)
{
    static uint8_t boot[512];
    int spc = 2, res = 1, fats = 2, root = 112, fatsz = 3;
    uint32_t total = 1440;
    if (disk_io && disk_io(false, 0, boot)) {
        int bps = boot[11] | boot[12] << 8;
        if (bps == 512 && boot[13] && boot[16] && boot[22]) {
            spc = boot[13];
            res = boot[14] | boot[15] << 8;
            fats = boot[16];
            root = boot[17] | boot[18] << 8;
            total = boot[19] | boot[20] << 8;
            fatsz = boot[22];
            media = boot[21];
        } else if (media == 0xf8) {
            total = 720; fatsz = 2;
        }
    }
    int firdir = res + fats * fatsz;
    int firrec = firdir + (root * 32 + 511) / 512;
    int maxclus = (int)(total - (uint32_t)firrec) / spc + 1;
    int shift = 1;
    while ((1 << (shift - 1)) < spc) shift++;
    uint8_t d[18] = {
        media, 0x00, 0x02, 0x0f, 0x04, (uint8_t)(spc - 1), (uint8_t)shift,
        (uint8_t)res, (uint8_t)(res >> 8), (uint8_t)fats, (uint8_t)(root > 254 ? 254 : root),
        (uint8_t)firrec, (uint8_t)(firrec >> 8), (uint8_t)maxclus, (uint8_t)(maxclus >> 8),
        (uint8_t)fatsz, (uint8_t)firdir, (uint8_t)(firdir >> 8),
    };
    for (int i = 0; i < 18; i++)
        wr8((uint16_t)(hl + 1 + i), d[i]);
}

static zuint8 disk_hook(void *ctx, zuint16 pc)
{
    (void)ctx;
    if (!disk_rom || msx_readmap[2] != disk_rom || pc < 0x4010 || pc > 0x401f || (pc - 0x4010) % 3)
        return 0x64;                    /* an ordinary LD H,H */
    uint8_t a = Z80_A(cpu), f = Z80_F(cpu);
    f &= (uint8_t)~1;                   /* carry: error */
    switch (pc) {
    case 0x4010: {                      /* DSKIO */
        static uint8_t buf[512];
        bool write = Z80_F(cpu) & 1;
        int n = Z80_B(cpu);
        uint32_t sec = Z80_DE(cpu);
        uint16_t hl = Z80_HL(cpu);
        int err = -1;
        if (a != 0 || !disk_io)
            err = 2;                    /* not ready */
        for (; err < 0 && n > 0; n--, sec++, hl = (uint16_t)(hl + 512)) {
            if (sec >= disk_sectors) { err = 8; break; }     /* record not found */
            if (write) {
                for (int i = 0; i < 512; i++) buf[i] = rd8((uint16_t)(hl + i));
                if (!disk_io(true, sec, buf)) err = 0;       /* write protected */
            } else {
                if (!disk_io(false, sec, buf)) { err = 4; break; }
                for (int i = 0; i < 512; i++) wr8((uint16_t)(hl + i), buf[i]);
            }
        }
        Z80_B(cpu) = (uint8_t)n;
        if (err >= 0) { a = (uint8_t)err; f |= 1; }
        break;
    }
    case 0x4013:                        /* DSKCHG */
        if (a != 0 || !disk_io) { a = 2; f |= 1; break; }
        if (disk_changed) {
            fill_dpb(Z80_HL(cpu), Z80_C(cpu));
            Z80_B(cpu) = 0xff;
            disk_changed = false;
        } else {
            Z80_B(cpu) = 1;
        }
        break;
    case 0x4016:                        /* GETDPB */
        fill_dpb(Z80_HL(cpu), Z80_B(cpu));
        break;
    case 0x4019:                        /* CHOICE: no formatting choices */
        Z80_HL(cpu) = 0;
        break;
    case 0x401c:                        /* DSKFMT */
        a = 16; f |= 1;
        break;
    default:                            /* MTOFF */
        break;
    }
    Z80_A(cpu) = a;
    Z80_F(cpu) = f;
    /* RET */
    uint16_t sp = Z80_SP(cpu);
    Z80_PC(cpu) = (uint16_t)(rd8(sp) | rd8((uint16_t)(sp + 1)) << 8);
    Z80_SP(cpu) = (uint16_t)(sp + 2);
    return Z80_HOOK;
}

void msx2_patch_disk_rom(uint8_t *rom16k)
{
    /* C-DISK (C-BIOS) calls its own DSKIO directly (PHYDIO, booting, the
     * BDOS): those routines jump to the trapped entries as well */
    static const char id[] = "C-DISK";
    bool cdisk = false;
    for (int i = 0; i + 6 <= 0x4000 && !cdisk; i++)
        cdisk = memcmp(rom16k + i, id, 6) == 0;
    for (int a = 0x10; a <= 0x1f; a += 3) {
        int t = rom16k[a + 1] | rom16k[a + 2] << 8;
        if (cdisk && rom16k[a] == 0xc3 && t >= 0x4030 && t < 0x7ffd) {
            rom16k[t - 0x4000] = 0xc3;              /* JP 40xxh */
            rom16k[t - 0x4000 + 1] = (uint8_t)a;
            rom16k[t - 0x4000 + 2] = 0x40;
        }
        rom16k[a] = 0x64;
    }
}

void msx2_insert_disk(msx_disk_io_fn io, uint32_t sectors)
{
    disk_io = io;
    disk_sectors = sectors;
    disk_changed = true;
}

/* ---- mapper guess -------------------------------------------------------- */

/* Counts the "LD (nnnn),A" instructions that write to each mapper's bank
 * registers. Konami writes 6000h/8000h/A000h, Konami SCC 5000h/7000h/
 * 9000h/B000h, ASCII 8 KB 6000h/6800h/7000h/7800h, ASCII 16 KB 6000h/
 * 7000h (often 77FFh). */
static int guess_mapper(const uint8_t *rom, uint32_t size)
{
    int konami = 0, scc_n = 0, a8 = 0, a16 = 0, both = 0;
    for (uint32_t i = 0; i + 2 < size; i++) {
        if (rom[i] != 0x32)
            continue;
        int a = rom[i + 1] | rom[i + 2] << 8;
        switch (a) {
        case 0x8000: case 0xa000: konami++; break;
        case 0x5000: case 0x9000: case 0xb000: scc_n++; break;
        case 0x6800: case 0x7800: a8++; break;
        case 0x77ff: a16++; break;
        case 0x6000: case 0x7000: both++; break;
        }
    }
    if (konami + scc_n > a8 + a16 + both / 2)
        return scc_n > konami ? MAP_KONAMI_SCC : MAP_KONAMI;
    if (a8 > 0)
        return MAP_ASCII8;
    if (a16 > 0 || both > 0)
        return MAP_ASCII16;
    return MAP_KONAMI_SCC;              /* common for Konami's larger games */
}

const char *msx1_mapper_name(int m)
{
    static const char *const n[] = { "plain ROM", "Konami", "Konami SCC", "ASCII 8K", "ASCII 16K" };
    return m >= 0 && m <= MAP_ASCII16 ? n[m] : "?";
}

int msx_key_of_char(char c)
{
    if (c >= '0' && c <= '7') return MSX_KEY(0, c - '0');
    if (c == '8') return MSX_KEY(1, 0);
    if (c == '9') return MSX_KEY(1, 1);
    if (c >= 'a' && c <= 'z') c = (char)(c - 32);
    if (c == 'A') return MSX_KEY(2, 6);
    if (c == 'B') return MSX_KEY(2, 7);
    if (c >= 'C' && c <= 'Z') {
        int i = c - 'C';                /* C..J row 3, K..R row 4, S..Z row 5 */
        return MSX_KEY(3 + i / 8, i % 8);
    }
    return -1;
}

/* ---- interface ----------------------------------------------------------- */

void msx1_init(const uint8_t *const main_rom[4], const uint8_t *logo_rom)
{
    msx2 = false;
    memcpy(bios_main, main_rom, sizeof bios_main);
    bios_logo = logo_rom;
    memset(empty, 0xff, sizeof empty);
    ay_init(&psg, CLOCK / 2, 22222);
    scc_init(&scc, CLOCK, 22222);
}

void msx2_init(const uint8_t *const main_rom[4], const uint8_t *logo_rom,
               const uint8_t *const sub[2], const uint8_t *disk)
{
    msx1_init(main_rom, logo_rom);
    msx2 = true;
    sub_rom[0] = sub[0];
    sub_rom[1] = sub[1];
    disk_rom = disk;
}

bool msx_is_msx2(void)
{
    return msx2;
}

int msx1_insert(const uint8_t *rom, uint32_t size)
{
    cart = NULL;
    if (!rom || size < 0x2000)
        return -1;
    cart = rom;
    cart_size = size;
    cart_banks = 1;
    while (cart_banks * 0x2000 < size)
        cart_banks <<= 1;
    memset(plain, 0, sizeof plain);
    bool ab0 = rom[0] == 'A' && rom[1] == 'B';
    bool ab4 = size > 0x4000 && rom[0x4000] == 'A' && rom[0x4001] == 'B';
    if (size > 0x10000) {
        mapper = guess_mapper(rom, size);
    } else {
        mapper = MAP_PLAIN;
        uint32_t base = 0x4000;
        if (size <= 0x4000) {
            int init = rom[2] | rom[3] << 8;
            if (ab0 && init >= 0x8000 && init < 0xc000)
                base = 0x8000;
        } else if (size > 0x8000 && !ab0 && ab4) {
            base = 0;                   /* 48/64 KB laid out from 0000h */
        }
        for (int p = 0; p < 8; p++) {
            int32_t off = (int32_t)(p * 0x2000) - (int32_t)base;
            if (size <= 0x2000 && off >= 0 && off < 0x4000)
                off &= 0x1fff;          /* 8 KB: mirrored in its 16 KB */
            if (off >= 0 && (uint32_t)off < size)
                plain[p] = rom + off;
        }
        if (!ab0 && !ab4 && size <= 0x8000)
            return -1;
    }
    return mapper;
}

void msx1_reset(void)
{
    memset(ram, 0, sizeof ram);
    memset(msx_keys, 0, sizeof msx_keys);
    tms_reset();
    v99_reset();
    sub3 = 0;
    for (int i = 0; i < 4; i++)
        mapper_seg[i] = (uint8_t)(3 - i);
    memset(ffpage, 0xff, sizeof ffpage);
    rtc_sel = rtc_mode = 0;
    memset(rtc_mem, 0, sizeof rtc_mem);
    frame_total = 0;
    ay_reset(&psg);
    scc_init(&scc, CLOCK, 22222);
    for (int i = 0; i < 4; i++)
        bank[i] = (uint8_t)(mapper == MAP_ASCII16 ? 0 : i);
    scc_on = false;
    slots = 0;
    ppi_c = 0;
    map_update();

    memset(&cpu, 0, sizeof cpu);
    cpu.fetch_opcode = rd;
    cpu.fetch = rd;
    cpu.read = rd;
    cpu.write = wr;
    cpu.in = io_in;
    cpu.out = io_out;
    cpu.inta = inta;
    cpu.hook = msx2 && disk_rom ? disk_hook : NULL;
    cpu.options = Z80_MODEL_ZILOG_NMOS;
    z80_power(&cpu, Z_TRUE);
    debt = 0;
}

/* MSX2: the frame through the V9938, lines as RGB565 */
static MSX_RAM void frame_v99(msx_line565_fn line)
{
    static uint16_t px[V99_WIDTH] __attribute__((aligned(4)));   /* written as words */
    v99_frame_start();
    int shown = v99_lines();
    for (int y = 0; y < LINES; y++) {
        line_start = y * LINE_CYCLES;
        int want = LINE_CYCLES - debt;
        int ran = want > 0 ? (int)z80_run(&cpu, (zusize)want) : 0;
        debt = ran - want;
        if (debt < 0) debt = 0;
        if (y < shown) {
            v99_line(y, px);
            irq_update();
            if (line)
                line(y, shown, px);
        }
        if (y == shown - 1) {
            v99_vblank();
            irq_update();
        }
    }
}

msx_line565_fn msx_line565;

MSX_RAM void msx1_frame(msx_line_fn line, int16_t *audio, int samples)
{
    static uint8_t px[TMS_WIDTH];
    frame_total++;
    if (samples > (int)(sizeof mix / sizeof mix[0]))
        samples = sizeof mix / sizeof mix[0];
    frame_samples = samples;
    made = 0;
    memset(mix, 0, (size_t)samples * sizeof mix[0]);

    if (msx2) frame_v99(msx_line565); else
    for (int y = 0; y < LINES; y++) {
        line_start = y * LINE_CYCLES;
        int want = LINE_CYCLES - debt;
        int ran = want > 0 ? (int)z80_run(&cpu, (zusize)want) : 0;
        debt = ran - want;
        if (debt < 0) debt = 0;
        if (y < TMS_LINES && line) {
            tms_line(y, px);
            line(y, px, tms_backdrop());
        }
        if (y == TMS_LINES - 1) {
            tms_vblank();
            irq_update();
        }
    }
    sound_to(FRAME_CYCLES);

    /* mix: PSG (unipolar) and SCC; the DC goes in a one-pole filter */
    static int32_t dc;
    for (int i = 0; i < samples; i++) {
        int32_t v = mix[i];
        dc += (v * 256 - dc) >> 10;
        v -= dc >> 8;
        audio[i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
    }
    frame_samples = 0;
}

uint8_t *msx1_ram(void)
{
    return ram;
}
