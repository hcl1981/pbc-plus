/*
 * PBC Mega Drive -- Sega Genesis / Mega Drive emulator for the
 * PicoBoy Color Plus (RP2350A, 520 KB SRAM, 16 MB flash, ST7789 240x280).
 *
 * Emulator core: frank-genesis (Gwenesis based) by rh1tech, AGPL-3.0.
 * What changed for this device:
 *   - no PSRAM: the ROM is executed in place from flash (rom_store.c)
 *   - no SD card: ROMs come over USB mass storage into a flash FatFs
 *     partition, taken from PicoBoyGB (flash_fs.c, usb_msc.c)
 *   - no HDMI: the frame is scaled to 240 px wide and streamed to the
 *     ST7789 while it is being drawn (video.c)
 *   - sound: core 0 logs the YM2612/PSG writes, core 1 synthesises them
 *     at the chip's own 53 kHz between drawing lines and averages down to
 *     22 kHz for the PWM speaker (ym_core1.c, audio_pwm.c as in ../sms).
 *     With sound off only the YM2612 timers tick, so games behave the same
 *
 * Boot:
 *   A or B held  -> USB stick mode (drive "PBC-ROMS"), eject restarts
 *   otherwise    -> ROM list, then the game
 */
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "hardware/spi.h"
#include "hardware/watchdog.h"
#include "hardware/sync.h"
#include "hardware/structs/qmi.h"
#include "hardware/structs/scb.h"
#include "hardware/dma.h"

#include "ff.h"
#include "tusb.h"

#include "board.h"
#include "pbc_launch.h"
#include "color565.h"
#include "display.h"
#include "input.h"
#include "menu.h"
#include "rom_store.h"
#include "ssf2.h"
#include "video.h"
#include "audio_pwm.h"
#include "ym_core1.h"
#include "HDMI.h"

#include "bus/gwenesis_bus.h"
#include "io/gwenesis_io.h"
#include "vdp/gwenesis_vdp.h"
#include "cpus/M68K/m68k.h"
#include "sound/z80inst.h"
#include "sound/ym2612.h"
#include "sound/gwenesis_sn76489.h"
#include "sound/sound_backend.h"

/* ---- build-time configuration (see CMakeLists.txt) ---------------------- */
#ifndef CPU_MHZ
#define CPU_MHZ 200
#endif
#ifndef CPU_VREG
#define CPU_VREG VREG_VOLTAGE_1_15
#endif
#ifndef FLASH_MAX_FREQ_MHZ
#define FLASH_MAX_FREQ_MHZ 100
#endif
#ifndef SHOW_FPS
#define SHOW_FPS 0
#endif

/* fps/speed/CPU line: SHOW_FPS on its own, the launcher's setting in the
 * combined firmware. */
static bool show_stats;
#ifndef Z80_SLICE_LINES
#define Z80_SLICE_LINES 16
#endif
/* Never skip more than this many frames in a row, however slow we are. */
#define MAX_SKIP 3

/* ---- globals the emulator core expects (frank-genesis kept them in main.c) */
extern unsigned char *ROM_DATA;
extern unsigned char M68K_RAM[];
extern unsigned char ZRAM[];
extern unsigned char gwenesis_vdp_regs[];
extern unsigned short gwenesis_vdp_status;
extern int hint_pending;
extern int screen_width;
extern int screen_height;
extern unsigned char button_state[];

int system_clock;
unsigned int lines_per_frame = LINES_PER_FRAME_NTSC;
int scan_line;
unsigned int frame_counter;

bool audio_enabled   = false;          /* sound on: synthesise YM2612 + PSG */
bool sn76489_enabled = false;
bool ym2612_enabled  = false;
bool z80_enabled     = true;

/* PSG samples, made on core 1 (ym_core1.c); core 0's YM2612 copy only
 * keeps the timers and never writes samples. */
static int16_t psg_buf[GWENESIS_AUDIO_BUFFER_MAX];
int16_t *gwenesis_sn76489_buffer = psg_buf;
int16_t *gwenesis_ym2612_buffer;
volatile int sn76489_index, sn76489_clock;
volatile int ym2612_index, ym2612_clock;

/* Input is sampled once per frame in the main loop; the core asks for it on
 * every pad read, so this has to stay cheap. */
void gwenesis_io_get_buttons(void) { }

/* ---- FatFs volume -------------------------------------------------------- */
static FATFS g_fs;
extern volatile bool usb_msc_ejected;

static bool fs_mount(void)   { return f_mount(&g_fs, "", 1) == FR_OK; }
static void fs_unmount(void) { f_mount(NULL, "", 0); }

static bool fs_format(void)
{
    static BYTE work[FF_MAX_SS];
    /* FM_SFD: no partition table, one FAT volume filling the region. */
    MKFS_PARM opt = { FM_FAT | FM_SFD, 0, 0, 0, 0 };
    if (f_mkfs("", &opt, work, sizeof work) != FR_OK)
        return false;
    if (fs_mount()) {
        f_setlabel("PBC-ROMS");
        fs_unmount();
    }
    return true;
}

static void fs_write_readme(void)
{
    /* static: a FIL carries a 4 KB sector buffer, the stack is only 4 KB */
    static FILINFO fi;
    static FIL f;
    if (f_stat("/MEGADRIVE.TXT", &fi) == FR_OK)
        return;
    if (f_open(&f, "/MEGADRIVE.TXT", FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
        return;
    f_puts("PBC Mega Drive - Genesis / Mega Drive emulator for the PicoBoy Color Plus\r\n"
           "\r\n"
           "Copy ROMs (.md .bin .gen .smd, up to 5 MB) to this drive, then EJECT\r\n"
           "it. The PicoBoy restarts by itself and shows the game list.\r\n"
           "\r\n"
           "Back to this mode: \"Add games via USB\" at the top of the game list,\r\n"
           "or hold A or B while switching on.\r\n"
           "\r\n"
           "Controls in game:\r\n"
           "  Joystick       D-pad\r\n"
           "  A / B          A / B\r\n"
           "  Center         C\r\n"
           "  Center + B     START\r\n"
           "\r\n"
           "Back to the game list: RESET.\r\n", &f);
    f_close(&f);
}

/* megadrive and nes share one drive. Whichever formatted it first used to
 * name it after itself; give it the common name in either case. */
static void fs_ensure_label(void)
{
    char label[24];
    if (f_getlabel("", label, NULL) == FR_OK && strcmp(label, "PBC-ROMS") != 0)
        f_setlabel("PBC-ROMS");
}

/* One diagnostic line per step: the panel is the only console this board
 * has, and a hang between two steps is then visible at a glance. */
static int diag_y;

static void diag(const char *s, uint16_t colour)
{
    pbc_display_text(8, diag_y, s, colour, PBC_BLACK, 1);
    diag_y += 11;
}

static void __attribute__((noreturn)) usb_stick_mode(void)
{
    menu_message("USB DRIVE MODE",
                 "Copy ROMs to the drive",
                 "PBC-ROMS, then eject it.",
                 "The PicoBoy restarts.");
    diag_y = 168;

    diag("Checking drive...", PBC_GREY);
    FRESULT fr = f_mount(&g_fs, "", 1);
    if (fr != FR_OK) {
        char m[32];
        snprintf(m, sizeof m, "empty (%d), formatting...", (int)fr);
        diag(m, PBC_YELLOW);
        if (fs_format() && fs_mount())
            fr = FR_OK;
    }
    if (fr != FR_OK) {
        diag("ERROR: drive unusable", PBC_RED);
        for (;;) tight_loop_contents();
    }
    diag("Writing MEGADRIVE.TXT...", PBC_GREY);
    fs_ensure_label();
    fs_write_readme();
    fs_unmount();               /* the host owns the medium from now on */

    diag("Starting USB...", PBC_GREY);
    tud_init(BOARD_TUD_RHPORT);
    diag("USB started", PBC_GREY);

    int status_y = diag_y;
    uint32_t loops = 0;
    uint64_t next_draw = 0;
    for (;;) {
        tud_task();
        loops++;
        if (time_us_64() >= next_draw) {
            next_draw = time_us_64() + 500000;
            int state = tud_mounted() ? 2 : tud_connected() ? 1 : 0;
            static const char *const names[] = {
                "PC: not connected  ", "PC: connected      ", "PC: drive mounted  " };
            char m[40];
            snprintf(m, sizeof m, "%-22s %5lu", names[state], (unsigned long)(loops % 100000));
            pbc_display_text(8, status_y, m, state == 2 ? PBC_YELLOW : PBC_WHITE, PBC_BLACK, 1);
        }
        if (usb_msc_ejected) {
            uint64_t t = time_us_64() + 300000;
            while (time_us_64() < t)
                tud_task();     /* let the host see the eject complete */
            watchdog_reboot(0, 0, 10);
            for (;;) tight_loop_contents();
        }
    }
}

/* ---- clocks -------------------------------------------------------------- */

/* From frank-genesis: keep the flash (and therefore XIP, which now also
 * serves the ROM) under FLASH_MAX_FREQ_MHZ whatever the system clock. Must
 * run from RAM while the QMI timing changes. */
static void __no_inline_not_in_flash_func(set_flash_timings)(int cpu_mhz)
{
    const int clock_hz = cpu_mhz * 1000000;
    const int max_flash_freq = FLASH_MAX_FREQ_MHZ * 1000000;

    int divisor = (clock_hz + max_flash_freq - (max_flash_freq >> 4) - 1) / max_flash_freq;
    if (divisor == 1 && clock_hz >= 166000000)
        divisor = 2;

    int rxdelay = divisor;
    if (clock_hz / divisor > 100000000 && clock_hz >= 166000000)
        rxdelay += 1;

    qmi_hw->m[0].timing = 0x60007000 |
                          rxdelay << QMI_M0_TIMING_RXDELAY_LSB |
                          divisor << QMI_M0_TIMING_CLKDIV_LSB;
}

/* Raised only after all flash writing (USB, ROM programming) is done, so
 * those always run at the stock clock and flash timing. */
static void overclock(void)
{
    if (CPU_MHZ > 150) {
        if (CPU_VREG > VREG_VOLTAGE_1_30)
            vreg_disable_voltage_limit();
        vreg_set_voltage(CPU_VREG);
        sleep_ms(10);
        set_flash_timings(CPU_MHZ);
        if (!set_sys_clock_khz(CPU_MHZ * 1000, false)) {
            set_flash_timings(250);
            set_sys_clock_khz(250000, true);
        }
    } else if (CPU_MHZ < 150) {
        /* below the boot clock: the boot flash timing stays valid */
        set_sys_clock_khz(CPU_MHZ * 1000, false);
    }
    /* clk_peri follows clk_sys: re-derive the SPI divider. */
    spi_set_baudrate(PBC_SPI, PBC_SPI_HZ);
}

/* ---- parallel line rendering (from frank-genesis, VDP_RASTER_RENDER) ----
 *
 * Core 1 draws line N while core 0 emulates line N+1. If core 1 is busy
 * (pushing pixels to the panel), core 0 reclaims the line and draws it
 * itself, which is exactly the single-core behaviour. Either way the line
 * is reported to the scaler right after it is drawn. */
static volatile int  vdp_job_line;
static volatile bool vdp_job_done;
static volatile bool vdp_job_busy;
static volatile int  vdp_job_claim;

static inline bool vdp_job_take(void)
{
    return __atomic_exchange_n(&vdp_job_claim, 0, __ATOMIC_ACQUIRE) == 1;
}

static inline void render_line(int line)
{
    gwenesis_vdp_render_line(line);
    video_line_done(line);
}

static void vdp_pipeline_sync(void)
{
    if (!vdp_job_busy)
        return;
    if (vdp_job_take()) {
        render_line(vdp_job_line);          /* core 1 never took it */
    } else {
        while (!vdp_job_done)
            tight_loop_contents();
    }
    vdp_job_done = false;
    vdp_job_busy = false;
}

static inline void vdp_pipeline_post(int line)
{
    vdp_pipeline_sync();
    vdp_job_line = line;
    vdp_job_busy = true;
    __atomic_store_n(&vdp_job_claim, 1, __ATOMIC_RELEASE);
}

static void core1_main(void)
{
    for (;;) {
        if (vdp_job_take()) {
            render_line(vdp_job_line);
            __dmb();
            vdp_job_done = true;
        }
        video_service();
        ym_core1_service();
    }
}

/* ---- emulator ------------------------------------------------------------ */

static void genesis_init(void)
{
    memset(M68K_RAM, 0, MAX_RAM_SIZE);
    memset(ZRAM, 0, MAX_Z80_RAM_SIZE);

    z80_set_memory(ZRAM);
    z80_start();
    z80_pulse_reset();

    m68k_init();
    m68k_pulse_reset();

    /* With sound off the FM synthesis is skipped, but the chip state
     * (timers, status) still has to be correct. */
    YM2612Init();
    YM2612ResetChip();
    YM2612Config(YM2612_DISCRETE);
    gwenesis_SN76489_Init(3579545, 888 * 60, AUDIO_FREQ_DIVISOR, PSG_INTEGRATED);
    gwenesis_SN76489_Reset();

    hdmi_line_lut_reset();
    for (int i = 0; i < 256; i++)
        pbc_palette[i] = PBC_BLACK;
    gwenesis_vdp_reset();
    gwenesis_vdp_set_buffer(&video_screen[0][0]);
    memset(video_screen, 0, sizeof video_screen);
}

static void __attribute__((noreturn)) emulation_loop(void)
{
    int last_w = 0, last_h = 0;
    uint64_t next_frame = time_us_64();
    uint32_t skipped_in_row = 0;

    /* FPS statistics */
    uint64_t stat_t0 = time_us_64();
    uint32_t stat_drawn = 0, stat_emulated = 0;
    uint64_t stat_busy_us = 0;          /* core 0 time not spent sleeping */
    uint32_t stat_z80_us = 0;           /* ... of that in the Z80 */
    uint32_t stat_vdp_us = 0;           /* ... waiting for or drawing lines */
    uint32_t stat_ym_us0 = ym_core1_busy_us;

    screen_width = 320;
    screen_height = 224;
    gwenesis_vdp_set_buffer(&video_screen[0][0]);
    gwenesis_vdp_render_config();

    for (;;) {
        /* ---- input ---- */
        genesis_input_t in = input_poll_genesis();
        button_state[0] = in.pad;
        button_state[1] = 0xFF;
        button_state[2] = 0xFF;
        uint64_t frame_start = time_us_64();

        /* ---- frame geometry ---- */
        int hint_counter = gwenesis_vdp_regs[10];
        bool is_pal = REG1_PAL;
        screen_width = REG12_MODE_H40 ? 320 : 256;
        screen_height = is_pal ? 240 : 224;
        lines_per_frame = is_pal ? LINES_PER_FRAME_PAL : LINES_PER_FRAME_NTSC;
        uint32_t period = is_pal ? 20000u : 16667u;

        bool force_render = false;
        if (screen_width != last_w || screen_height != last_h) {
            gwenesis_vdp_render_config();
            last_w = screen_width;
            last_h = screen_height;
            force_render = true;
        }

        /* ---- skip drawing when more than a frame behind ---- */
        uint64_t now = time_us_64();
        bool render = force_render || skipped_in_row >= MAX_SKIP || now <= next_frame + period;
        if (render) {
            video_frame_begin(screen_width, screen_height);
            skipped_in_row = 0;
        } else {
            skipped_in_row++;
        }

        system_clock = 0;
        scan_line = 0;
        vdp_palette_frame_begin();

        extern volatile int zclk;
        zclk = 0;
        if (!audio_enabled)
            sn76489_clock = sn76489_index = 0;  /* else core 1's */
        ym2612_clock = ym2612_index = 0;

        while (scan_line < (int)lines_per_frame) {
            m68k_run(system_clock + VDP_CYCLES_PER_LINE);

            /* Z80 in slices; always on the VBlank IRQ edges so the sound
             * driver sees its interrupt (games wait on it). */
            bool z80_due =
                ((scan_line % Z80_SLICE_LINES) == (Z80_SLICE_LINES - 1)) ||
                (scan_line == (int)lines_per_frame - 1) ||
                (scan_line == screen_height - 1) ||
                (scan_line == screen_height);
            if (z80_due) {
                uint32_t t = timer_hw->timerawl;
                sound_z80_run(system_clock + VDP_CYCLES_PER_LINE);
                stat_z80_us += timer_hw->timerawl - t;
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

            if (render && scan_line < screen_height) {
                /* Palette in force for this line, before the next line's
                 * emulation can split it. */
                hdmi_line_lut[scan_line] = vdp_palette_current_lut();
                uint32_t t = timer_hw->timerawl;
                vdp_pipeline_post(scan_line);
                stat_vdp_us += timer_hw->timerawl - t;
            }
            scan_line++;

            if (scan_line == screen_height) {
                if (REG1_VBLANK_INTERRUPT != 0) {
                    gwenesis_vdp_status |= STATUS_VIRQPENDING;
                    m68k_set_irq(6);
                }
                sound_z80_irq(1);
            }
            if (scan_line == screen_height + 1)
                sound_z80_irq(0);

            system_clock += VDP_CYCLES_PER_LINE;
        }
        if (audio_enabled) {
            ym2612_run(system_clock);           /* core 0: timers */
            ym_core1_frame_end(system_clock, is_pal);
        } else {
            sound_frame_end(system_clock);
        }

        if (render) {
            uint32_t t = timer_hw->timerawl;
            vdp_pipeline_sync();            /* retire the last line in flight */
            stat_vdp_us += timer_hw->timerawl - t;
        }

        frame_counter++;
        m68k.cycles -= system_clock;

        /* ---- pacing: never faster than the real console ---- */
        next_frame += period;
        now = time_us_64();
        stat_busy_us += now - frame_start;
        if (now < next_frame) {
            sleep_until(from_us_since_boot(next_frame));   /* WFE: saves battery */
        } else if (now > next_frame + 4 * period) {
            next_frame = now;               /* hopelessly behind: stop catching up */
        }

        stat_emulated++;
        if (render)
            stat_drawn++;
        if (show_stats && (now = time_us_64()) - stat_t0 >= 1000000) {
            char s[48];
            uint32_t full = is_pal ? 50 : 60;
            uint64_t span = now - stat_t0;
            /* CPU: share of real time core 0 was working rather than
             * waiting for the next frame. Near 100 % means no headroom.
             * Z: of that in the Z80, V: waiting for core 1 or drawing a
             * line itself; the rest is the 68000. S: core 1's time spent
             * on the sound. */
            uint32_t ym_us = ym_core1_busy_us;
            snprintf(s, sizeof s, "%lufps %lu%% CPU%lu%% Z%lu V%lu S%lu",
                     (unsigned long)stat_drawn,
                     (unsigned long)(stat_emulated * 100 / full),
                     (unsigned long)(stat_busy_us * 100 / span),
                     (unsigned long)(stat_z80_us * 100ull / span),
                     (unsigned long)(stat_vdp_us * 100ull / span),
                     (unsigned long)((ym_us - stat_ym_us0) * 100ull / span));
            stat_ym_us0 = ym_us;
            video_set_status(s);
            stat_t0 = now;
            stat_drawn = stat_emulated = 0;
            stat_busy_us = 0;
            stat_z80_us = stat_vdp_us = 0;
        }
    }
}

/* ---- crash report ------------------------------------------------------- */

/* A fault would otherwise end in a lockup and a silent restart. Show where
 * it happened instead (PC/LR from the exception frame, fault status), so
 * the address can be looked up in the .elf. */
void __attribute__((used, noreturn)) hardfault_report(const uint32_t *frame)
{
    uint core = get_core_num();
    if (core == 0)
        multicore_reset_core1();
    dma_hw->abort = (1u << NUM_DMA_CHANNELS) - 1;
    while (dma_hw->abort)
        tight_loop_contents();
    char l[4][32];
    snprintf(l[0], sizeof l[0], "CRASH on core %u", core);
    snprintf(l[1], sizeof l[1], "PC %08lx", (unsigned long)frame[6]);
    snprintf(l[2], sizeof l[2], "LR %08lx", (unsigned long)frame[5]);
    snprintf(l[3], sizeof l[3], "CFSR %08lx", (unsigned long)scb_hw->cfsr);
    pbc_display_fill(0, 0, PBC_LCD_W, PBC_LCD_H, PBC_BLACK);
    for (int i = 0; i < 4; i++)
        pbc_display_text(16, 80 + i * 24, l[i], i ? PBC_WHITE : PBC_RED, PBC_BLACK, 2);
    for (;;)
        tight_loop_contents();
}

void __attribute__((naked)) isr_hardfault(void)
{
    __asm volatile(
        "tst lr, #4      \n"
        "ite eq          \n"
        "mrseq r0, msp   \n"
        "mrsne r0, psp   \n"
        "b hardfault_report \n");
}

/* ---- main ---------------------------------------------------------------- */

/* Restart into the ROM list. Waits for every button to be released first:
 * A or B still held at boot would land in USB stick mode instead. */
static void __attribute__((noreturn)) reboot_to_menu(void)
{
    menu_wait_release();
    watchdog_reboot(0, 0, 10);
    for (;;) tight_loop_contents();
}

static uint8_t *scratch(void)
{
    /* Loading happens before the emulator runs, so the framebuffer is free
     * to serve as the 32 KB copy buffer. */
    return &video_screen[0][0];
}

int main(void)
{
    input_init();
    pbc_display_init();
    pbc_display_backlight(200);

#if PBC_MULTI
    /* Combined firmware: the launcher has picked the game and owns the
     * USB drive mode and the game list. */
    static char rom[ROM_NAME_MAX];
    const pbc_launch_t *launch = pbc_launch_get();
    if (!launch || !fs_mount())
        reboot_to_menu();
    strncpy(rom, launch->game, sizeof rom - 1);
#else
    if (input_raw() & (IN_A | IN_B))
        usb_stick_mode();

    if (!fs_mount()) {
        /* Fresh device: nothing on the stick yet. */
        usb_stick_mode();
    }

    static char rom[ROM_NAME_MAX];
    if (!menu_pick_rom(rom, sizeof rom)) {
        fs_unmount();
        usb_stick_mode();
    }
#endif

    if (!rom_store_matches(rom)) {
        menu_progress_begin(rom);
        const char *err = rom_store_program(rom, scratch(), menu_progress);
        if (err) {
            menu_message("ERROR", err, rom, "Press a button");
            menu_wait_release();
            while (!input_raw())
                sleep_ms(20);
            reboot_to_menu();
        }
    }
    fs_unmount();

    int volume;
#if PBC_MULTI
    show_stats = pbc_launch_stats();
    video_set_mode((int)pbc_launch_video());
    volume = launch->sound ? launch->volume : 0;
#else
    show_stats = SHOW_FPS;
    video_set_mode(VIDEO_FIT);
    volume = AUDIO_VOLUME;          /* no options menu in this firmware */
#endif
    audio_enabled = sn76489_enabled = volume > 0;

    ROM_DATA = (unsigned char *)rom_store_data();
    ssf2_init();                    /* Super Street Fighter II: bank switching */

    static const menu_control_t controls[] = {
        { "D-pad",   "Joystick" },
        { "A",       "Button A" },
        { "B",       "Button B" },
        { "C",       "Center" },
        { "START",   "Center + B" },
    };
#if PBC_MULTI
    (void)controls;                 /* combined firmware: the launcher showed them */
#else
    menu_controls(controls, sizeof controls / sizeof controls[0]);
#endif

    pbc_display_fill(0, 0, PBC_LCD_W, PBC_LCD_H, PBC_BLACK);
    menu_wait_release();

    overclock();
    audio_pwm_init();               /* after the clock change: PWM carrier */
    audio_pwm_set_volume(volume);
    genesis_init();
    if (audio_enabled)
        ym_core1_init();
    multicore_launch_core1(core1_main);

    emulation_loop();
}
