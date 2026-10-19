/*
 * PBC Game Boy Color -- Game Boy and Game Boy Color emulator for the
 * PicoBoy Color Plus (RP2350A, 520 KB SRAM, 16 MB flash, ST7789 240x280).
 *
 * Emulator core: Peanut-GB (Mahyar Koshkouei, MIT) with the Game Boy Color
 * support of tvecera's gbc-rtc-fix branch, as found in PicoPlus-devel's
 * pico-peanutGB; sound: minigb_apu as used in ../picoboygb. The platform
 * layer is the one from nes and c64:
 *   - ROMs come over USB mass storage into a flash FatFs partition
 *     (from PicoBoyGB), the chosen game is copied into a flash region and
 *     read from there in place (bank 0 mirrored in RAM)
 *   - 160x144 scaled 1.5x to 240x216 on the ST7789, sent by core 1
 *   - sound on the PWM speaker, its sample timer on core 1; on/off and
 *     volume only in the options of the ROM list
 *   - battery-backed cartridge RAM is kept as a .sav file next to the ROM,
 *     written while the game runs once it has stopped changing
 *
 * Boot:
 *   A or B held  -> USB stick mode, eject restarts
 *   otherwise    -> ROM list, then the game (RESET leaves it)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#include "hardware/pll.h"
#include "hardware/vreg.h"
#include "hardware/spi.h"
#include "hardware/watchdog.h"
#include "hardware/structs/qmi.h"

#include "ff.h"
#include "tusb.h"

#include "board.h"
#include "pbc_launch.h"
#include "color565.h"
#include "display.h"
#include "input.h"
#include "menu.h"
#include "rom_store.h"
#include "video.h"
#include "audio_pwm.h"
#include "settings.h"

/* Peanut-GB: single-header core, implemented in this file. */
#define ENABLE_SOUND 1
#define ENABLE_LCD   1
#define RGB555_TO_RGB444(c) (c)     /* the core keeps a 444 copy for HDMI; unused */
#include "minigb_apu.h"
#include "peanut_gb.h"

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
    if (f_stat("/GBC.TXT", &fi) == FR_OK)
        return;
    if (f_open(&f, "/GBC.TXT", FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
        return;
    f_puts("PBC Game Boy Color - Game Boy and Game Boy Color emulator\r\n"
           "for the PicoBoy Color Plus\r\n"
           "\r\n"
           "Copy ROMs (.gb .gbc) to this drive, then EJECT it. The PicoBoy\r\n"
           "restarts by itself and shows the game list.\r\n"
           "\r\n"
           "Back to this mode: \"Add games via USB\" at the top of the game list,\r\n"
           "or hold A or B while switching on.\r\n"
           "\r\n"
           "Saved games of cartridges with a battery are stored as <game>.sav\r\n"
           "next to the ROM. Wait a moment after saving before you press RESET.\r\n"
           "\r\n"
           "Controls in game:\r\n"
           "  Joystick       D-pad\r\n"
           "  A / B          A / B\r\n"
           "  Center + B     START\r\n"
           "  Center + A     SELECT\r\n"
           "\r\n"
           "Back to the game list: RESET.\r\n", &f);
    f_close(&f);
}

/* The PBC firmwares share one drive. Whichever formatted it first used to
 * name it after itself; give it the common name in either case. */
static void fs_ensure_label(void)
{
    char label[24];
    if (f_getlabel("", label, NULL) == FR_OK && strcmp(label, "PBC-ROMS") != 0)
        f_setlabel("PBC-ROMS");
}

/* One diagnostic line per step: the panel is the only console. */
static int diag_y;

static void diag(const char *s, uint16_t colour)
{
    pbc_display_text(8, diag_y, s, colour, PBC_BLACK, 1);
    diag_y += 11;
}

static void __attribute__((noreturn)) usb_stick_mode(void)
{
    menu_message("USB DRIVE MODE",
                 "Copy Game Boy ROMs to",
                 "the drive, then eject.",
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
    fs_ensure_label();
    fs_write_readme();
    fs_unmount();               /* the host owns the medium from now on */

    tud_init(BOARD_TUD_RHPORT);
    diag("USB started", PBC_GREY);

    int status_y = diag_y;
    uint64_t next_draw = 0;
    for (;;) {
        tud_task();
        if (time_us_64() >= next_draw) {
            next_draw = time_us_64() + 500000;
            int state = tud_mounted() ? 2 : tud_connected() ? 1 : 0;
            static const char *const names[] = {
                "PC: not connected  ", "PC: connected      ", "PC: drive mounted  " };
            pbc_display_text(8, status_y, names[state],
                             state == 2 ? PBC_YELLOW : PBC_WHITE, PBC_BLACK, 1);
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

/* ---- clocks (as in megadrive) -------------------------------------------- */

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

/* The SPI clock is clk_peri divided by an even number, and clk_peri
 * follows clk_sys: at 200 MHz the display got only 50 MHz (16.6 ms per
 * frame, barely inside 16.7 ms). USB is used only before the game (stick
 * mode), so in the game the USB PLL runs at 150 MHz and drives clk_peri
 * alone: 75 MHz SPI whatever the system clock. clk_usb and clk_adc, which
 * hang off that PLL, are stopped first. */
static void display_clock(void)
{
    clock_stop(clk_usb);
    clock_stop(clk_adc);
    pll_init(pll_usb, 1, 1500 * MHZ, 5, 2);          /* 12 MHz x 125 / 10 */
    clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
                    150 * MHZ, 150 * MHZ);
    spi_set_baudrate(PBC_SPI, PBC_SPI_HZ);
}

/* Raised only after all flash writing is done. */
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
    }
    display_clock();
}

/* ---- emulator ------------------------------------------------------------ */

#define FRAME_US   16743u              /* 4194304 Hz / 70224 cycles = 59.73 Hz */
#define CART_RAM_MAX 0x20000           /* MBC5: up to 128 KB */

static struct gb_s gb;
static const uint8_t *rom;             /* in XIP flash */
static uint32_t rom_len;
static uint8_t bank0[0x4000];          /* bank 0 is read constantly: keep it in RAM */
static uint8_t cart_ram[CART_RAM_MAX];
static uint32_t save_size;
static volatile bool save_dirty;
static uint64_t save_first_change, save_last_change;

uint8_t __not_in_flash_func(gb_rom_read)(struct gb_s *g, const uint_fast32_t addr)
{
    (void)g;
    if (addr < sizeof bank0) return bank0[addr];
    return addr < rom_len ? rom[addr] : 0xFF;
}

uint8_t __not_in_flash_func(gb_cart_ram_read)(struct gb_s *g, const uint_fast32_t addr)
{
    (void)g;
    return addr < CART_RAM_MAX ? cart_ram[addr] : 0xFF;
}

void __not_in_flash_func(gb_cart_ram_write)(struct gb_s *g, const uint_fast32_t addr, const uint8_t v)
{
    (void)g;
    if (addr >= CART_RAM_MAX || cart_ram[addr] == v)
        return;
    cart_ram[addr] = v;
    if (!save_dirty) {
        save_dirty = true;
        save_first_change = time_us_64();
    }
    save_last_change = time_us_64();
}

void gb_error(struct gb_s *g, const enum gb_error_e e, const uint16_t addr)
{
    /* Many games make harmless invalid accesses; keep running. */
    (void)g; (void)e; (void)addr;
}

/* DMG games: one fixed, easy-to-read green palette for background and both
 * sprite palettes (peanut pixel: bits 4-5 = palette, bits 0-1 = shade). */
static const uint32_t dmg_shades[4] = { 0xE0F8D0, 0x88C070, 0x346856, 0x081820 };
static uint16_t dmg_lut[64];

/* CGB colour as fixed by the core (bits 10-14 red, 5-9 green, 0-4 blue)
 * -> panel word. */
static inline uint16_t cgb_to_panel(uint16_t c)
{
    unsigned r = (c >> 10) & 31, g = (c >> 5) & 31, b = c & 31;
    return PBC_RGB(r << 3 | r >> 2, g << 3 | g >> 2, b << 3 | b >> 2);
}

void __not_in_flash_func(lcd_draw_line)(struct gb_s *g, const uint8_t *pixels, const uint_fast8_t line)
{
    uint16_t *row = video_back() + line * GB_W;
    if (g->cgb.cgbMode) {
        /* Palettes can change between lines; convert only the entries that
         * changed since the last line (usually none). */
        static uint16_t seen[64], lut[64];
        static bool valid;
        for (int i = 0; i < 64; i++) {
            uint16_t c = g->cgb.fixPalette[i];
            if (c != seen[i] || !valid) {
                seen[i] = c;
                lut[i] = cgb_to_panel(c);
            }
        }
        valid = true;
        for (int x = 0; x < GB_W; x++)
            row[x] = lut[pixels[x] & 63];
    } else {
        for (int x = 0; x < GB_W; x++)
            row[x] = dmg_lut[pixels[x] & 0x33];
    }
}

/* ---- battery saves ---------------------------------------------------------
 *
 * Cartridge RAM lives in RAM and goes to "<rom>.sav" on the FatFs drive. A
 * game is left with RESET, so it is written while the game runs: once the RAM
 * has not changed for a second (or has been changing for 20 s). Unchanged
 * flash sectors are skipped (flash_fs.c), core 1 is parked for each written
 * one -- the game stalls for about 50 ms per 4 KB sector that did change. */
static char save_path[ROM_NAME_MAX + 8];

static void save_path_for(const char *game)
{
    snprintf(save_path, sizeof save_path, "/%s", game);
    char *dot = strrchr(save_path, '.');
    if (dot) *dot = 0;
    strcat(save_path, ".sav");
}

static void save_load(void)
{
    static FIL f;
    UINT got;
    memset(cart_ram, 0xFF, sizeof cart_ram);
    if (!save_size || f_open(&f, save_path, FA_READ) != FR_OK)
        return;
    if (f_size(&f) >= save_size)
        f_read(&f, cart_ram, save_size, &got);
    f_close(&f);
}

static void save_write(void)
{
    static FIL f;
    UINT put;
    save_dirty = false;
    if (f_open(&f, save_path, FA_WRITE | FA_OPEN_ALWAYS) != FR_OK)
        return;
    f_write(&f, cart_ram, save_size, &put);
    f_truncate(&f);
    f_close(&f);
}

static void save_poll(void)
{
    if (!save_dirty || !save_size)
        return;
    uint64_t now = time_us_64();
    if (now - save_last_change >= 1000000 || now - save_first_change >= 20000000)
        save_write();
}

static void __attribute__((noreturn)) emulation_loop(void)
{
    uint64_t next_frame = time_us_64();
    /* minigb_apu renders AUDIO_SAMPLES (= 372 at 22222 Hz) stereo frames per
     * Game Boy frame; it is a floating-point expression, hence the margin. */
    static int16_t stereo[2 * 400];
    static int16_t mono[400];
    const unsigned n = AUDIO_SAMPLES;

    audio_pwm_set_volume(g_settings.sound ? g_settings.volume : 0);

    /* fps, speed and load: compile-time default, in the combined firmware
     * the launcher's option */
#if PBC_MULTI
    const bool show_stats = pbc_launch_stats();
#else
    const bool show_stats = SHOW_FPS;
#endif
    uint64_t stat_t0 = time_us_64(), stat_busy_us = 0;
    uint32_t stat_drawn = 0, stat_emulated = 0;

    for (;;) {
        uint8_t joy = input_poll_gb();
        gb.direct.joypad = (uint8_t)~joy;       /* active low */

        uint64_t frame_start = time_us_64();
        gb_run_frame(&gb);

        audio_callback(NULL, stereo, n * 2 * sizeof stereo[0]);
        for (unsigned i = 0; i < n; i++)
            mono[i] = (int16_t)(((int32_t)stereo[2 * i] + stereo[2 * i + 1]) / 2);
        audio_pwm_push(mono, (int)n);

        if (video_present())
            stat_drawn++;
        stat_emulated++;
        save_poll();

        next_frame += FRAME_US;
        uint64_t now = time_us_64();
        stat_busy_us += now - frame_start;
        if (now < next_frame)
            sleep_until(from_us_since_boot(next_frame));
        else if (now > next_frame + 4 * FRAME_US)
            next_frame = now;               /* hopelessly behind: stop catching up */

        if (!show_stats)
            continue;
        now = time_us_64();
        if (now - stat_t0 >= 1000000) {
            char s[32];
            snprintf(s, sizeof s, "%lufps %lu%% CPU%lu%%",
                     (unsigned long)stat_drawn,
                     (unsigned long)(stat_emulated * 100 / 60),
                     (unsigned long)(stat_busy_us * 100 / (now - stat_t0)));
            video_set_status(s);
            stat_t0 = now;
            stat_drawn = stat_emulated = 0;
            stat_busy_us = 0;
        }
    }
}

/* ---- crash screen (as in c64) ---------------------------------------------
 *
 * There is no serial console on this board, so a fault is shown on the panel:
 * the faulting PC and LR go into arm-none-eabi-addr2line -e build/pbc_gbc.elf.
 * A stack-guard hit (MPU) arrives here too. */
void __attribute__((used, noreturn)) hardfault_report(uint32_t *frame, uint32_t exc_return)
{
    (void)exc_return;
    char l1[32], l2[32], l3[32];
    snprintf(l1, sizeof l1, "Core %u  PC %08lx", get_core_num(), (unsigned long)frame[6]);
    snprintf(l2, sizeof l2, "LR %08lx", (unsigned long)frame[5]);
    snprintf(l3, sizeof l3, "CFSR %08lx", (unsigned long)(*(volatile uint32_t *)0xE000ED28));
    if (get_core_num() == 0)
        multicore_reset_core1();        /* the panel is ours now */
    pbc_display_end();
    menu_message("CRASH", l1, l2, l3);
    for (;;) tight_loop_contents();
}

void __attribute__((naked)) isr_hardfault(void)
{
    __asm volatile(
        "tst lr, #4      \n"
        "ite eq          \n"
        "mrseq r0, msp   \n"
        "mrsne r0, psp   \n"
        "mov r1, lr      \n"
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

static void __attribute__((noreturn)) fatal(const char *what, const char *detail)
{
    menu_message("ERROR", what, detail, "Press a button");
    menu_wait_release();
    while (!input_raw())
        sleep_ms(20);
    reboot_to_menu();
}

int main(void)
{
    input_init();
    settings_load();
    pbc_display_init();
    pbc_display_backlight(200);

#if PBC_MULTI
    /* Combined firmware: the launcher has picked the game and owns the
     * USB drive mode and the game list. */
    static char game[ROM_NAME_MAX];
    const pbc_launch_t *launch = pbc_launch_get();
    if (!launch || !fs_mount())
        reboot_to_menu();
    strncpy(game, launch->game, sizeof game - 1);
#else
    if (input_raw() & (IN_A | IN_B))
        usb_stick_mode();

    if (!fs_mount())
        usb_stick_mode();           /* fresh device: nothing on the stick yet */

    static char game[ROM_NAME_MAX];
    if (!menu_pick_rom(game, sizeof game)) {
        fs_unmount();
        usb_stick_mode();
    }
#endif

    if (!rom_store_matches(game)) {
        uint8_t *scratch = malloc(16384);
        if (!scratch)
            fatal("Out of memory", "for loading");
        menu_progress_begin(game);
        const char *err = rom_store_program(game, scratch, menu_progress);
        free(scratch);
        if (err)
            fatal(err, game);
    }
    /* The volume stays mounted: the .sav file is written while the game runs. */

    rom = rom_store_data();
    rom_len = rom_store_size();
    memcpy(bank0, rom, rom_len < sizeof bank0 ? rom_len : sizeof bank0);

    for (int i = 0; i < 64; i++)
        dmg_lut[i] = pbc_rgb24(dmg_shades[i & 3]);

    audio_init();
    if (gb_init(&gb, gb_rom_read, gb_cart_ram_read, gb_cart_ram_write, gb_error, NULL) != GB_INIT_NO_ERROR)
        fatal("Cannot start game:", "unsupported cartridge");
    gb_init_lcd(&gb, lcd_draw_line);
    gb.direct.frame_skip = false;

    save_size = gb_get_save_size(&gb);
    if (save_size > CART_RAM_MAX)
        save_size = CART_RAM_MAX;
    save_path_for(game);
    save_load();

    static const menu_control_t controls[] = {
        { "D-pad",   "Joystick" },
        { "A",       "Button A" },
        { "B",       "Button B" },
        { "START",   "Center + B" },
        { "SELECT",  "Center + A" },
    };
#if PBC_MULTI
    (void)controls;                 /* combined firmware: the launcher showed them */
#else
    menu_controls(controls, sizeof controls / sizeof controls[0]);
#endif

    pbc_display_fill(0, 0, PBC_LCD_W, PBC_LCD_H, PBC_BLACK);
    menu_wait_release();

    overclock();
    audio_pwm_init();

#if PBC_MULTI
    video_set_mode((int)pbc_launch_video());
#else
    video_set_mode(VIDEO_SCALE);
#endif
    multicore_launch_core1(video_core1_main);
    video_wait_ready();             /* sample timer and lockout live on core 1 */
    emulation_loop();
}
