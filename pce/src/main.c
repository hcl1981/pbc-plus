/*
 * PBC PC Engine -- PC Engine / TurboGrafx-16 emulator for the PicoBoy
 * Color Plus (RP2350A, 520 KB SRAM, 16 MB flash, ST7789 240x280).
 *
 * Emulator core: pce-go from retro-go (ducalex, GPL-2.0) in the version of
 * pico-pcePlus (PicoPlus-devel, GPL-3.0). HuCards only: CD-ROM and the
 * SuperGrafx need PSRAM, which this board does not have. The platform layer
 * is the one from nes/megadrive:
 *   - ROMs come over USB mass storage into a flash FatFs partition
 *     (from PicoBoyGB); the chosen card is copied into a flash region and
 *     pce-go maps its banks straight onto XIP flash (rom_store.c)
 *   - the picture is cropped or scaled to 240 wide on the ST7789 and sent
 *     by core 1 (video.c)
 *   - sound on the PWM speaker (audio_pwm.c), its timer on core 1; on/off
 *     and volume only in the options of the ROM list
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
#include "hardware/structs/powman.h"

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
#include "pce_glue.h"
#include "pce-go.h"
#include "pce.h"

/* ---- build-time configuration (see CMakeLists.txt) ---------------------- */
#ifndef CPU_MHZ
#define CPU_MHZ 200
#endif
#ifndef CPU_VREG
#define CPU_VREG VREG_VOLTAGE_1_15
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
    if (f_stat("/PCE.TXT", &fi) == FR_OK)
        return;
    if (f_open(&f, "/PCE.TXT", FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
        return;
    f_puts("PBC PC Engine - PC Engine / TurboGrafx-16 emulator for the PicoBoy Color Plus\r\n"
           "\r\n"
           "Copy HuCard images (.pce) to this drive, then EJECT it. The PicoBoy\r\n"
           "restarts by itself and shows the game list. CD-ROM and SuperGrafx\r\n"
           "games are not supported.\r\n"
           "\r\n"
           "Back to this mode: \"Add games via USB\" at the top of the game list,\r\n"
           "or hold A or B while switching on.\r\n"
           "\r\n"
           "Controls in game:\r\n"
           "  Joystick       D-pad\r\n"
           "  A / B          I / II\r\n"
           "  Center + B     RUN\r\n"
           "  Center + A     SELECT\r\n"
           "\r\n"
           "Back to the game list: RESET.\r\n", &f);
    f_close(&f);
}

/* The emulators share one drive; give it the common name. */
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
                 "Copy HuCard images to",
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

/* ---- crash screen (as in c64) --------------------------------------------
 *
 * There is no serial console on this board, so a fault is shown on the panel:
 * the faulting PC and LR go into arm-none-eabi-addr2line -e build/pbc_pce.elf.
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

/* ---- emulator ------------------------------------------------------------ */

/* fps, speed and CPU load: the launcher's option in the combined firmware,
 * SHOW_FPS on its own. */
static bool show_stats = SHOW_FPS;

static void __attribute__((noreturn)) emulation_loop(void)
{
    uint64_t next_frame = time_us_64();
    uint64_t stat_t0 = time_us_64(), stat_busy_us = 0;
    uint32_t stat_drawn = 0, stat_emulated = 0;

    for (;;) {
        PCE.Joypad.regs[0] = input_poll_pce();
        for (int i = 1; i < 5; i++)
            PCE.Joypad.regs[i] = 0;

        uint64_t frame_start = time_us_64();
        pce_run();
        pce_glue_audio_frame();

        int w, h;
        pce_glue_frame_size(&w, &h);
        if (video_present(w, h))
            stat_drawn++;
        stat_emulated++;

        next_frame += PCE_FRAME_US;
        uint64_t now = time_us_64();
        stat_busy_us += now - frame_start;
        if (now < next_frame)
            sleep_until(from_us_since_boot(next_frame));
        else if (now > next_frame + 4 * PCE_FRAME_US)
            next_frame = now;               /* hopelessly behind: stop catching up */

        now = time_us_64();
        if (show_stats && now - stat_t0 >= 1000000) {
            char s[32];
            snprintf(s, sizeof s, "%lufps %lu%% CPU%lu%%",
                     (unsigned long)stat_drawn,
                     (unsigned long)(stat_emulated * (uint64_t)PCE_FRAME_US * 100 / (now - stat_t0)),
                     (unsigned long)(stat_busy_us * 100 / (now - stat_t0)));
            video_set_status(s);
            stat_t0 = now;
            stat_drawn = stat_emulated = 0;
            stat_busy_us = 0;
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

/* The chip restarted on its own (supply dip, glitch detector, reset request).
 * Says so instead of silently landing in USB stick mode because A or B
 * happened to be held during play. Returns false after a normal start. */
static bool unexpected_reset_report(void)
{
    uint32_t r = powman_hw->chip_reset;
    const char *why;
    if (r & POWMAN_CHIP_RESET_HAD_BOR_BITS)
        why = "Supply voltage dropped";
    else if (r & POWMAN_CHIP_RESET_HAD_GLITCH_DETECT_BITS)
        why = "Glitch detector";
    else if (r & POWMAN_CHIP_RESET_HAD_HZD_SYS_RESET_REQ_BITS)
        why = "System reset request";
    else
        return false;

    char code[32];
    snprintf(code, sizeof code, "(reset %08lx)", (unsigned long)r);
    menu_message("UNEXPECTED RESTART", why, code, "Press a button");
    menu_wait_release();
    while (!input_raw())
        sleep_ms(20);
    menu_wait_release();
    return true;
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
    static char rom[ROM_NAME_MAX];
    const pbc_launch_t *launch = pbc_launch_get();
    if (!launch || !fs_mount())
        reboot_to_menu();
    strncpy(rom, launch->game, sizeof rom - 1);
    show_stats = pbc_launch_stats();
    video_set_mode(pbc_launch_video());
#else
    if (!unexpected_reset_report() && (input_raw() & (IN_A | IN_B)))
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
        uint8_t *scratch = malloc(16384);
        if (!scratch)
            fatal("Out of memory", "for loading");
        menu_progress_begin(rom);
        const char *err = rom_store_program(rom, scratch, menu_progress);
        free(scratch);
        if (err)
            fatal(err, rom);
    }
    fs_unmount();

    static const menu_control_t controls[] = {
        { "D-pad",   "Joystick" },
        { "I",       "Button A" },
        { "II",      "Button B" },
        { "RUN",     "Center + B" },
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

    /* HuCard only: stereo PSG (downmixed in pce_glue.c), no SuperGrafx. */
    SetSgxModePCE(false);
    if (InitPCE(AUDIO_RATE, true) != 0)
        fatal("Emulator start", "out of memory");
    if (LoadCard((uint8_t *)rom_store_data(), rom_store_size()) != 0)
        fatal("Cannot load game:", "not a HuCard");

    multicore_launch_core1(video_core1_main);
    video_wait_ready();             /* the sample timer belongs to core 1 */
    audio_pwm_set_volume(g_settings.sound ? g_settings.volume : 0);

    emulation_loop();
}
