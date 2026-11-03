/* zx.c -- see zx.h */
#include <string.h>
#include "zx.h"
#include "tape.h"
#include "ay8910.h"
#include "Z80.h"

#ifdef PBC_ON_DEVICE
#define ZX_RAM __attribute__((section(".time_critical.zx")))
#else
#define ZX_RAM
#endif

#define AUDIO_MAX_N 1024

uint8_t zx_keys[8];
uint8_t zx_kempston;
uint8_t *zx_readmap[8];                 /* redcode reads through it (Z80_PBC_MAP) */

static Z80 cpu;
static ay8910_t psg;
static int model = ZX_48K;

/* timing of the model */
static int t_line, n_lines, t_frame, first_line, int_len, float_base;

static uint8_t ram[8][0x4000];          /* 48K: banks 5, 2, 0 */
static uint8_t basic_rom[0x4000];       /* the 48 BASIC ROM, LD-BYTES trapped */
static const uint8_t *rom48, *rom_editor, *rom_basic128;
static uint8_t *wpage[4];               /* NULL: ROM */
static uint8_t port7ffd;
static bool paging_locked;

static int frame_t;                     /* T-state of the frame where z80_run started */
static uint32_t frames;
static uint8_t border, last_fe;
static int tape_t;

/* sound */
static int32_t mix[AUDIO_MAX_N];
static int frame_n, made;
static uint16_t beep_t[2048];
static uint8_t beep_v[2048];
static int beep_n;
static uint8_t beep_level, beep_start;

/* autoload: frames to go, and which */
static int auto_frame = -1;

/* ---- memory ------------------------------------------------------------------- */

static void map_update(void)
{
    const uint8_t *rom = (model == ZX_128K && !(port7ffd & 0x10)) ? rom_editor : basic_rom;
    int top = model == ZX_128K ? (port7ffd & 7) : 0;
    zx_readmap[0] = (uint8_t *)rom;
    zx_readmap[1] = (uint8_t *)rom + 0x2000;
    zx_readmap[2] = ram[5];
    zx_readmap[3] = ram[5] + 0x2000;
    zx_readmap[4] = ram[2];
    zx_readmap[5] = ram[2] + 0x2000;
    zx_readmap[6] = ram[top];
    zx_readmap[7] = ram[top] + 0x2000;
    wpage[0] = NULL;
    wpage[1] = ram[5];
    wpage[2] = ram[2];
    wpage[3] = ram[top];
}

ZX_RAM void zx_write(uint16_t a, uint8_t v)
{
    uint8_t *p = wpage[a >> 14];
    if (p)
        p[a & 0x3fff] = v;
}

static uint8_t rd8(uint16_t a) { return zx_readmap[a >> 13][a & 0x1fff]; }

static zuint8 rd(void *ctx, zuint16 a) { (void)ctx; return rd8(a); }
static void wr(void *ctx, zuint16 a, zuint8 v) { (void)ctx; zx_write(a, v); }

/* ---- time and sound ----------------------------------------------------------- */

static inline int now(void)
{
    return frame_t + (int)cpu.cycles;
}

static void sound_to(int t)
{
    if (model != ZX_128K || !frame_n)
        return;
    int want = (int)((int64_t)t * frame_n / t_frame);
    if (want > frame_n) want = frame_n;
    if (want > made) {
        ay_render(&psg, mix + made, want - made);
        made = want;
    }
}

static void beep(uint8_t level)
{
    if (level == beep_level)
        return;
    beep_level = level;
    if (beep_n < (int)(sizeof beep_t / sizeof beep_t[0])) {
        int t = now();
        beep_t[beep_n] = (uint16_t)(t < 0 ? 0 : t > 0xffff ? 0xffff : t);
        beep_v[beep_n++] = level;
    }
}

static void tape_sync(int t)
{
    if (t > tape_t) {
        if (tape_playing())
            tape_advance((uint32_t)(t - tape_t));
        tape_t = t;
    }
}

/* ---- I/O ---------------------------------------------------------------------- */

/* what the ULA is reading from the screen right now (FFh outside it) */
static uint8_t floating_bus(void)
{
    int d = now() - float_base;
    if (d < 0)
        return 0xff;
    int y = d / t_line, x = d % t_line;
    if (y >= 192 || x >= 128)
        return 0xff;
    int col = (x >> 3) * 2, k = x & 7;
    if (k >= 4)
        return 0xff;
    if (k & 2)
        col++;
    const uint8_t *scr = ram[(model == ZX_128K && (port7ffd & 8)) ? 7 : 5];
    if (k & 1)
        return scr[0x1800 + (y >> 3) * 32 + col];
    return scr[((y & 0xc0) << 5) | ((y & 7) << 8) | ((y & 0x38) << 2) | col];
}

static zuint8 io_in(void *ctx, zuint16 port)
{
    (void)ctx;
    if (!(port & 1)) {                  /* ULA */
        tape_sync(now());
        uint8_t k = 0;
        for (int r = 0; r < 8; r++)
            if (!(port & (0x100 << r)))
                k |= zx_keys[r];
        uint8_t v = (uint8_t)((~k & 0x1f) | 0xa0);
        int ear = tape_playing() ? tape_level() : (last_fe >> 4) & 1;   /* issue 3 */
        return ear ? v | 0x40 : v;
    }
    if (!(port & 0x20))
        return zx_kempston;             /* Kempston at 1Fh */
    if (model == ZX_128K && (port & 0xc002) == 0xc000)
        return ay_read_data(&psg, NULL);
    return floating_bus();
}

static void io_out(void *ctx, zuint16 port, zuint8 v)
{
    (void)ctx;
    if (!(port & 1)) {
        border = v & 7;
        beep((uint8_t)((v >> 3) & 3));  /* bit 4 EAR, bit 3 MIC */
        last_fe = v;
    }
    if (model == ZX_128K) {
        if (!(port & 0x8002)) {
            if (!paging_locked) {
                port7ffd = v;
                paging_locked = (v & 0x20) != 0;
                map_update();
            }
        } else if ((port & 0xc002) == 0xc000) {
            ay_write_addr(&psg, v);
        } else if ((port & 0xc002) == 0x8000) {
            sound_to(now());
            ay_write_data(&psg, v);
        }
    }
}

static zuint8 inta(void *ctx, zuint16 a)
{
    (void)ctx;
    (void)a;
    return 0xff;                        /* the idle data bus */
}

/* ---- loading from tape: LD-BYTES (0556h) ----------------------------------------
 *
 * The copy of the 48 BASIC ROM holds the trap opcode 64h at 0556h. With a
 * standard block next on the tape the load is done at once: A the flag
 * byte expected, carry set to LOAD (else VERIFY), IX the address, DE the
 * length; carry set on return if all went well, as the ROM does it. A
 * block with another flag is passed over with an error, so the ROM goes on
 * looking (LOAD "" skipping data blocks while it looks for a header). With
 * anything else next, the tape plays in real time and the ROM loads it. */
static zuint8 trap(void *ctx, zuint16 pc)
{
    (void)ctx;
    if (pc != 0x0556 || zx_readmap[0] != basic_rom)
        return 0x64;                    /* an ordinary LD H,H */
    const uint8_t *b;
    uint32_t n;
    if (!tape_present() || tape_playing() || !tape_next_standard(&b, &n)) {
        if (tape_present() && !tape_playing() && !tape_at_end())
            tape_play(true);
        return 0x14;                    /* INC D: the ROM loads it itself */
    }
    uint8_t want = Z80_A(cpu);
    bool load = Z80_F(cpu) & 1;
    uint16_t ix = Z80_IX(cpu), de = Z80_DE(cpu);
    bool ok = false;
    if (n >= 1 && b[0] == want) {
        uint8_t parity = b[0];
        uint32_t i = 0;
        for (; i < de && i + 1 < n; i++) {
            uint8_t v = b[i + 1];
            parity ^= v;
            if (load)
                zx_write((uint16_t)(ix + i), v);
            else if (rd8((uint16_t)(ix + i)) != v)
                break;
        }
        ix = (uint16_t)(ix + i);
        de = (uint16_t)(de - i);
        if (de == 0 && i + 1 < n) {
            parity ^= b[i + 1];         /* the checksum */
            ok = parity == 0;
        }
    }
    Z80_IX(cpu) = ix;
    Z80_DE(cpu) = de;
    Z80_F(cpu) = (uint8_t)(ok ? (Z80_F(cpu) | 1) : (Z80_F(cpu) & ~1));
    cpu.iff1 = cpu.iff2 = 1;            /* as SA/LD-RET leaves it */
    uint16_t sp = Z80_SP(cpu);
    Z80_PC(cpu) = (uint16_t)(rd8(sp) | rd8((uint16_t)(sp + 1)) << 8);
    Z80_SP(cpu) = (uint16_t)(sp + 2);
    /* a custom loader may follow: then the tape runs on by itself */
    if (!tape_peek_standard() && !tape_at_end())
        tape_play(true);
    return Z80_HOOK;
}

/* ---- the machine ---------------------------------------------------------------- */

static void set_model(int m)
{
    model = m;
    tape_is_48k = m == ZX_48K;
    if (m == ZX_128K) {
        t_line = 228; n_lines = 311; first_line = 63; int_len = 36; float_base = 14364;
        memcpy(basic_rom, rom_basic128, sizeof basic_rom);
    } else {
        t_line = 224; n_lines = 312; first_line = 64; int_len = 32; float_base = 14338;
        memcpy(basic_rom, rom48, sizeof basic_rom);
    }
    t_frame = t_line * n_lines;
    basic_rom[0x0556] = 0x64;           /* the trap, see above */
    /* AY at 1.7734 MHz (128K) */
    ay_init(&psg, 1773400, 22222);
}

void zx_reset(void)
{
    memset(ram, 0, sizeof ram);
    port7ffd = 0;
    paging_locked = false;
    map_update();
    memset(&cpu, 0, sizeof cpu);
    cpu.fetch_opcode = rd;
    cpu.fetch = rd;
    cpu.read = rd;
    cpu.write = wr;
    cpu.in = io_in;
    cpu.out = io_out;
    cpu.inta = inta;
    cpu.hook = trap;
    cpu.options = Z80_MODEL_ZILOG_NMOS;
    z80_power(&cpu, Z_TRUE);
    ay_reset(&psg);
    frame_t = 0;
    tape_t = 0;
    border = 7;
    last_fe = 0;
    beep_level = 0;
    auto_frame = -1;
}

void zx_init(int m, const uint8_t *r48, const uint8_t *r128_0, const uint8_t *r128_1)
{
    rom48 = r48;
    rom_editor = r128_0;
    rom_basic128 = r128_1;
    set_model(m);
    zx_reset();
}

int zx_model(void) { return model; }

uint32_t zx_frame_us(void)
{
    return model == ZX_128K ? 19992 : 19968;
}

uint32_t zx_palette(int i)
{
    static const uint32_t pal[16] = {
        0x000000, 0x0000d7, 0xd70000, 0xd700d7, 0x00d700, 0x00d7d7, 0xd7d700, 0xd7d7d7,
        0x000000, 0x0000ff, 0xff0000, 0xff00ff, 0x00ff00, 0x00ffff, 0xffff00, 0xffffff,
    };
    return pal[i & 15];
}

int zx_key_of_char(char c)
{
    static const char rows[8][6] = { "\001ZXCV", "ASDFG", "QWERT", "12345", "09876", "POIUY", "\002LKJH", " \003MNB" };
    if (c >= 'a' && c <= 'z')
        c = (char)(c - 32);
    for (int r = 0; r < 8; r++)
        for (int b = 0; b < 5; b++)
            if (rows[r][b] == c && c > 3)
                return ZX_KEY(r, b);
    return -1;
}

/* ---- tapes and snapshots ---------------------------------------------------------- */

bool zx_insert_tape(const uint8_t *d, uint32_t n, bool tzx)
{
    return tape_insert(d, n, tzx);
}

void zx_tape_rewind(void)
{
    tape_rewind();
}

bool zx_tape_busy(void)
{
    return tape_playing();
}

void zx_autoload(void)
{
    auto_frame = 0;
}

/* the keys of the autoload at frame f of it */
static void autoload_keys(int f)
{
    if (model == ZX_128K) {
        if (f >= 60 && f < 64)
            zx_key_set(ZXK_ENTER);      /* the menu's first entry: Tape Loader */
        return;
    }
    /* 48K: LOAD "" -- J gives LOAD in K mode, SYMBOL SHIFT + P a quote */
    if (f >= 110 && f < 114) zx_key_set(ZX_KEY(6, 3));
    if (f >= 120 && f < 124) { zx_key_set(ZXK_SYM); zx_key_set(ZX_KEY(5, 0)); }
    if (f >= 130 && f < 134) { zx_key_set(ZXK_SYM); zx_key_set(ZX_KEY(5, 0)); }
    if (f >= 140 && f < 144) zx_key_set(ZXK_ENTER);
}

static void set_regs_common(void)
{
    cpu.halt_line = 0;
    cpu.int_line = 0;
    cpu.request = 0;
}

static bool load_sna(const uint8_t *d, uint32_t n)
{
    bool is128 = n == 131103 || n == 147487;
    if (n != 49179 && !is128)
        return false;
    set_model(is128 ? ZX_128K : ZX_48K);
    zx_reset();
    cpu.i = d[0];
    Z80_HL_(cpu) = (uint16_t)(d[1] | d[2] << 8);
    Z80_DE_(cpu) = (uint16_t)(d[3] | d[4] << 8);
    Z80_BC_(cpu) = (uint16_t)(d[5] | d[6] << 8);
    Z80_AF_(cpu) = (uint16_t)(d[7] | d[8] << 8);
    Z80_HL(cpu) = (uint16_t)(d[9] | d[10] << 8);
    Z80_DE(cpu) = (uint16_t)(d[11] | d[12] << 8);
    Z80_BC(cpu) = (uint16_t)(d[13] | d[14] << 8);
    Z80_IY(cpu) = (uint16_t)(d[15] | d[16] << 8);
    Z80_IX(cpu) = (uint16_t)(d[17] | d[18] << 8);
    cpu.iff1 = cpu.iff2 = (d[19] >> 2) & 1;
    cpu.r = d[20];
    cpu.r7 = d[20] & 0x80;
    Z80_AF(cpu) = (uint16_t)(d[21] | d[22] << 8);
    Z80_SP(cpu) = (uint16_t)(d[23] | d[24] << 8);
    cpu.im = d[25] & 3;
    border = d[26] & 7;
    last_fe = border;
    set_regs_common();
    const uint8_t *m = d + 27;
    if (!is128) {
        memcpy(ram[5], m, 0x4000);
        memcpy(ram[2], m + 0x4000, 0x4000);
        memcpy(ram[0], m + 0x8000, 0x4000);
        uint16_t sp = Z80_SP(cpu);
        Z80_PC(cpu) = (uint16_t)(rd8(sp) | rd8((uint16_t)(sp + 1)) << 8);
        Z80_SP(cpu) = (uint16_t)(sp + 2);
        return true;
    }
    const uint8_t *x = m + 0xc000;
    Z80_PC(cpu) = (uint16_t)(x[0] | x[1] << 8);
    port7ffd = x[2];
    paging_locked = (port7ffd & 0x20) != 0;
    int top = port7ffd & 7;
    memcpy(ram[5], m, 0x4000);
    memcpy(ram[2], m + 0x4000, 0x4000);
    memcpy(ram[top], m + 0x8000, 0x4000);
    const uint8_t *rest = x + 4;
    for (int b = 0; b < 8; b++) {
        if (b == 5 || b == 2 || b == top)
            continue;
        if (rest + 0x4000 > d + n)
            break;
        memcpy(ram[b], rest, 0x4000);
        rest += 0x4000;
    }
    map_update();
    return true;
}

/* .z80 memory: compressed (ED ED count byte) into dst; false if broken */
static bool z80_unpack(const uint8_t *s, uint32_t n, uint8_t *dst, uint32_t size, bool v1)
{
    uint32_t i = 0, o = 0;
    while (o < size && i < n) {
        if (v1 && i + 3 < n && s[i] == 0 && s[i + 1] == 0xed && s[i + 2] == 0xed && s[i + 3] == 0)
            break;                      /* end marker */
        if (i + 3 < n && s[i] == 0xed && s[i + 1] == 0xed) {
            uint32_t cnt = s[i + 2];
            uint8_t v = s[i + 3];
            for (uint32_t k = 0; k < cnt && o < size; k++)
                dst[o++] = v;
            i += 4;
        } else {
            dst[o++] = s[i++];
        }
    }
    return o == size;
}

static bool load_z80(const uint8_t *d, uint32_t n)
{
    if (n < 30)
        return false;
    uint8_t f1 = d[12] == 0xff ? 1 : d[12];
    uint16_t pc = (uint16_t)(d[6] | d[7] << 8);
    int ext = 0, hw = 0;
    if (pc == 0) {
        if (n < 32)
            return false;
        ext = d[30] | d[31] << 8;
        if (n < (uint32_t)(32 + ext))
            return false;
        pc = (uint16_t)(d[32] | d[33] << 8);
        hw = d[34];
    }
    bool is128 = false;
    if (ext == 23)
        is128 = hw == 3 || hw == 4;
    else if (ext >= 54)
        is128 = hw >= 4;
    set_model(is128 ? ZX_128K : ZX_48K);
    zx_reset();
    Z80_A(cpu) = d[0];
    Z80_F(cpu) = d[1];
    Z80_BC(cpu) = (uint16_t)(d[2] | d[3] << 8);
    Z80_HL(cpu) = (uint16_t)(d[4] | d[5] << 8);
    Z80_PC(cpu) = pc;
    Z80_SP(cpu) = (uint16_t)(d[8] | d[9] << 8);
    cpu.i = d[10];
    cpu.r = (uint8_t)((d[11] & 0x7f) | ((f1 & 1) << 7));
    cpu.r7 = cpu.r & 0x80;
    border = (f1 >> 1) & 7;
    last_fe = border;
    Z80_DE(cpu) = (uint16_t)(d[13] | d[14] << 8);
    Z80_BC_(cpu) = (uint16_t)(d[15] | d[16] << 8);
    Z80_DE_(cpu) = (uint16_t)(d[17] | d[18] << 8);
    Z80_HL_(cpu) = (uint16_t)(d[19] | d[20] << 8);
    Z80_A_(cpu) = d[21];
    Z80_F_(cpu) = d[22];
    Z80_IY(cpu) = (uint16_t)(d[23] | d[24] << 8);
    Z80_IX(cpu) = (uint16_t)(d[25] | d[26] << 8);
    cpu.iff1 = d[27] ? 1 : 0;
    cpu.iff2 = d[28] ? 1 : 0;
    cpu.im = d[29] & 3;
    set_regs_common();

    if (ext == 0) {                     /* version 1: 48K, maybe compressed */
        static uint8_t tmp[0xc000];     /* not on the 4 KB stack */
        const uint8_t *s = d + 30;
        if (f1 & 0x20) {
            if (!z80_unpack(s, n - 30, tmp, 0xc000, true))
                return false;
            s = tmp;
        } else if (n - 30 < 0xc000) {
            return false;
        }
        memcpy(ram[5], s, 0x4000);
        memcpy(ram[2], s + 0x4000, 0x4000);
        memcpy(ram[0], s + 0x8000, 0x4000);
        return true;
    }
    if (is128) {
        port7ffd = d[35];
        paging_locked = (port7ffd & 0x20) != 0;
        if (d[37] & 4) {                /* AY registers present */
            for (int r = 0; r < 16; r++) {
                ay_write_addr(&psg, (uint8_t)r);
                ay_write_data(&psg, d[39 + r]);
            }
            ay_write_addr(&psg, d[38]);
        }
    }
    uint32_t i = 32 + (uint32_t)ext;
    while (i + 3 <= n) {
        uint32_t len = d[i] | d[i + 1] << 8;
        int page = d[i + 2];
        i += 3;
        bool raw = len == 0xffff;
        if (raw) len = 0x4000;
        if (i + len > n)
            return false;
        uint8_t *dst = NULL;
        if (is128) {
            if (page >= 3 && page <= 10)
                dst = ram[page - 3];
        } else {
            if (page == 4) dst = ram[2];
            else if (page == 5) dst = ram[0];
            else if (page == 8) dst = ram[5];
        }
        if (dst) {
            if (raw) memcpy(dst, d + i, 0x4000);
            else if (!z80_unpack(d + i, len, dst, 0x4000, false)) return false;
        }
        i += len;
    }
    map_update();
    return true;
}

bool zx_load_snapshot(const uint8_t *d, uint32_t n, bool z80)
{
    return z80 ? load_z80(d, n) : load_sna(d, n);
}

/* ---- the frame -------------------------------------------------------------------- */

static ZX_RAM void run_to(int t)
{
    while (frame_t < t) {
        int ran = (int)z80_run(&cpu, (zusize)(t - frame_t));
        frame_t += ran > 0 ? ran : 4;
    }
}

static ZX_RAM void draw_line(int ul, uint8_t *px)
{
    int y = ul - first_line;
    if (y < 0 || y >= 192) {
        memset(px, border, ZX_WIDTH);
        return;
    }
    const uint8_t *scr = ram[(model == ZX_128K && (port7ffd & 8)) ? 7 : 5];
    const uint8_t *bm = scr + (((y & 0xc0) << 5) | ((y & 7) << 8) | ((y & 0x38) << 2));
    const uint8_t *at = scr + 0x1800 + (y >> 3) * 32;
    bool flash = (frames & 16) != 0;
    for (int c = 0; c < 32; c++) {
        uint8_t a = at[c], b = bm[c];
        uint8_t bright = (a >> 3) & 8;
        uint8_t ink = (a & 7) | bright, paper = ((a >> 3) & 7) | bright;
        if ((a & 0x80) && flash) {
            uint8_t s = ink; ink = paper; paper = s;
        }
        uint8_t *o = px + c * 8;
        for (int i = 0; i < 8; i++)
            o[i] = (b & (0x80 >> i)) ? ink : paper;
    }
}

/* the beeper's level changes of this frame into the samples (box filter) */
static void beeper_render(void)
{
    static const int amp[4] = { 0, 1200, 9000, 10200 };    /* MIC, EAR */
    int ci = 0;
    uint8_t level = beep_start;
    for (int s = 0; s < frame_n; s++) {
        int a = (int)((int64_t)s * t_frame / frame_n);
        int b = (int)((int64_t)(s + 1) * t_frame / frame_n);
        int64_t acc = 0;
        int t = a;
        while (ci < beep_n && beep_t[ci] < b) {
            int c = beep_t[ci] < a ? a : beep_t[ci];
            acc += (int64_t)amp[level] * (c - t);
            t = c;
            level = beep_v[ci++];
        }
        acc += (int64_t)amp[level] * (b - t);
        mix[s] += (int32_t)(acc / (b - a));
    }
}

ZX_RAM void zx_frame(zx_line_fn line, int16_t *audio, int n)
{
    static uint8_t px[ZX_WIDTH];
    if (n > AUDIO_MAX_N)
        n = AUDIO_MAX_N;
    frame_n = n;
    made = 0;
    memset(mix, 0, (size_t)n * sizeof mix[0]);
    beep_n = 0;
    beep_start = beep_level;
    frames++;
    if (auto_frame >= 0) {
        autoload_keys(auto_frame);
        if (++auto_frame > 200)
            auto_frame = -1;
    }

    z80_int(&cpu, Z_TRUE);
    run_to(int_len);
    z80_int(&cpu, Z_FALSE);
    int top = first_line - ZX_BORDER;
    for (int ul = 0; ul < n_lines; ul++) {
        run_to((ul + 1) * t_line);
        int p = ul - top;
        if (line && p >= 0 && p < ZX_SHOWN) {
            draw_line(ul, px);
            line(p, px);
        }
    }
    tape_sync(t_frame);
    tape_t = 0;
    sound_to(t_frame);
    beeper_render();

    /* the DC goes in a one-pole filter */
    static int32_t dc;
    for (int i = 0; i < n; i++) {
        int32_t v = mix[i];
        dc += (v * 256 - dc) >> 10;
        v -= dc >> 8;
        audio[i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
    }
    frame_n = 0;
    frame_t -= t_frame;
}
