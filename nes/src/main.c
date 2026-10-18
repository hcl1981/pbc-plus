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
 *   - no sound output: the APU still runs, its samples are discarded
 *
 * Boot:
 *   A or B held  -> USB stick mode, eject restarts
 *   otherwise    -> ROM list, then the game
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
#include "color565.h"
#include "display.h"
#include "input.h"
#include "menu.h"
#include "rom_store.h"
#include "video.h"
#include "quicknes.h"

/* ---- build-time configuration (see CMakeLists.txt) ---------------------- */
#ifndef CPU_MHZ
#define CPU_MHZ 250
#endif
#ifndef CPU_VREG
#define CPU_VREG VREG_VOLTAGE_1_25
#endif
#ifndef FLASH_MAX_FREQ_MHZ
#define FLASH_MAX_FREQ_MHZ 88
#endif
#ifndef SHOW_FPS
#define SHOW_FPS 1
#endif
/* QuickNES always synthesises sound; a low rate keeps that cheap. */
#define SAMPLE_RATE 11025
/* Holding SELECT (CENTER + A) this long leaves the game. */
#define SELECT_HOLD_US 1000000u

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
    if (f_stat("/NES.TXT", &fi) == FR_OK)
        return;
    if (f_open(&f, "/NES.TXT", FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
        return;
    f_puts("PBC NES - NES-Emulator fuer den PicoBoy Color Plus\r\n"
           "\r\n"
           "NES-ROMs (.nes) hierher kopieren, dann das Laufwerk AUSWERFEN.\r\n"
           "Der PicoBoy startet danach von selbst neu und zeigt die Spieleliste.\r\n"
           "\r\n"
           "Zurueck in diesen Modus: beim Einschalten A oder B gedrueckt halten.\r\n"
           "\r\n"
           "Steuerung im Spiel:\r\n"
           "  Joystick       Steuerkreuz\r\n"
           "  A / B          A / B\r\n"
           "  Mitte + B      START\r\n"
           "  Mitte + A      SELECT (1 s halten = zurueck zur Spieleliste)\r\n", &f);
    f_close(&f);
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
    menu_message("USB-STICK-MODUS",
                 "NES-ROMs auf das Laufwerk",
                 "kopieren und dann",
                 "auswerfen. Startet neu.");
    diag_y = 168;

    diag("Laufwerk pruefen...", PBC_GREY);
    FRESULT fr = f_mount(&g_fs, "", 1);
    if (fr != FR_OK) {
        char m[32];
        snprintf(m, sizeof m, "leer (%d), formatiere...", (int)fr);
        diag(m, PBC_YELLOW);
        if (fs_format() && fs_mount())
            fr = FR_OK;
    }
    if (fr != FR_OK) {
        diag("FEHLER: Laufwerk unbrauchbar", PBC_RED);
        for (;;) tight_loop_contents();
    }
    fs_write_readme();
    fs_unmount();               /* the host owns the medium from now on */

    tud_init(BOARD_TUD_RHPORT);
    diag("USB gestartet", PBC_GREY);

    int status_y = diag_y;
    uint64_t next_draw = 0;
    for (;;) {
        tud_task();
        if (time_us_64() >= next_draw) {
            next_draw = time_us_64() + 500000;
            int state = tud_mounted() ? 2 : tud_connected() ? 1 : 0;
            static const char *const names[] = {
                "PC: keine Verbindung", "PC: verbunden       ", "PC: Laufwerk aktiv  " };
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

/* ---- emulator ------------------------------------------------------------ */

static void emulation_loop(void)
{
    const uint32_t period = 16639;          /* NTSC: 60.0988 Hz */
    uint64_t next_frame = time_us_64();
    uint64_t select_since = 0;
    const uint8_t *prev_pixels = NULL, *last_pixels = NULL;

    uint64_t stat_t0 = time_us_64(), stat_busy_us = 0;
    uint32_t stat_drawn = 0, stat_emulated = 0;

    for (;;) {
        uint8_t joy = input_poll_nes();
        if (joy & NES_SELECT) {
            if (select_since == 0)
                select_since = time_us_64();
            else if (time_us_64() - select_since >= SELECT_HOLD_US)
                return;
        } else {
            select_since = 0;
        }

        /* QuickNES draws this frame into the buffer of two frames ago.
         * If core 1 is still sending that one, let it finish. */
        while (prev_pixels && video_busy_with(prev_pixels))
            tight_loop_contents();

        uint64_t frame_start = time_us_64();
        qnes_emulate_frame(joy, 0);

        static int16_t discard[1024];
        qnes_read_samples(discard, 1024);   /* the APU output must be drained */

        int pal_size = 0;
        const int16_t *pal = qnes_get_palette(&pal_size);
        const uint8_t *px = qnes_get_pixels();
        prev_pixels = last_pixels;
        last_pixels = px;
        if (px && pal && video_present(px, 256 + 16, pal, pal_size))
            stat_drawn++;
        stat_emulated++;

        next_frame += period;
        uint64_t now = time_us_64();
        stat_busy_us += now - frame_start;
        if (now < next_frame)
            sleep_until(from_us_since_boot(next_frame));
        else if (now > next_frame + 4 * period)
            next_frame = now;               /* hopelessly behind: stop catching up */

#if SHOW_FPS
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
#endif
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
    menu_message("FEHLER", what, detail, "Taste: zurueck");
    menu_wait_release();
    while (!input_raw())
        sleep_ms(20);
    reboot_to_menu();
}

int main(void)
{
    input_init();
    pbc_display_init();
    pbc_display_backlight(200);

    if (input_raw() & (IN_A | IN_B))
        usb_stick_mode();

    if (!fs_mount())
        usb_stick_mode();           /* fresh device: nothing on the stick yet */

    static char rom[ROM_NAME_MAX];
    if (!menu_pick_rom(rom, sizeof rom)) {
        fs_unmount();
        usb_stick_mode();
    }

    if (!rom_store_matches(rom)) {
        /* Heap, freed again before the emulator allocates its tile cache. */
        uint8_t *scratch = malloc(16384);
        if (!scratch)
            fatal("Kein Speicher", "zum Laden");
        menu_progress_begin(rom);
        const char *err = rom_store_program(rom, scratch, menu_progress);
        free(scratch);
        if (err)
            fatal(err, rom);
    }
    fs_unmount();

    pbc_display_fill(0, 0, PBC_LCD_W, PBC_LCD_H, PBC_BLACK);
    menu_wait_release();

    overclock();

    qnes_set_region(QNES_REGION_NTSC);
    if (qnes_init(SAMPLE_RATE) != 0)
        fatal("Emulator-Start", "fehlgeschlagen");
    if (qnes_load_rom_inplace(rom_store_data(), (long)rom_store_size()) != 0)
        fatal("Spiel nicht ladbar:", "Mapper oder Speicher");

    multicore_launch_core1(video_core1_main);
    emulation_loop();

    /* SELECT held: back to the ROM list. Stop core 1 so the panel is ours
     * again, then reboot. */
    multicore_reset_core1();
    pbc_display_end();
    menu_message("PBC NES", "Zurueck zur Spieleliste...", NULL, "Tasten loslassen");
    reboot_to_menu();
}
