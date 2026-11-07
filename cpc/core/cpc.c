/* cpc.c -- see cpc.h */
#include <stdio.h>
#include <string.h>
#include "cpc.h"
#include "fdc.h"
#include "tape.h"
#include "ay8910.h"
#include "Z80.h"

#ifdef PBC_ON_DEVICE
#define CPC_RAM __attribute__((section(".time_critical.cpc")))
#define CPC_TAB(n) __attribute__((section(".data.cpc_" n)))  /* a table read by RAM code */
#else
#define CPC_RAM
#define CPC_TAB(n)
#endif

#define LINE_T    256                   /* 64 us at 4 MHz */
#define LINES     312
#define FRAME_T   (LINE_T * LINES)
#define AUDIO_MAX_N 1024

uint8_t cpc_keys[10];
uint8_t *cpc_readmap[8];                /* redcode reads through it (Z80_PBC_MAP) */

static Z80 cpu;
static ay8910_t psg;

static uint8_t ram[8][0x4000];

const uint8_t *cpc_ram(void) { return &ram[0][0]; }
static const uint8_t *rom_os, *rom_basic, *rom_amsdos;
/* the ROMs run from RAM: the firmware's interrupt code alone runs 300
 * times a second, and from flash every miss of the XIP cache costs */
static uint8_t rom_ram[3][0x4000] __attribute__((aligned(4)));
static uint8_t *wpage[4];

/* gate array */
static uint8_t ga_pen, ga_ink[17], ga_mode, ga_rom, ram_cfg, upper_rom;
static bool ink_dirty = true;           /* the line table (below) is stale */
static int ga_counter, hsync_after_vsync;

/* CRTC 6845 */
static uint8_t crtc_sel, crtc[18];
static int vcc, vlc, adj_left, vsync_left, since_vsync;
static uint16_t ma_frame;
static bool in_adjust;

/* PPI 8255 */
static uint8_t ppi_a, ppi_c;

static int frame_t;                     /* T-states into the frame */
static uint32_t frames;
static int tape_t;
static bool motor;

/* sound */
static int32_t mix[AUDIO_MAX_N];
static int frame_n, made;

/* typing */
static char type_buf[64];
static int type_pos, type_wait;

/* ---- memory ---------------------------------------------------------------------- */

static uint8_t ram_map[8][4] CPC_TAB("ram_map") = {
    { 0, 1, 2, 3 }, { 0, 1, 2, 7 }, { 4, 5, 6, 7 }, { 0, 3, 2, 7 },
    { 0, 4, 2, 3 }, { 0, 5, 2, 3 }, { 0, 6, 2, 3 }, { 0, 7, 2, 3 },
};

static CPC_RAM void map_update(void)
{
    for (int p = 0; p < 4; p++)
        wpage[p] = ram[ram_map[ram_cfg & 7][p]];
    const uint8_t *r[4] = { wpage[0], wpage[1], wpage[2], wpage[3] };
    if (!(ga_rom & 4))
        r[0] = rom_os;
    if (!(ga_rom & 8))
        r[3] = upper_rom == 7 ? rom_amsdos : rom_basic;
    for (int p = 0; p < 4; p++) {
        cpc_readmap[2 * p] = (uint8_t *)r[p];
        cpc_readmap[2 * p + 1] = (uint8_t *)r[p] + 0x2000;
    }
}

CPC_RAM void cpc_write(uint16_t a, uint8_t v)
{
    wpage[a >> 14][a & 0x3fff] = v;     /* writes always reach the RAM */
}

static zuint8 rd(void *ctx, zuint16 a) { (void)ctx; return cpc_readmap[a >> 13][a & 0x1fff]; }
static void wr(void *ctx, zuint16 a, zuint8 v) { (void)ctx; cpc_write(a, v); }

/* ---- time, sound, tape ---------------------------------------------------------- */

static inline int now(void)
{
    return frame_t + (int)cpu.cycles;
}

static CPC_RAM void sound_to(int t)
{
    if (!frame_n)
        return;
    int want = t * frame_n / FRAME_T;   /* < 2^31; no __aeabi_ldivmod */
    if (want > frame_n) want = frame_n;
    if (want > made) {
        ay_render(&psg, mix + made, want - made);
        made = want;
    }
}

/* the tape runs in 3.5 MHz T-states (the TZX unit), the CPC at 4 MHz */
static CPC_RAM void tape_sync(int t)
{
    if (t > tape_t) {
        if (tape_playing())
            tape_advance((uint32_t)(t - tape_t) * 7 / 8);
        tape_t = t;
    }
}

static CPC_RAM void set_motor(bool on)
{
    if (on == motor)
        return;
    tape_sync(now());
    motor = on;
    if (on && tape_present() && !tape_at_end())
        tape_play(true);
    else if (!on)
        tape_play(false);
}

/* ---- I/O ------------------------------------------------------------------------- */

static CPC_RAM uint8_t keyboard_row(void)
{
    int r = ppi_c & 15;
    return r < 10 ? (uint8_t)~cpc_keys[r] : 0xff;
}

static CPC_RAM void irq(bool on)
{
    z80_int(&cpu, on ? Z_TRUE : Z_FALSE);
}

static CPC_RAM void psg_control(void)
{
    switch (ppi_c >> 6) {
    case 2: sound_to(now()); ay_write_data(&psg, ppi_a); break;
    case 3: ay_write_addr(&psg, ppi_a); break;
    default: break;
    }
}

static CPC_RAM zuint8 io_in(void *ctx, zuint16 port)
{
    (void)ctx;
    uint8_t v = 0xff;
    if (!(port & 0x4000) && ((port >> 8) & 3) == 3)
        v = 0;                          /* CRTC registers (type 0): R12/R13 write only */
    if (!(port & 0x0800)) {             /* PPI */
        switch ((port >> 8) & 3) {
        case 0:
            v = (ppi_c >> 6) == 1 ? (psg.addr == 14 ? keyboard_row() : ay_read_data(&psg, NULL)) : ppi_a;
            break;
        case 1:
            tape_sync(now());
            v = (uint8_t)(0x7e | (vsync_left > 0 ? 1 : 0) |
                          (motor && tape_playing() && tape_level() ? 0x80 : 0));
            break;
        case 2:
            v = ppi_c;
            break;
        }
    }
    if (!(port & 0x0480) && (port & 0x0100))
        v = (port & 1) ? fdc_read() : fdc_status();
    return v;
}

static CPC_RAM void io_out(void *ctx, zuint16 port, zuint8 v)
{
    (void)ctx;
    if ((port & 0xc000) == 0x4000) {    /* gate array */
        switch (v >> 6) {
        case 0: ga_pen = (v & 0x10) ? 16 : (v & 15); break;
        case 1:
            if (ga_ink[ga_pen] != (v & 31)) {
                ga_ink[ga_pen] = v & 31;
                ink_dirty = true;
            }
            break;
        case 2:
            ga_mode = v & 3;
            ga_rom = v;
            if (v & 0x10) {
                ga_counter = 0;
                irq(false);
            }
            map_update();
            break;
        case 3:
            ram_cfg = v & 7;
            map_update();
            break;
        }
    }
    if (!(port & 0x2000)) {             /* upper ROM select */
        upper_rom = v;
        map_update();
    }
    if (!(port & 0x4000)) {             /* CRTC */
        switch ((port >> 8) & 3) {
        case 0: crtc_sel = v & 31; break;
        case 1: if (crtc_sel < 18) crtc[crtc_sel] = v; break;
        }
    }
    if (!(port & 0x0800)) {             /* PPI */
        switch ((port >> 8) & 3) {
        case 0: ppi_a = v; psg_control(); break;
        case 2: ppi_c = v; psg_control(); set_motor((v & 0x10) != 0); break;
        case 3:
            if (v & 0x80) {
                ppi_c = 0;              /* mode set: the outputs go low */
            } else {
                int b = (v >> 1) & 7;
                ppi_c = (uint8_t)((v & 1) ? ppi_c | (1 << b) : ppi_c & ~(1 << b));
            }
            psg_control();
            set_motor((ppi_c & 0x10) != 0);
            break;
        }
    }
    if (!(port & 0x0480)) {             /* FDC */
        if (port & 0x0100) {
            if (port & 1) fdc_write(v);
        } else {
            fdc_motor(v & 1);
        }
    }
}

static CPC_RAM zuint8 inta(void *ctx, zuint16 a)
{
    (void)ctx;
    (void)a;
    ga_counter &= 0x1f;                 /* the acknowledge clears bit 5 */
    irq(false);
    return 0xff;
}

/* ---- the screen -------------------------------------------------------------------- */

/* per byte its eight pixels in the current inks and mode, rebuilt when
 * one of them changes. Each half (four pixels, one word) is one of 16
 * words that depend only on the inks: per mode, pen_idx says which two
 * of them a byte is, so a rebuild makes the 16 words and copies. */
static uint32_t line_tab[256][2] __attribute__((aligned(8)));
static uint8_t pen_idx[3][256][2];
static int tab_mode = -1;

static CPC_RAM void build_line_tab(void)
{
    int mode = ga_mode == 3 ? 0 : ga_mode;
    uint32_t t[16];
    for (int n = 0; n < 16; n++) {
        uint32_t a, b;
        switch (mode) {
        case 0:                         /* one pen, four pixels */
            t[n] = ga_ink[n & (ga_mode == 3 ? 3 : 15)] * 0x01010101u;
            break;
        case 1:                         /* two pens, two pixels each */
            a = ga_ink[n >> 2];
            b = ga_ink[n & 3];
            t[n] = a * 0x0101u | b * 0x01010000u;
            break;
        default:                        /* four pens, one pixel each */
            t[n] = (uint32_t)ga_ink[n >> 3 & 1] | (uint32_t)ga_ink[n >> 2 & 1] << 8 |
                   (uint32_t)ga_ink[n >> 1 & 1] << 16 | (uint32_t)ga_ink[n & 1] << 24;
            break;
        }
    }
    const uint8_t (*ix)[2] = pen_idx[mode];
    for (int b = 0; b < 256; b++) {
        line_tab[b][0] = t[ix[b][0]];
        line_tab[b][1] = t[ix[b][1]];
    }
    tab_mode = ga_mode;
    ink_dirty = false;
}

static void make_tables(void)
{
    for (int b = 0; b < 256; b++) {
        /* mode 0: the two pixels' pens */
        pen_idx[0][b][0] = (uint8_t)(((b >> 7) & 1) | ((b >> 2) & 2) | ((b >> 3) & 4) | ((b << 2) & 8));
        pen_idx[0][b][1] = (uint8_t)(((b >> 6) & 1) | ((b >> 1) & 2) | ((b >> 2) & 4) | ((b << 3) & 8));
        /* mode 1: pixels 0,1 and 2,3 as pen * 4 + pen */
        int p[4];
        for (int i = 0; i < 4; i++)
            p[i] = ((b >> (7 - i)) & 1) | (((b >> (3 - i)) & 1) << 1);
        pen_idx[1][b][0] = (uint8_t)(p[0] << 2 | p[1]);
        pen_idx[1][b][1] = (uint8_t)(p[2] << 2 | p[3]);
        /* mode 2: the nibbles, leftmost pixel in bit 3 */
        pen_idx[2][b][0] = (uint8_t)(b >> 4);
        pen_idx[2][b][1] = (uint8_t)(b & 15);
    }
}

static CPC_RAM void draw_line(uint8_t *px)  /* px: 8-aligned */
{
    uint8_t border = ga_ink[16];
    int r9 = crtc[9] & 31;
    if (vcc >= (crtc[6] & 127) || in_adjust || vlc > r9) {
        memset(px, border, CPC_WIDTH);
        return;
    }
    int chars = crtc[1];
    int w = chars * 16;
    int skip = 0, x0 = (CPC_WIDTH - w) / 2;
    if (x0 < 0) {
        skip = (-x0) / 16;
        chars -= 2 * skip;
        x0 = (CPC_WIDTH - chars * 16) / 2;
    }
    memset(px, border, (size_t)x0);
    if (ink_dirty || tab_mode != ga_mode)
        build_line_tab();
    uint8_t *o = px + x0;
    uint16_t ma = (uint16_t)(ma_frame + vcc * crtc[1] + skip);
    for (int c = 0; c < chars; c++, ma++, o += 16) {
        uint32_t addr = ((uint32_t)(ma & 0x3000) << 2) | ((uint32_t)(vlc & 7) << 11) | ((uint32_t)(ma & 0x3ff) << 1);
        const uint8_t *m = &ram[addr >> 14][addr & 0x3fff];
        memcpy(o, line_tab[m[0]], 8);
        memcpy(o + 8, line_tab[m[1]], 8);
    }
    int right = CPC_WIDTH - (int)(o - px);
    if (right > 0)
        memset(o, border, (size_t)right);
}

/* the end of a line: CRTC counters, vertical sync, the gate array's interrupt */
static CPC_RAM void end_line(void)
{
    if (vsync_left > 0)
        vsync_left--;
    since_vsync++;
    if (in_adjust) {
        if (--adj_left <= 0) {
            in_adjust = false;
            vcc = 0;
            vlc = 0;
            ma_frame = (uint16_t)(((crtc[12] & 0x3f) << 8) | crtc[13]);
        }
    } else if (++vlc > (crtc[9] & 31)) {
        vlc = 0;
        if (++vcc > (crtc[4] & 127)) {
            if (crtc[5] & 31) {
                in_adjust = true;
                adj_left = crtc[5] & 31;
            } else {
                vcc = 0;
                ma_frame = (uint16_t)(((crtc[12] & 0x3f) << 8) | crtc[13]);
            }
        }
    }
    if (!in_adjust && vlc == 0 && vcc == (crtc[7] & 127) && vsync_left == 0) {
        vsync_left = (crtc[3] >> 4) ? (crtc[3] >> 4) : 16;
        since_vsync = 0;
        hsync_after_vsync = 2;
    }
    /* gate array: an interrupt every 52 lines, re-synced two lines into the VSYNC */
    if (++ga_counter >= 52) {
        ga_counter = 0;
        irq(true);
    }
    if (hsync_after_vsync && --hsync_after_vsync == 0) {
        if (ga_counter >= 32)
            irq(true);
        ga_counter = 0;
    }
}

/* ---- the machine -------------------------------------------------------------------- */

void cpc_reset(void)
{
    memset(ram, 0, sizeof ram);
    ga_pen = 0;
    memset(ga_ink, 20, sizeof ga_ink);  /* black */
    ink_dirty = true;
    ga_mode = 1;
    ga_rom = 0;
    ram_cfg = 0;
    upper_rom = 0;
    ga_counter = 0;
    hsync_after_vsync = 0;
    static const uint8_t crtc_init[18] = { 63, 40, 46, 0x8e, 38, 0, 25, 30, 0, 7, 0, 0, 0x30, 0, 0, 0, 0, 0 };
    memcpy(crtc, crtc_init, sizeof crtc);
    crtc_sel = 0;
    vcc = vlc = adj_left = vsync_left = 0;
    since_vsync = 0;
    in_adjust = false;
    ma_frame = 0x3000;
    ppi_a = ppi_c = 0;
    motor = false;
    map_update();
    memset(&cpu, 0, sizeof cpu);
    cpu.fetch_opcode = rd;
    cpu.fetch = rd;
    cpu.read = rd;
    cpu.write = wr;
    cpu.in = io_in;
    cpu.out = io_out;
    cpu.inta = inta;
    cpu.options = Z80_MODEL_ZILOG_NMOS;
    z80_power(&cpu, Z_TRUE);
    ay_reset(&psg);
    fdc_reset();
    frame_t = 0;
    tape_t = 0;
    type_buf[0] = 0;
    type_pos = 0;
}

void cpc_init(const uint8_t *rom, const uint8_t *amsdos)
{
    memcpy(rom_ram[0], rom, 0x8000);    /* OS and BASIC */
    memcpy(rom_ram[2], amsdos, 0x4000);
    rom_os = rom_ram[0];
    rom_basic = rom_ram[1];
    rom_amsdos = rom_ram[2];
    make_tables();
    ay_init(&psg, 1000000, 45454);    /* = AUDIO_RATE (src/audio_pwm.h) */
    cpc_reset();
}

uint32_t cpc_palette(int i)
{
    static const uint32_t pal[32] = {
        0x808080, 0x808080, 0x00ff80, 0xffff80, 0x000080, 0xff0080, 0x008080, 0xff8080,
        0xff0080, 0xffff80, 0xffff00, 0xffffff, 0xff0000, 0xff00ff, 0xff8000, 0xff80ff,
        0x000080, 0x00ff80, 0x00ff00, 0x00ffff, 0x000000, 0x0000ff, 0x008000, 0x0080ff,
        0x800080, 0x80ff80, 0x80ff00, 0x80ffff, 0x800000, 0x8000ff, 0x808000, 0x8080ff,
    };
    return pal[i & 31];
}

int cpc_key_of_char(char c)
{
    static const char *const rows[10] = {
        "\0\0\0\0\0\0\0\0", "\0\0\0\0\0\0\0\0", "\0[\n]\0\0\\\0", "^-@P;:/.",
        "09OILKM,", "87UYHJN ", "65RTGFBV", "43EWSDCX", "12\0Q\0A\0Z", "\0\0\0\0\0\0\0\0",
    };
    if (c >= 'a' && c <= 'z')
        c = (char)(c - 32);
    if (c == '"') return CPC_KEY(8, 1) | 0x100;     /* SHIFT 2 */
    if (c == '|') return CPC_KEY(3, 2) | 0x100;     /* SHIFT @ */
    if (c == ' ') return CPCK_SPACE;
    if (c == '\n') return CPCK_RETURN;
    for (int r = 2; r < 9; r++)
        for (int b = 0; b < 8; b++)
            if (rows[r][b] == c && c)
                return CPC_KEY(r, b);
    return -1;
}

/* ---- loading ------------------------------------------------------------------------ */

bool cpc_insert_disk(const uint8_t *d, uint32_t n)
{
    return fdc_insert(d, n);
}

bool cpc_insert_tape(const uint8_t *d, uint32_t n)
{
    return tape_insert(d, n, true);
}

void cpc_tape_rewind(void)
{
    tape_rewind();
}

bool cpc_tape_busy(void)
{
    return motor && tape_playing();
}

void cpc_type(const char *s, int delay)
{
    strncpy(type_buf, s, sizeof type_buf - 1);
    type_buf[sizeof type_buf - 1] = 0;
    type_pos = 0;
    type_wait = delay;
}

bool cpc_autoload_disk(void)
{
    const char *p = fdc_first_program();
    if (!p)
        return false;
    char s[40];
    if (!p[0])
        strcpy(s, "|CPM\n");
    else
        snprintf(s, sizeof s, "RUN\"%s\n", p);
    cpc_type(s, 90);
    return true;
}

void cpc_autoload_tape(void)
{
    /* the 6128 starts on the disk: |TAPE, then RUN" and "press any key" */
    cpc_type("|TAPE\nRUN\"\n~ ", 90);
}

/* one step of the typing, each frame: a key held 3 frames, 3 frames apart */
static void type_step(void)
{
    if (!type_buf[type_pos])
        return;
    if (type_wait > 0) {
        type_wait--;
        return;
    }
    static int phase;
    char ch = type_buf[type_pos];
    if (ch == '~') {                    /* a pause */
        type_pos++;
        type_wait = 40;
        return;
    }
    if (phase < 3) {
        int k = cpc_key_of_char(ch);
        if (k >= 0) {
            cpc_key_set(k & 0xff);
            if (k & 0x100)
                cpc_key_set(CPCK_SHIFT);
        }
    }
    if (++phase >= 6) {
        phase = 0;
        type_pos++;
    }
}

bool cpc_load_snapshot(const uint8_t *d, uint32_t n)
{
    if (n < 0x100 + 0x10000 || memcmp(d, "MV - SNA", 8) != 0)
        return false;
    cpc_reset();
    uint32_t kb = d[0x6b] | d[0x6c] << 8;
    if (kb != 64 && kb != 128)
        kb = n >= 0x100 + 0x20000 ? 128 : 64;
    if (n < 0x100 + kb * 1024)
        kb = 64;
    memcpy(ram, d + 0x100, kb * 1024);
    Z80_F(cpu) = d[0x11]; Z80_A(cpu) = d[0x12];
    Z80_C(cpu) = d[0x13]; Z80_B(cpu) = d[0x14];
    Z80_E(cpu) = d[0x15]; Z80_D(cpu) = d[0x16];
    Z80_L(cpu) = d[0x17]; Z80_H(cpu) = d[0x18];
    cpu.r = d[0x19];
    cpu.r7 = d[0x19] & 0x80;
    cpu.i = d[0x1a];
    cpu.iff1 = d[0x1b] & 1;
    cpu.iff2 = d[0x1c] & 1;
    Z80_IX(cpu) = (uint16_t)(d[0x1d] | d[0x1e] << 8);
    Z80_IY(cpu) = (uint16_t)(d[0x1f] | d[0x20] << 8);
    Z80_SP(cpu) = (uint16_t)(d[0x21] | d[0x22] << 8);
    Z80_PC(cpu) = (uint16_t)(d[0x23] | d[0x24] << 8);
    cpu.im = d[0x25] & 3;
    Z80_F_(cpu) = d[0x26]; Z80_A_(cpu) = d[0x27];
    Z80_C_(cpu) = d[0x28]; Z80_B_(cpu) = d[0x29];
    Z80_E_(cpu) = d[0x2a]; Z80_D_(cpu) = d[0x2b];
    Z80_L_(cpu) = d[0x2c]; Z80_H_(cpu) = d[0x2d];
    ga_pen = d[0x2e] & 31;
    if (ga_pen > 16) ga_pen = 16;
    for (int i = 0; i < 17; i++)
        ga_ink[i] = d[0x2f + i] & 31;
    ink_dirty = true;
    ga_rom = d[0x40];
    ga_mode = d[0x40] & 3;
    ram_cfg = d[0x41] & 7;
    crtc_sel = d[0x42] & 31;
    memcpy(crtc, d + 0x43, 18);
    upper_rom = d[0x55];
    ppi_a = d[0x56];
    ppi_c = d[0x58];
    for (int r = 0; r < 16; r++) {
        ay_write_addr(&psg, (uint8_t)r);
        ay_write_data(&psg, d[0x5b + r]);
    }
    ay_write_addr(&psg, d[0x5a]);
    ma_frame = (uint16_t)(((crtc[12] & 0x3f) << 8) | crtc[13]);
    map_update();
    return true;
}

/* ---- the frame ------------------------------------------------------------------------ */

/* run up to T-state t: every instruction takes whole microseconds. The
 * Z80 rounds them itself (Z80_PBC_ROUND4) and runs the stretch in one
 * call; before, one call per instruction cost about as much as the
 * instruction, and a HALT went 4 T-states per call. */
static CPC_RAM void run_to(int t)
{
    while (frame_t < t) {
        int c = (int)z80_run(&cpu, (zusize)(t - frame_t));
        frame_t += c > 0 ? (c + 3) & ~3 : 4;
    }
}

CPC_RAM void cpc_frame(cpc_line_begin_fn begin, cpc_line_commit_fn commit, int16_t *audio, int n)
{
    if (n > AUDIO_MAX_N)
        n = AUDIO_MAX_N;
    frame_n = n;
    made = 0;
    memset(mix, 0, (size_t)n * sizeof mix[0]);
    frames++;
    type_step();

    for (int l = 0; l < LINES; l++) {
        run_to((l + 1) * LINE_T);
        int p = since_vsync - 32;
        if (begin && p >= 0 && p < CPC_SHOWN) {
            uint8_t *px = begin(p);     /* straight into the video ring */
            if (px) {
                draw_line(px);
                commit(p);
            }
        }
        end_line();
    }
    tape_sync(FRAME_T);
    tape_t = 0;
    sound_to(FRAME_T);

    static int32_t dc;
    for (int i = 0; i < n; i++) {
        int32_t v = mix[i];
        dc += (v * 256 - dc) >> 10;
        v -= dc >> 8;
        audio[i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
    }
    frame_n = 0;
    frame_t -= FRAME_T;
}
