/* host.c -- the PBC Mega Drive core as a PC program, for finding picture
 * bugs without flashing.
 *
 * Runs the same core sources as the firmware (68000, Z80, VDP, YM2612
 * timers); the few ARM assembly pieces are replaced by the C below or by
 * the core's own C versions. The frame loop follows emulation_loop() in
 * src/main.c, including the per-line palette tables of video.c, so
 * mid-frame palette effects come out as on the device. Not covered: the
 * timing between the two cores (core 1 converting rows late), sound.
 * Keep the loop in step with src/main.c when that changes.
 *
 *   mdhost ROM FRAMES [-z ZONE [-s]] [-d FROM[:EVERY]] [-o DIR] [-p]
 *
 *   -z ZONE   Sonic 2: press START until a level runs, start it in this
 *             zone/act (0x0D01 = Chemical Plant 2), then run right and
 *             jump. Without -z: START every two seconds for 20 seconds.
 *   -s        ... and 100 frames into the level on to the special stage
 *             (Game_Mode $10, as a star post with 50 rings sets it);
 *             MDHOST_TIME=1 prints the time each frame takes
 *   -d        write frames from FROM on, every EVERY-th (default 1), as
 *             DIR/fNNNNN.ppm (default: only the last frame)
 *   -p        print per frame from which line how many of the 64 colours
 *             are redirected by a raster split
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "bus/gwenesis_bus.h"
#include "io/gwenesis_io.h"
#include "vdp/gwenesis_vdp.h"
#include "cpus/M68K/m68k.h"
#include "sound/z80inst.h"
#include "sound/ym2612.h"
#include "sound/gwenesis_sn76489.h"
#include "cpus/Z80/Z80.h"
#include "HDMI.h"

/* ---- globals the core expects (src/main.c) ------------------------------- */
extern unsigned char *ROM_DATA;
extern unsigned char M68K_RAM[];
extern unsigned char ZRAM[];
extern unsigned char gwenesis_vdp_regs[];
extern unsigned short gwenesis_vdp_status;
extern int hint_pending;
extern int screen_width, screen_height;
extern unsigned char button_state[];
int system_clock;
unsigned int lines_per_frame = LINES_PER_FRAME_NTSC;
int scan_line;
unsigned int frame_counter;
bool audio_enabled = false, sn76489_enabled = false, ym2612_enabled = false, z80_enabled = true;
static int16_t psg_buf[GWENESIS_AUDIO_BUFFER_MAX];
int16_t *gwenesis_sn76489_buffer = psg_buf;
int16_t *gwenesis_ym2612_buffer;
volatile int sn76489_index, sn76489_clock, ym2612_index, ym2612_clock;
void gwenesis_io_get_buttons(void) {}

/* ---- C for core/cpus/M68K/m68k_memory_opt.S ------------------------------ */
uint32_t m68k_read_rom16_fast(uint32_t a) { return *(uint16_t *)(ROM_DATA + a); }
uint32_t m68k_read_rom32_fast(uint32_t a)
{
    uint32_t v;
    memcpy(&v, ROM_DATA + a, 4);
    return (v >> 16) | (v << 16);
}
uint32_t m68k_read_ram16_fast(uint32_t a) { return *(uint16_t *)(M68K_RAM + (a & 0xFFFF)); }
void m68k_write_ram16_fast(uint32_t a, uint32_t v) { *(uint16_t *)(M68K_RAM + (a & 0xFFFF)) = v; }
void m68k_write_ram8_fast(uint32_t a, uint32_t v) { M68K_RAM[(a & 0xFFFF) ^ 1] = v; }

/* ---- C for core/sound/z80_mem_opt.S and core/cpus/Z80/z80_arm.S ---------- */
extern Z80 cpu;
extern volatile int zclk;
extern int current_timeslice;
extern unsigned char *Z80_RAM;
unsigned int zbank_mem_r8(unsigned int);
void zbank_mem_w8(unsigned int, unsigned int);
void zbankreg_mem_w8(unsigned int);

static int z80_time(void) { return zclk + current_timeslice - cpu.ICount * Z80_FREQ_DIVISOR; }

byte RdZ80(word a)
{
    if (a < 0x4000) return Z80_RAM[a & 0x1FFF];
    if (a < 0x6000) return YM2612Read(z80_time());
    if (a >= 0x8000) return zbank_mem_r8(a);
    return 0xFF;
}

void WrZ80(word a, byte v)
{
    if (a < 0x4000)       Z80_RAM[a & 0x1FFF] = v;
    else if (a < 0x6000)  YM2612Write(a & 3, v, z80_time());
    else if (a == 0x6000) zbankreg_mem_w8(v);
    else if (a == 0x7F11) gwenesis_SN76489_Write(v, z80_time());
    else if (a >= 0x8000) zbank_mem_w8(a, v);
}

int z80_arm_exec(int cycles) { return ExecZ80(&cpu, cycles); }

/* ---- palette path, as src/video.c ----------------------------------------- */
const uint8_t *hdmi_line_lut[HDMI_MAX_LINES];
uint8_t hdmi_identity_lut[256];
uint16_t pbc_palette[256];
static uint32_t pal888[256];

void graphics_set_palette(uint8_t i, uint32_t c) { pal888[i] = c; }

void hdmi_line_lut_reset(void)
{
    for (int i = 0; i < 256; i++) hdmi_identity_lut[i] = i & 0x3F;
    for (int i = 0; i < HDMI_MAX_LINES; i++) hdmi_line_lut[i] = hdmi_identity_lut;
}

static uint8_t screen[240][320];
static uint8_t rgb[240][320][3];

static void convert_line(int y)
{
    const uint8_t *lut = hdmi_line_lut[y];
    for (int x = 0; x < screen_width; x++) {
        uint32_t c = pal888[lut[screen[y][x]]];
        rgb[y][x][0] = c >> 16;
        rgb[y][x][1] = c >> 8;
        rgb[y][x][2] = c;
    }
}

static void save_ppm(const char *dir, int frame)
{
    char fn[512];
    snprintf(fn, sizeof fn, "%s/f%05d.ppm", dir, frame);
    FILE *f = fopen(fn, "wb");
    if (!f) { perror(fn); exit(1); }
    fprintf(f, "P6 %d %d 255\n", screen_width, screen_height);
    for (int y = 0; y < screen_height; y++)
        fwrite(rgb[y], 3, screen_width, f);
    fclose(f);
}

static void report_splits(int frame)
{
    const uint8_t *prev = NULL;
    printf("frame %d:", frame);
    for (int y = 0; y < screen_height; y++) {
        if (hdmi_line_lut[y] != prev) {
            int n = 0;
            for (int i = 0; i < 64; i++) n += hdmi_line_lut[y][i] != i;
            printf(" line %d: %d", y, n);
            prev = hdmi_line_lut[y];
        }
    }
    printf("\n");
}

/* ---- Sonic 2 RAM ($FFxxxx, word-swapped like the core keeps it) ----------- */
#define S2_GAME_MODE  (M68K_RAM[0xF600 ^ 1])
#define S2_IN_LEVEL   ((S2_GAME_MODE & 0x7F) == 0x0C)

static void usage(void)
{
    fprintf(stderr, "mdhost ROM FRAMES [-z ZONE [-s]] [-d FROM[:EVERY]] [-o DIR] [-p]\n");
    exit(2);
}

int main(int argc, char **argv)
{
    if (argc < 3) usage();
    const char *rom_path = argv[1];
    int frames = atoi(argv[2]);
    int zone = -1, dump_from = frames - 1, dump_every = 1;
    bool splits = false, special = false;
    const char *dir = ".";
    for (int i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "-z") && i + 1 < argc) zone = (int)strtol(argv[++i], NULL, 0);
        else if (!strcmp(argv[i], "-d") && i + 1 < argc) {
            dump_from = atoi(argv[++i]);
            const char *c = strchr(argv[i], ':');
            if (c) dump_every = atoi(c + 1) > 0 ? atoi(c + 1) : 1;
        }
        else if (!strcmp(argv[i], "-o") && i + 1 < argc) dir = argv[++i];
        else if (!strcmp(argv[i], "-p")) splits = true;
        else if (!strcmp(argv[i], "-s")) special = true;
        else usage();
    }

    static uint8_t rom[0x500000];
    FILE *f = fopen(rom_path, "rb");
    if (!f) { perror(rom_path); return 1; }
    size_t n = fread(rom, 1, sizeof rom, f);
    fclose(f);
    for (size_t i = 0; i + 1 < n; i += 2) {        /* byte-swapped, as rom_store.c */
        uint8_t t = rom[i]; rom[i] = rom[i + 1]; rom[i + 1] = t;
    }
    ROM_DATA = rom;

    /* genesis_init() in src/main.c */
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
    gwenesis_vdp_reset();
    gwenesis_vdp_set_buffer(&screen[0][0]);
    screen_width = 320;
    screen_height = 224;
    gwenesis_vdp_render_config();

    int last_w = 0, last_h = 0, level_frames = 0;
    for (int fr = 0; fr < frames; fr++) {
        /* ---- input ---- */
        uint8_t pad = 0xFF;                          /* active low: S A C B R L D U */
        if (zone >= 0) {
            level_frames = S2_IN_LEVEL ? level_frames + 1 : 0;
            if (!level_frames && fr % 60 < 5 && !(special && S2_GAME_MODE == 0x10))
                pad &= ~0x80;                                             /* START */
            if (special && level_frames == 100)
                S2_GAME_MODE = 0x10;                                      /* special stage */
            if (level_frames > 200) {
                pad &= ~0x08;                                             /* right */
                if (fr % 90 < 25) pad &= ~0x40;                           /* A: jump */
                if (fr % 300 > 200) pad = (pad | 0x08) & ~0x02;           /* down */
            }
        } else if (fr < 1200 && fr % 120 < 5) {
            pad &= ~0x80;
        }
        button_state[0] = pad;
        button_state[1] = 0xFF;
        button_state[2] = 0xFF;

        /* ---- frame, as emulation_loop() in src/main.c ---- */
        int hint_counter = gwenesis_vdp_regs[10];
        screen_width = REG12_MODE_H40 ? 320 : 256;
        screen_height = REG1_PAL ? 240 : 224;
        lines_per_frame = REG1_PAL ? LINES_PER_FRAME_PAL : LINES_PER_FRAME_NTSC;
        if (screen_width != last_w || screen_height != last_h) {
            gwenesis_vdp_render_config();
            last_w = screen_width;
            last_h = screen_height;
        }
        system_clock = 0;
        scan_line = 0;
        vdp_palette_frame_begin();
        zclk = 0;
        sn76489_clock = sn76489_index = 0;
        ym2612_clock = ym2612_index = 0;

        while (scan_line < (int)lines_per_frame) {
            const uint8_t *line_lut = vdp_palette_current_lut();
            if (zone >= 0 && level_frames < 2) {    /* Current_ZoneAndAct $FFFE10 */
                M68K_RAM[0xFE10 ^ 1] = (uint8_t)(zone >> 8);
                M68K_RAM[0xFE11 ^ 1] = (uint8_t)zone;
            }
            if (scan_line == screen_height) {
                m68k_run(system_clock + 788);
                if (REG1_VBLANK_INTERRUPT != 0) {
                    gwenesis_vdp_status |= STATUS_VIRQPENDING;
                    m68k_set_irq(6);
                }
            }
            m68k_run(system_clock + VDP_CYCLES_PER_LINE);
            if (scan_line % 16 == 15 || scan_line == (int)lines_per_frame - 1 ||
                scan_line == screen_height - 1 || scan_line == screen_height)
                z80_run(system_clock + VDP_CYCLES_PER_LINE);
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
                gwenesis_vdp_render_line(scan_line);
                convert_line(scan_line);
            }
            scan_line++;
            if (scan_line == screen_height)
                z80_irq_line(1);
            if (scan_line == screen_height + 1)
                z80_irq_line(0);
            system_clock += VDP_CYCLES_PER_LINE;
        }
        ym2612_run(system_clock);
        frame_counter++;
        m68k.cycles -= system_clock;

        {
            static struct timespec t0;
            struct timespec t1;
            clock_gettime(CLOCK_MONOTONIC, &t1);
            if (getenv("MDHOST_TIME") && fr)
                printf("frame %d mode %02x %ld us\n", fr, S2_GAME_MODE,
                       (t1.tv_sec - t0.tv_sec) * 1000000 + (t1.tv_nsec - t0.tv_nsec) / 1000);
            t0 = t1;
        }
        if (fr >= dump_from && (fr - dump_from) % dump_every == 0) {
            save_ppm(dir, fr);
            if (splits)
                report_splits(fr);
        }
    }
    if (zone >= 0)
        printf("Sonic 2: game mode %02x, zone/act %02x%02x\n", S2_GAME_MODE,
               M68K_RAM[0xFE10 ^ 1], M68K_RAM[0xFE11 ^ 1]);
    return 0;
}
