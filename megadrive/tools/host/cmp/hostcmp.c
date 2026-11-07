/* hostcmp.c -- the PBC Mega Drive core on the PC with sound and the panel
 * path, everything hashed per frame: for comparing two source trees (see
 * README.md in this directory). Built by hb.sh (PC) or ab.sh (Cortex-M33,
 * run by armrun.py).
 *
 *   hostcmp ROM FRAMES [-z ZONE] [-r SEED] [-m crop] [-t] [-q] [-d FRAME]
 *   hostcmp ROM 1 -F STEPS        VDP ports at random (DMA, FIFO, palette)
 *   hostcmp ROM 1 -L N            VDP line renderer on N random screens
 *
 * One line per frame: picture as sent to the panel (video.c, BGR565), the
 * VDP's index buffer, core 1's YM2612 and the PSG (ym_core1.c replays the
 * log), what goes to the speaker, 68000 RAM, Z80 RAM, VRAM/CRAM/VSRAM/
 * registers, and the CPU state after every m68k_run()/z80_run() slice.
 * Input: START every two seconds, from frame 1500 on random pads (-r);
 * -z as in mdhost (Sonic 2 level). -t: time per part, -d: panel as PPM.
 * HC_LINEDUMP=FRAME prints the 68000 state after each slice of a frame,
 * HC_FOLD=1 one platform-independent line per frame (PC vs. M33),
 * -DPCHIST -DM68K_TRACE_HOOK=m68k_trace_hook: 68000 PC histogram from
 * frame HC_PCFROM on (where the time goes, waiting loops).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>
#include "bus/gwenesis_bus.h"
#include "io/gwenesis_io.h"
#include "vdp/gwenesis_vdp.h"
#include "m68k.h"
#include "sound/z80inst.h"
#include "sound/ym2612.h"
#include "sound/gwenesis_sn76489.h"
#include "HDMI.h"
#include "video.h"
#include "ym_core1.h"
#define z80_run redcode_z80_run
#define z80_execute redcode_z80_execute
#include "Z80.h"
#undef z80_run
#undef z80_execute
#include "hardware/structs/timer.h"

#ifdef ARMBENCH
int arm_cur = 7;                           /* which part runs, for the instruction counter */
void arm_mark(int k);                      /* armglue.c: the emulator watches its calls */
void arm_frame(int fr);
uint64_t arm_hash[8192];
uint64_t arm_comp[1024][9];
uint32_t arm_rom_len;
char arm_args[256] = "x 10";
#endif
extern unsigned char *ROM_DATA;
extern unsigned char M68K_RAM[];
extern unsigned char ZRAM[];
extern unsigned char gwenesis_vdp_regs[];
extern unsigned short gwenesis_vdp_status;
extern unsigned char VRAM[];
extern unsigned short CRAM[];
extern unsigned short VSRAM[];
extern int hint_pending;
extern int screen_width, screen_height;
extern unsigned char button_state[];
extern Z80 cpu;
int system_clock;
int dbg_frame;
unsigned int lines_per_frame = LINES_PER_FRAME_NTSC;
int scan_line;
unsigned int frame_counter;
bool audio_enabled = true, sn76489_enabled = true, ym2612_enabled = false, z80_enabled = true;
static int16_t psg_buf[GWENESIS_AUDIO_BUFFER_MAX];
int16_t *gwenesis_sn76489_buffer = psg_buf;
int16_t *gwenesis_ym2612_buffer;
volatile int sn76489_index, sn76489_clock, ym2612_index, ym2612_clock;
void gwenesis_io_get_buttons(void) {}
#ifdef PCHIST
static uint32_t *pchist;
static int pchist_on;
void m68k_trace_hook(void)
{
    if (pchist_on) pchist[m68k_get_reg(M68K_REG_PC) & 0xFFFFFF]++;
}
#endif
static fake_timer_hw_t fake_timer;
fake_timer_hw_t *timer_hw = &fake_timer;

/* ---- hashing --------------------------------------------------------------- */
static inline uint64_t mix64(uint64_t h, uint64_t v)
{
    h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    h *= 0xff51afd7ed558ccdull;
    return h ^ (h >> 31);
}
static uint64_t hash_buf(uint64_t h, const void *p, size_t n)
{
    const uint8_t *b = p;
    while (n >= 8) { uint64_t v; memcpy(&v, b, 8); h = mix64(h, v); b += 8; n -= 8; }
    uint64_t v = 0;
    memcpy(&v, b, n);
    return mix64(h, v ^ (uint64_t)n << 56);
}

/* ---- panel (display.c stand-in) -------------------------------------------- */
static uint16_t panel[280][240];
static int win_x, win_y, win_w, win_h, win_pos;
static uint64_t h_panel_rows;
static bool prof;
void pbc_display_window(int x, int y, int w, int h) { win_x = x; win_y = y; win_w = w; win_h = h; win_pos = 0; }
void pbc_display_begin(void) {}
void pbc_display_send(const uint16_t *px, int n)
{
#ifdef ARMBENCH
    int c = arm_cur; arm_mark(6);
#endif
    for (int i = 0; i < n; i++, win_pos++) {
        int y = win_y + win_pos / win_w, x = win_x + win_pos % win_w;
        if (y < 280 && x < 240) panel[y][x] = px[i];
    }
    if (!prof) h_panel_rows = hash_buf(h_panel_rows, px, n * 2);
#ifdef ARMBENCH
    arm_mark(c);
#endif
}
bool pbc_display_busy(void) { return false; }
void pbc_display_wait(void) {}
void pbc_display_end(void) {}
void pbc_display_fill(int x, int y, int w, int h, uint16_t c) { (void)x; (void)y; (void)w; (void)h; (void)c; }
void pbc_display_text(int x, int y, const char *s, uint16_t fg, uint16_t bg, int scale) { (void)x; (void)y; (void)s; (void)fg; (void)bg; (void)scale; }
int pbc_display_text_w(const char *s, int scale) { (void)s; (void)scale; return 0; }
void pbc_display_text_centered(int y, const char *s, uint16_t fg, uint16_t bg, int scale) { (void)y; (void)s; (void)fg; (void)bg; (void)scale; }

/* ---- speaker (audio_pwm.c stand-in): hash both chips and the mix ----------- */
extern int16_t *synth_ym2612_buffer;
extern volatile int synth_ym2612_index;
static uint64_t h_ym, h_psg, h_pwm;
static long n_ym, n_psg, n_pwm, nz_ym, nz_psg;
void audio_pwm_push(const int16_t *s, int n)
{
#ifdef ARMBENCH
    int c = arm_cur; arm_mark(6);
#endif
    h_pwm = hash_buf(h_pwm, s, n * 2);
    n_pwm += n;
    h_ym = hash_buf(h_ym, synth_ym2612_buffer, synth_ym2612_index * 2);
    n_ym += synth_ym2612_index;
    h_psg = hash_buf(h_psg, gwenesis_sn76489_buffer, sn76489_index * 2);
    n_psg += sn76489_index;
    for (int i = 0; i < synth_ym2612_index; i++) nz_ym += synth_ym2612_buffer[i] != 0;
    for (int i = 0; i < sn76489_index; i++) nz_psg += gwenesis_sn76489_buffer[i] != 0;
#ifdef ARMBENCH
    arm_mark(c);
#endif
}

/* ---- profiling -------------------------------------------------------------- */
enum { P_68K, P_Z80, P_VDP, P_CONV, P_SND, P_OTHER, P_N };
static double ptime[P_N];
static const char *pname[P_N] = { "68k", "z80", "vdp_line", "convert", "sound_c1", "frame_rest" };
static inline double now_s(void)
{
#ifdef ARMBENCH
    return 0;
#else
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
#endif
}
#ifdef ARMBENCH
#define PROF(k, stmt) do { arm_mark(k); stmt; arm_mark(7); } while (0)
#else
#define PROF(k, stmt) do { if (prof) { double _t = now_s(); stmt; ptime[k] += now_s() - _t; } else { stmt; } } while (0)
#endif

/* ---- per-line CPU state ---------------------------------------------------- */
static uint64_t h_cpu;
static void hash_68k(void)
{
#ifdef ARMBENCH
    arm_mark(6);
#endif
    uint32_t r[19];
    r[0] = m68k_get_reg(M68K_REG_PC);
    r[1] = m68k_get_reg(M68K_REG_SR);
    for (int i = 0; i < 16; i++) r[2 + i] = m68k.dar[i];
    r[18] = m68k.cycles;
    h_cpu = hash_buf(h_cpu, r, sizeof r);
    { static int ld = -2; if (ld == -2) ld = getenv("HC_LINEDUMP") ? atoi(getenv("HC_LINEDUMP")) : -1;
      extern int dbg_frame;
      if (dbg_frame == ld) printf("L%d pc %06x sr %04x d0 %08x d1 %08x a0 %08x a7 %08x cyc %d\n", scan_line, r[0], r[1], r[2], r[3], r[10], r[17], r[18]); }
}
static void hash_z80(void)
{
#ifdef ARMBENCH
    arm_mark(6);
#endif
    h_cpu = hash_buf(h_cpu, (uint8_t *)&cpu + offsetof(Z80, data), sizeof(Z80) - offsetof(Z80, data));
    h_cpu = mix64(h_cpu, (uint64_t)(uint32_t)zclk);
}

static void service_core1(void)
{
    PROF(P_CONV, video_service());
    PROF(P_SND, while (1) { extern void ym_core1_service(void); ym_core1_service(); extern int hostcmp_ring_empty(void); if (hostcmp_ring_empty()) break; });
}


/* ---- VDP port fuzzer: -F STEPS --------------------------------------------- */
extern unsigned char SAT_CACHE[];
extern unsigned short fifo[];
void gwenesis_vdp_write_memory_16(unsigned int address, unsigned int value);
unsigned int gwenesis_vdp_read_memory_16(unsigned int address);
static uint32_t frng = 99;
static uint32_t fr32(void) { frng ^= frng << 13; frng ^= frng >> 17; frng ^= frng << 5; return frng; }
static void vdp_fuzz(long steps)
{
    uint64_t hr = 0;
    for (int i = 0; i < 0x10000; i++) M68K_RAM[i] = (uint8_t)fr32();
    gwenesis_vdp_reset();
    screen_width = 320; screen_height = 224;
    for (long st = 0; st < steps; st++) {
        scan_line = fr32() % 262;
        uint32_t r = fr32();
        switch (r % 10) {
        case 0: case 1: case 2: {                       /* register */
            static const uint8_t regs[] = { 1, 5, 12, 15, 15, 19, 20, 21, 22, 23, 23, 0, 10 };
            int reg = regs[(r >> 4) % sizeof regs];
            int v = (r >> 12) & 0xFF;
            if (reg == 1) v |= 0x14;                    /* DMA on, mode 5 */
            if (reg == 15 && (r & 0x100000)) v = (r >> 21) & 1 ? 2 : 1;
            if (reg == 20) v &= (r & 0x200000) ? 0xFF : 0x01;
            gwenesis_vdp_write_memory_16(0xC00004, 0x8000 | reg << 8 | v);
            break;
        }
        case 3: case 4: {                               /* command, maybe DMA */
            if (gwenesis_vdp_regs[19] == 0 && gwenesis_vdp_regs[20] == 0 && (gwenesis_vdp_regs[23] >> 6) == 3)
                gwenesis_vdp_write_memory_16(0xC00004, 0x9301);   /* no 4G-byte copies */
            static const uint8_t codes[] = { 1, 3, 5, 0, 4, 8, 0xC, 1, 1, 0x21, 0x21, 0x23, 0x25, 0x21 };
            int code = codes[(r >> 4) % sizeof codes];
            unsigned addr = (r >> 8) & 0xFFFF;
            if (r & 0x80) addr = (gwenesis_vdp_regs[5] << 9) + ((r >> 9) & 0x3FF);   /* near the SAT */
            addr &= 0xFFFF;
            gwenesis_vdp_write_memory_16(0xC00004, (code & 3) << 14 | (addr & 0x3FFF));
            gwenesis_vdp_write_memory_16(0xC00004, (code & 0x3C) << 2 | addr >> 14);
            break;
        }
        case 5: case 6: case 7:
            gwenesis_vdp_write_memory_16(0xC00000 + ((r >> 3) & 2), (r >> 8) & 0xFFFF);
            break;
        case 8:
            hr = mix64(hr, gwenesis_vdp_read_memory_16(0xC00000 + ((r >> 3) & 2)));
            break;
        default:
            hr = mix64(hr, gwenesis_vdp_read_memory_16(0xC00004));
            break;
        }
        if (st % 1000 == 999 || st == steps - 1) {
            uint64_t h = hash_buf(hr, VRAM, 0x10000);
            h = hash_buf(h, CRAM, 128); h = hash_buf(h, VSRAM, 80);
            h = hash_buf(h, SAT_CACHE, 0x400); h = hash_buf(h, gwenesis_vdp_regs, 0x20);
            h = hash_buf(h, fifo, 8); h = hash_buf(h, pbc_palette, 512);
            printf("%ld %016llx\n", st, (unsigned long long)h);
        }
    }
}

/* ---- VDP line renderer fuzzer: -L ITER --------------------------------------- */
extern int sprite_overflow;
void gwenesis_vdp_sprite_cache_dirty(void);
static void line_fuzz(long iters)
{
    uint64_t h = 0;
    for (long it = 0; it < iters; it++) {
        int dens = fr32() % 4;                       /* how many transparent pixels */
        for (int i = 0; i < 0x10000; i++) {
            uint8_t v = (uint8_t)fr32();
            if (dens >= 1 && (fr32() & 1)) v &= 0xF0;
            if (dens >= 2 && (fr32() & 1)) v &= 0x0F;
            if (dens == 3 && (fr32() % 4)) v = 0;
            VRAM[i] = v;
        }
        /* name tables: mostly sane tile numbers, all attribute bits */
        for (int i = 0; i < 40; i++) VSRAM[i] = (uint16_t)(fr32() & 0x7FF);
        static const uint8_t sizes[] = { 0x00, 0x01, 0x03, 0x10, 0x11, 0x30, 0x01, 0x01 };
        gwenesis_vdp_regs[0] = (fr32() % 64 == 0) ? 0x05 : 0x04;
        gwenesis_vdp_regs[1] = 0x44 | ((fr32() % 32 == 0) ? 0 : 0x40) | ((fr32() % 4 == 0) ? 0x08 : 0);
        gwenesis_vdp_regs[2] = fr32() & 0x38;
        gwenesis_vdp_regs[3] = fr32() & 0x3E;
        gwenesis_vdp_regs[4] = fr32() & 0x07;
        gwenesis_vdp_regs[5] = fr32() & 0x7F;
        gwenesis_vdp_regs[7] = fr32() & 0xFF;
        gwenesis_vdp_regs[11] = fr32() & 0x07;
        gwenesis_vdp_regs[12] = ((fr32() & 1) ? 0x81 : 0) | ((fr32() % 4 == 0) ? 0x08 : 0) | ((fr32() % 64 == 0) ? 0x02 : 0);
        gwenesis_vdp_regs[13] = fr32() & 0x3F;
        gwenesis_vdp_regs[16] = sizes[fr32() % 8];
        gwenesis_vdp_regs[17] = (fr32() % 3 == 0) ? (fr32() & 0x9F) : 0;
        gwenesis_vdp_regs[18] = (fr32() % 3 == 0) ? (fr32() & 0x9F) : 0;
        screen_width = (gwenesis_vdp_regs[12] & 1) ? 320 : 256;
        screen_height = (gwenesis_vdp_regs[1] & 8) ? 240 : 224;
        unsigned sat = (gwenesis_vdp_regs[5] & ((gwenesis_vdp_regs[12] & 1) ? 0x7E : 0x7F)) << 9;
        for (int i = 0; i < 0x400; i++) SAT_CACHE[i] = VRAM[(sat + i) & 0xFFFF];
        gwenesis_vdp_sprite_cache_dirty();
        sprite_overflow = 0;
        memset(video_screen, 0, sizeof video_screen);
        for (int y = 0; y < screen_height; y++)
            gwenesis_vdp_render_line(y);
        h = hash_buf(h, video_screen, sizeof video_screen);
        h = mix64(h, (uint64_t)sprite_overflow);
        if (it % 10 == 9 || it == iters - 1)
            printf("%ld %016llx\n", it, (unsigned long long)h);
#ifdef ARMBENCH
        arm_hash[it & 8191] = h;
#endif
    }
}

#define S2_GAME_MODE  (M68K_RAM[0xF600 ^ 1])
#define S2_IN_LEVEL   ((S2_GAME_MODE & 0x7F) == 0x0C)

uint8_t hc_rom[0x800010];
int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "hostcmp ROM FRAMES [-m crop] [-z ZONE] [-r SEED] [-t] [-q]\n"); return 2; }
    const char *rom_path = argv[1];
    int frames = atoi(argv[2]);
    int zone = -1;
    uint32_t seed = 1;
    bool quiet = false;
    int vmode = VIDEO_FIT;
    int dump_at = -1;
    long fuzz_steps = 0, line_iters = 0;
    for (int i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "-z") && i + 1 < argc) zone = (int)strtol(argv[++i], NULL, 0);
        else if (!strcmp(argv[i], "-r") && i + 1 < argc) seed = (uint32_t)strtoul(argv[++i], NULL, 0);
        else if (!strcmp(argv[i], "-m") && i + 1 < argc) vmode = !strcmp(argv[++i], "crop") ? VIDEO_CROP : VIDEO_FIT;
        else if (!strcmp(argv[i], "-t")) prof = true;
        else if (!strcmp(argv[i], "-q")) quiet = true;
        else if (!strcmp(argv[i], "-F") && i + 1 < argc) fuzz_steps = atol(argv[++i]);
        else if (!strcmp(argv[i], "-L") && i + 1 < argc) line_iters = atol(argv[++i]);
        else if (!strcmp(argv[i], "-d") && i + 1 < argc) dump_at = atoi(argv[++i]);
    }

#ifdef ARMBENCH
    size_t n = arm_rom_len;
    uint8_t *rom = hc_rom;
#else
    uint8_t *rom = hc_rom;
    FILE *f = fopen(rom_path, "rb");
    if (!f) { perror(rom_path); return 1; }
    size_t n = fread(rom, 1, 0x500000, f);
    fclose(f);
#endif
    for (size_t i = 0; i + 1 < n; i += 2) { uint8_t t = rom[i]; rom[i] = rom[i + 1]; rom[i + 1] = t; }
    ROM_DATA = rom;

    memset(M68K_RAM, 0, MAX_RAM_SIZE);
    memset(ZRAM, 0, MAX_Z80_RAM_SIZE);
    z80_set_memory(ZRAM);
    z80_start();
    z80_pulse_reset();
    m68k_init();
    m68k_pulse_reset();
    YM2612Init();
    YM2612ResetChip();
    YM2612Config(YM2612_DISCRETE);
    gwenesis_SN76489_Init(3579545, 888 * 60, AUDIO_FREQ_DIVISOR, PSG_INTEGRATED);
    gwenesis_SN76489_Reset();
    hdmi_line_lut_reset();
    for (int i = 0; i < 256; i++) pbc_palette[i] = 0;
    gwenesis_vdp_reset();
    gwenesis_vdp_set_buffer(&video_screen[0][0]);
    memset(video_screen, 0, sizeof video_screen);
    set_region();
    extern unsigned char io_reg[];
    bool region_pal = (io_reg[0] & 0x40) != 0;
    if (region_pal) gwenesis_vdp_status |= 1;
    video_set_mode(vmode);
    if (fuzz_steps) { vdp_fuzz(fuzz_steps); return 0; }
    if (line_iters) { gwenesis_vdp_reset(); line_fuzz(line_iters); return 0; }
    ym_core1_init();
    screen_width = 320;
    screen_height = 224;
    gwenesis_vdp_render_config();

    uint32_t rng = seed * 2654435761u + 12345;
    uint8_t held = 0xFF;
    int hold_left = 0;
    int last_w = 0, last_h = 0, level_frames = 0;
    double t_start = now_s();
    for (int fr = 0; fr < frames; fr++) {
        dbg_frame = fr;
#ifdef PCHIST
        if (!pchist) pchist = calloc(1 << 24, 4);
        pchist_on = fr >= (getenv("HC_PCFROM") ? atoi(getenv("HC_PCFROM")) : 0);
#endif
#ifdef ARMBENCH
        arm_frame(fr);
#endif
#ifdef ARMBENCH
        arm_mark(7);
#endif
        uint8_t pad = 0xFF;                          /* active low: S A C B R L D U */
        if (zone >= 0) {
            level_frames = S2_IN_LEVEL ? level_frames + 1 : 0;
            if (!level_frames && fr % 60 < 5) pad &= ~0x80;
            if (level_frames > 200) {
                pad &= ~0x08;
                if (fr % 90 < 25) pad &= ~0x40;
                if (fr % 300 > 200) pad = (pad | 0x08) & ~0x02;
            }
        } else if (fr < 1500) {
            if (fr % 120 < 5) pad &= ~0x80;
        } else {
            if (--hold_left <= 0) {
                rng = rng * 1103515245u + 12345u;
                uint32_t r = rng >> 8;
                static const uint8_t dirs[9] = { 0, 1, 2, 4, 8, 5, 9, 6, 10 };
                uint8_t p = dirs[r % 9];
                r /= 9;
                if (r % 10 < 3) p |= 0x10;   /* B */
                r /= 10;
                if (r % 10 < 3) p |= 0x20;   /* C */
                r /= 10;
                if (r % 10 < 4) p |= 0x40;   /* A */
                r /= 10;
                if (r % 50 == 0) p |= 0x80;  /* START */
                r /= 50;
                held = (uint8_t)~p;
                hold_left = 6 + r % 40;
            }
            pad = held;
        }
        button_state[0] = pad;
        button_state[1] = 0xFF;
        button_state[2] = 0xFF;

        int hint_counter = gwenesis_vdp_regs[10];
        screen_width = REG12_MODE_H40 ? 320 : 256;
        screen_height = REG1_PAL ? 240 : 224;
        lines_per_frame = region_pal ? LINES_PER_FRAME_PAL : LINES_PER_FRAME_NTSC;
        if (screen_width != last_w || screen_height != last_h) {
            gwenesis_vdp_render_config();
            last_w = screen_width;
            last_h = screen_height;
        }
        video_frame_begin(screen_width, screen_height);
        h_panel_rows = 0;
        system_clock = 0;
        scan_line = 0;
        vdp_palette_frame_begin();
        zclk = 0;
        ym2612_clock = ym2612_index = 0;
        h_cpu = 0;

        while (scan_line < (int)lines_per_frame) {
#ifdef ARMBENCH
            arm_mark(7);
#endif
            const uint8_t *line_lut = vdp_palette_current_lut();
            if (zone >= 0 && level_frames < 2) {
                M68K_RAM[0xFE10 ^ 1] = (uint8_t)(zone >> 8);
                M68K_RAM[0xFE11 ^ 1] = (uint8_t)zone;
            }
            if (scan_line == screen_height) {
                video_palette_freeze();
                PROF(P_68K, m68k_run(system_clock + 788));
                hash_68k();
                if (REG1_VBLANK_INTERRUPT != 0) {
                    gwenesis_vdp_status |= STATUS_VIRQPENDING;
                    m68k_set_irq(6);
                }
            }
            PROF(P_68K, m68k_run(system_clock + VDP_CYCLES_PER_LINE));
            hash_68k();
            if (scan_line % 16 == 15 || scan_line == (int)lines_per_frame - 1 ||
                scan_line == screen_height - 1 || scan_line == screen_height) {
                PROF(P_Z80, z80_run(system_clock + VDP_CYCLES_PER_LINE));
                hash_z80();
            }
            if (scan_line == 0 || scan_line > screen_height)
                hint_counter = gwenesis_vdp_regs[10];
            if (--hint_counter < 0) {
                if (REG0_LINE_INTERRUPT != 0 && scan_line <= screen_height) {
                    hint_pending = 1;
                    if ((gwenesis_vdp_status & STATUS_VIRQPENDING) == 0)
                        m68k_update_irq(4);
                }
                hint_counter = gwenesis_vdp_regs[10];
            }
            if (scan_line < screen_height) {
                hdmi_line_lut[scan_line] = line_lut;
                PROF(P_VDP, gwenesis_vdp_render_line(scan_line));
                video_line_done(scan_line);
            }
            service_core1();
            scan_line++;
            if (scan_line == screen_height) z80_irq_line(1);
            if (scan_line == screen_height + 1) z80_irq_line(0);
            system_clock += VDP_CYCLES_PER_LINE;
        }
        PROF(P_OTHER, ym2612_run(system_clock));
        ym_core1_frame_end(system_clock, region_pal);
        service_core1();
        while (!video_idle()) service_core1();
        frame_counter++;
        m68k.cycles -= system_clock;

#ifdef ARMBENCH
        arm_mark(6);
#endif
        uint64_t h_vdp = hash_buf(0, video_screen, sizeof video_screen);
        uint64_t h_ram = hash_buf(0, M68K_RAM, 0x10000);
        uint64_t h_zram = hash_buf(0, ZRAM, 0x2000);
        uint64_t h_vram = hash_buf(0, VRAM, 0x10000);
        h_vram = hash_buf(h_vram, CRAM, 64 * 2);
        h_vram = hash_buf(h_vram, VSRAM, 40 * 2);
        h_vram = hash_buf(h_vram, gwenesis_vdp_regs, 24);
        h_vram = mix64(h_vram, gwenesis_vdp_status);
        if (fr == dump_at) {
            char fn[64]; snprintf(fn, sizeof fn, "panel%05d.ppm", fr);
            FILE *pf = fopen(fn, "wb");
            fprintf(pf, "P6 240 280 255\n");
            for (int y = 0; y < 280; y++) for (int x = 0; x < 240; x++) {
                uint16_t c = panel[y][x];
                fputc((c & 31) << 3, pf); fputc(((c >> 5) & 63) << 2, pf); fputc((c >> 11) << 3, pf);
            }
            fclose(pf);
            long nz = 0; for (int i = 0; i < synth_ym2612_index; i++) nz += synth_ym2612_buffer[i] != 0;
            fprintf(stderr, "dump %d: ym nonzero %ld\n", fr, nz);
        }
        uint64_t h_fold = mix64(mix64(mix64(mix64(mix64(mix64(mix64(mix64(mix64(h_panel_rows, h_vdp), h_ym), h_psg), h_pwm), h_ram), h_zram), h_vram), 0), fr);
#ifdef ARMBENCH
        arm_hash[fr & 8191] = h_fold;
        if (fr < 1024) { uint64_t *c = arm_comp[fr]; c[0] = h_panel_rows; c[1] = h_vdp; c[2] = h_ym; c[3] = h_psg; c[4] = h_pwm; c[5] = h_ram; c[6] = h_zram; c[7] = h_vram; c[8] = h_cpu; }
#endif
        if (getenv("HC_FOLD")) { printf("%05d %016llx %016llx %016llx %016llx %016llx %016llx %016llx %016llx\n", fr, (unsigned long long)h_panel_rows, (unsigned long long)h_vdp, (unsigned long long)h_ym, (unsigned long long)h_psg, (unsigned long long)h_pwm, (unsigned long long)h_ram, (unsigned long long)h_zram, (unsigned long long)h_vram); continue; }
        if (!quiet)
            printf("%05d pic %016llx vdp %016llx ym %016llx/%ld psg %016llx/%ld pwm %016llx/%ld ram %016llx z %016llx vram %016llx cpu %016llx\n",
                   fr, (unsigned long long)h_panel_rows, (unsigned long long)h_vdp,
                   (unsigned long long)h_ym, n_ym, (unsigned long long)h_psg, n_psg,
                   (unsigned long long)h_pwm, n_pwm, (unsigned long long)h_ram,
                   (unsigned long long)h_zram, (unsigned long long)h_vram, (unsigned long long)h_cpu);
    }
#ifdef PCHIST
    {
        uint64_t tot = 0;
        for (int i = 0; i < 1 << 24; i++) tot += pchist[i];
        for (int k = 0; k < 40; k++) {
            int best = 0;
            for (int i = 0; i < 1 << 24; i++) if (pchist[i] > pchist[best]) best = i;
            if (!pchist[best]) break;
            unsigned op = m68k_read_memory_16(best);
            fprintf(stderr, "%06x %5.2f%%  %04x %04x %04x\n", best, pchist[best] * 100.0 / tot, op, m68k_read_memory_16(best + 2), m68k_read_memory_16(best + 4));
            pchist[best] = 0;
        }
    }
#endif
#ifndef ARMBENCH
#endif
    double tt = now_s() - t_start;
    if (prof) {
        double sum = 0;
        for (int i = 0; i < P_N; i++) sum += ptime[i];
        fprintf(stderr, "total %.3f s (%d frames)", tt, frames);
        for (int i = 0; i < P_N; i++) fprintf(stderr, "  %s %.3f", pname[i], ptime[i]);
        fprintf(stderr, "  unaccounted %.3f\n", tt - sum);
    }
    fprintf(stderr, "samples ym %ld (nonzero %ld) psg %ld (nonzero %ld) pwm %ld\n", n_ym, nz_ym, n_psg, nz_psg, n_pwm);
    if (zone >= 0)
        fprintf(stderr, "Sonic 2: game mode %02x, zone/act %02x%02x\n", S2_GAME_MODE, M68K_RAM[0xFE10 ^ 1], M68K_RAM[0xFE11 ^ 1]);
    return 0;
}
