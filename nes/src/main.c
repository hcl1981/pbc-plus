/*
 * PBC NES -- NES emulator for the PicoBoy Color Plus
 * (RP2350A, 520 KB SRAM, 16 MB flash, ST7789 240x280).
 *
 * Emulator core: QuickNES (Shay Green, LGPL-2.1+) as forked in frank-nes
 * by rh1tech (GPL-3.0). The platform layer is the one from megadrive:
 *   - ROMs come over USB mass storage into a flash FatFs partition
 *     (from PicoBoyGB), the chosen game is copied into a flash region and
 *     QuickNES reads it from there in place
 *   - 240x240 1:1 on the ST7789, sent by core 1 (video.c)
 *   - battery-backed cartridge RAM is kept as a .sav file next to the ROM,
 *     written while the game runs (as in gbc/)
 *   - sound on the PWM speaker (audio_pwm.c); on/off and volume only in
 *     the options of the ROM list (saved), never in a game, so nothing can
 *     be changed by accident
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
#if PBC_MULTI
#include "pbc_loading.h"
#endif
#include "rom_store.h"
#include "video.h"
#include "audio_pwm.h"
#include "settings.h"
#include "quicknes.h"
#include "pbc_link.h"

/* ---- build-time configuration (see CMakeLists.txt) ---------------------- */
#ifndef CPU_MHZ
#define CPU_MHZ 150
#endif
#ifndef CPU_VREG
#define CPU_VREG VREG_VOLTAGE_1_10
#endif
#ifndef FLASH_MAX_FREQ_MHZ
#define FLASH_MAX_FREQ_MHZ 88
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
    /* on the heap: needed once, and the RAM is the emulator's afterwards */
    BYTE *work = malloc(FF_MAX_SS);
    if (!work)
        return false;
    /* FM_SFD: no partition table, one FAT volume filling the region. */
    MKFS_PARM opt = { FM_FAT | FM_SFD, 0, 0, 0, 0 };
    FRESULT r = f_mkfs("", &opt, work, FF_MAX_SS);
    free(work);
    if (r != FR_OK)
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
    if (f_stat("/NES.TXT", &fi) == FR_OK)
        return;
    if (f_open(&f, "/NES.TXT", FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
        return;
    f_puts("PBC NES - NES emulator for the PicoBoy Color Plus\r\n"
           "\r\n"
           "Copy NES ROMs (.nes) to this drive, then EJECT it. The PicoBoy\r\n"
           "restarts by itself and shows the game list.\r\n"
           "\r\n"
           "Back to this mode: \"Add games via USB\" at the top of the game list,\r\n"
           "or hold A or B while switching on.\r\n"
           "\r\n"
           "Saved games of cartridges with a battery are stored as <game>.sav\r\n"
           "next to the ROM. Wait a moment after saving before you press RESET.\r\n"
           "\r\n"
           "Game Genie codes: put them in <game>.cht next to the ROM, one code\r\n"
           "(6 or 8 letters) per line; text after the code is a comment.\r\n"
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

/* megadrive and nes share one drive. Whichever formatted it first used to
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
                 "Copy NES ROMs to the",
                 "drive, then eject it.",
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
    /* clk_peri follows clk_sys: re-derive the SPI divider. */
    spi_set_baudrate(PBC_SPI, PBC_SPI_HZ);
}

/* ---- battery saves --------------------------------------------------------
 *
 * The RAM at $6000 of a cartridge with a battery goes to "<rom>.sav" on the
 * FatFs drive. A game is left with RESET, so it is written while the game
 * runs: a checksum of the RAM four times a second; once it has not changed
 * for a second (or has been changing for 20 s), the file is written.
 * Unchanged flash sectors are skipped and core 1 is parked for each written
 * one (flash_fs.c) -- the game stalls for about 50 ms per changed 4 KB. */
static uint8_t *save_ram;
static long save_size;
static char save_path[ROM_NAME_MAX + 8];
static uint32_t save_sum;
static bool save_dirty;
static uint64_t save_first_change, save_last_change;

static uint32_t save_checksum(void)
{
    uint32_t h = 2166136261u;
    for (long i = 0; i < save_size; i++)
        h = (h ^ save_ram[i]) * 16777619u;
    return h;
}

static void save_load(const char *game)
{
    static FIL f;
    UINT got;
    /* link play: both devices must run the same RAM, so no saves at all */
    save_ram = pbc_link_active() ? NULL : qnes_get_battery_ram(&save_size);
    if (!save_ram)
        return;
    snprintf(save_path, sizeof save_path, "/%s", game);
    char *dot = strrchr(save_path, '.');
    if (dot) *dot = 0;
    strcat(save_path, ".sav");
    if (f_open(&f, save_path, FA_READ) == FR_OK) {
        if (f_size(&f) >= (FSIZE_t)save_size)
            f_read(&f, save_ram, (UINT)save_size, &got);
        f_close(&f);
    }
    save_sum = save_checksum();
}

static void save_write(void)
{
    static FIL f;
    UINT put = 0;
    save_dirty = false;
    if (f_open(&f, save_path, FA_WRITE | FA_OPEN_ALWAYS) == FR_OK) {
        f_write(&f, save_ram, (UINT)save_size, &put);
        f_truncate(&f);
        f_close(&f);
    }
    if (put != (UINT)save_size) {
        /* drive full (or broken): say so once, keep playing */
        f_unlink(save_path);
        save_ram = NULL;
        video_set_status("NO ROOM FOR THE SAVE FILE");
    }
}

static void save_poll(void)
{
    static unsigned frames;
    if (!save_ram || ++frames % 15)
        return;
    uint64_t now = time_us_64();
    uint32_t sum = save_checksum();
    if (sum != save_sum) {
        save_sum = sum;
        save_last_change = now;
        if (!save_dirty) {
            save_dirty = true;
            save_first_change = now;
        }
    }
    if (save_dirty && (now - save_last_change >= 1000000 || now - save_first_change >= 20000000))
        save_write();
}

/* ---- Game Genie ----------------------------------------------------------
 *
 * "<rom>.cht" next to the ROM: one code per line (6 or 8 letters), the rest
 * of a line after a space or '#' is a comment. Not in link play: both
 * devices must run the same game. */

static uint64_t cheat_note_until;   /* "n CHEATS" on screen until then */

static void cheats_load(const char *game)
{
    static FIL f;
    static char text[1024];
    char path[ROM_NAME_MAX + 8];
    UINT got = 0;
    if (pbc_link_active())
        return;
    snprintf(path, sizeof path, "/%s", game);
    char *dot = strrchr(path, '.');
    if (dot) *dot = 0;
    strcat(path, ".cht");
    if (f_open(&f, path, FA_READ) != FR_OK)
        return;
    f_read(&f, text, sizeof text - 1, &got);
    f_close(&f);
    text[got] = 0;

    const char *codes[16];
    int n = 0;
    for (char *line = text; *line && n < 16;) {
        char *end = line + strcspn(line, "\r\n");
        char *word = line + strspn(line, " \t");
        size_t len = strcspn(word, " \t#\r\n");
        bool next = *end != 0;
        if ((len == 6 || len == 8) && word + len <= end) {
            word[len] = 0;
            codes[n++] = word;
        }
        line = next ? end + 1 : end;
    }
    if (n == 0)
        return;
    int on = qnes_set_game_genie(codes, n);
    char s[24];
    if (on > 0)
        snprintf(s, sizeof s, on == 1 ? "1 CHEAT" : "%d CHEATS", on);
    else
        strcpy(s, "NO USABLE CHEATS");  /* malformed, or no RAM for the pages */
    video_set_status(s);
    cheat_note_until = time_us_64() + 3000000;
}

/* ---- emulator ------------------------------------------------------------ */

/* fps, speed and CPU load: from the launcher in the combined firmware,
 * else the build option. */
static bool show_stats;

static void __attribute__((noreturn)) emulation_loop(void)
{
    const uint32_t period = 16639;          /* NTSC: 60.0988 Hz */
    uint64_t next_frame = time_us_64();
    const uint8_t *prev_pixels = NULL, *last_pixels = NULL;

    audio_pwm_set_volume(g_settings.sound ? g_settings.volume : 0);

    uint64_t stat_t0 = time_us_64(), stat_busy_us = 0;
    uint32_t stat_drawn = 0, stat_emulated = 0;

    for (;;) {
        /* QuickNES draws this frame into the buffer of two frames ago.
         * If core 1 is still sending that one, let it finish. */
        while (prev_pixels && video_busy_with(prev_pixels))
            tight_loop_contents();

        /* as late as possible: the partner waits for this */
        uint16_t joy1, joy2;
        pbc_link_frame(input_poll_nes(), qnes_get_ram(), 0x800, &joy1, &joy2);

        uint64_t frame_start = time_us_64();
        qnes_emulate_frame(joy1, joy2);

        static int16_t samples[1024];
        long n = qnes_read_samples(samples, 1024);   /* must be drained anyway */
        audio_pwm_push(samples, (int)n);

        int pal_size = 0;
        const int16_t *pal = qnes_get_palette(&pal_size);
        const uint8_t *px = qnes_get_pixels();
        prev_pixels = last_pixels;
        last_pixels = px;
        if (px && pal && video_present(px, 256 + 16, pal, pal_size))
            stat_drawn++;
        stat_emulated++;
        save_poll();

        next_frame += period;
        uint64_t now = time_us_64();
        stat_busy_us += now - frame_start;
        if (pbc_link_follows())
            next_frame = now;               /* the partner's frames are the clock */
        else if (now < next_frame)
            sleep_until(from_us_since_boot(next_frame));
        else if (now > next_frame + 4 * period)
            next_frame = now;               /* hopelessly behind: stop catching up */

        if (cheat_note_until && time_us_64() > cheat_note_until) {
            cheat_note_until = 0;
            if (!show_stats)
                video_set_status("");
        }
        if (pbc_link_out_of_sync_news())
            video_set_status("LINK OUT OF SYNC");
        else if (show_stats && !pbc_link_out_of_sync()) {
            now = time_us_64();
            uint64_t span = now - stat_t0;
            if (span >= 1000000) {
                /* speed: emulated frames against the 60.0988 Hz of the NES */
                char s[32];
                snprintf(s, sizeof s, "%lufps %lu%% CPU%lu%%",
                         (unsigned long)(stat_drawn * 1000000ull / span),
                         (unsigned long)(stat_emulated * (uint64_t)period * 100 / span),
                         (unsigned long)(stat_busy_us * 100 / span));
                video_set_status(s);
                stat_t0 = now;
                stat_drawn = stat_emulated = 0;
                stat_busy_us = 0;
            }
        }
    }
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
#if !PBC_MULTI
    pbc_display_backlight(200);     /* combined firmware: pbc_fade_in() */
#endif

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

    if (!fs_mount())
        usb_stick_mode();           /* fresh device: nothing on the stick yet */

    static char rom[ROM_NAME_MAX];
    if (!menu_pick_rom(rom, sizeof rom)) {
        fs_unmount();
        usb_stick_mode();
    }
#endif

    if (!rom_store_matches(rom)) {
        /* Heap, freed again before the emulator allocates its tile cache. */
        uint8_t *scratch = malloc(16384);
        if (!scratch)
            fatal("Out of memory", "for loading");
        menu_progress_begin(rom);
        const char *err = rom_store_program(rom, scratch, menu_progress);
        free(scratch);
        if (err)
            fatal(err, rom);
    }
    /* The volume stays mounted: the .sav file is written while the game runs. */

    static const menu_control_t controls[] = {
        { "D-pad",   "Joystick" },
        { "A",       "Button B" },
        { "B",       "Button A" },
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

    qnes_set_region(QNES_REGION_NTSC);
    audio_pwm_init();
    if (qnes_init(AUDIO_RATE) != 0)
        fatal("Emulator start", "failed");
    if (qnes_load_rom_inplace(rom_store_data(), (long)rom_store_size()) != 0)
        fatal("Cannot load game:", "mapper or memory");

#if PBC_MULTI
    show_stats = pbc_launch_stats();
    video_set_mode(pbc_launch_video());
#else
    show_stats = SHOW_FPS;
    video_set_mode(VIDEO_MODE_CROP);
#endif
    pbc_link_start(pbc_link_hash(PBC_LINK_HASH_START ^ 1, rom_store_data(), rom_store_size()));
    cheats_load(rom);               /* after the link start: not in link play */
    save_load(rom);
    pbc_display_fill(0, 0, PBC_LCD_W, PBC_LCD_H, PBC_BLACK);
    multicore_launch_core1(video_core1_main);
#if PBC_MULTI
    pbc_fade_in(false);             /* over the first frames */
#endif
    emulation_loop();
}
