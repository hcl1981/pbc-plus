/*
 * PBC C64 -- Commodore 64 emulator for the PicoBoy Color Plus
 * (RP2350A, 520 KB SRAM, 16 MB flash, ST7789 240x280).
 *
 * Emulator core: Frodo4 (Christian Bauer) as ported in frank-c64 by
 * rh1tech, GPL-2.0-or-later, in its line-based mode. The platform layer is
 * the one from megadrive and nes:
 *   - disk images (.d64), programs (.prg) and cartridges (.crt) come over USB mass storage into
 *     a flash FatFs partition (from PicoBoyGB); Frodo uses the image there
 *     directly, writes included (saved games), see flash_fs.c
 *   - no keyboard: after boot LOAD"*",8,1 is typed automatically (or the
 *     program chosen per game in the options), and RUN once BASIC is back
 *     at READY (a program that starts itself never is)
 *   - the joystick port is chosen per game in the options of the ROM list
 *   - 320x240 of the picture scaled to 240x180 on the ST7789 (c64_video.c)
 *   - SID on the PWM speaker (audio_pwm.c), off by default, on/off and
 *     volume in the options; its sample timer runs on core 1
 *
 * Boot:
 *   A or B held  -> USB stick mode, eject restarts
 *   otherwise    -> ROM list, then the game (RESET leaves it)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
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
#include "audio_pwm.h"
#include "settings.h"
#include "c64_glue.h"
#include "c64_input.h"
#include "c64_video.h"
#include "fatfs_stdio.h"
#include "openroms_syms.h"
#include "c64_roms.h"
#include "cart_store.h"

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
/* With the processor-level 1541 a second 6502 runs along: more clock, at
 * the voltage the Mega Drive emulator runs 200 MHz with. */
#define TRUE_DRIVE_MHZ  200
#define TRUE_DRIVE_VREG VREG_VOLTAGE_1_15

/* PAL: 312 lines x 63 cycles at 985248 Hz = 50.125 Hz */
#define FRAME_US 19950u

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
    if (f_stat("/C64.TXT", &fi) == FR_OK)
        return;
    if (f_open(&f, "/C64.TXT", FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
        return;
    f_puts("PBC C64 - Commodore 64 emulator for the PicoBoy Color Plus\r\n"
           "\r\n"
           "Copy disk images (.d64), programs (.prg) or cartridges (.crt) to this\r\n"
           "drive, then EJECT\r\n"
           "it. The PicoBoy restarts by itself and shows the game list. A game\r\n"
           "starts with LOAD\"*\",8,1 and RUN, typed automatically.\r\n"
           "\r\n"
           "Back to this mode: \"Add games via USB\" at the top of the game list,\r\n"
           "or hold A or B while switching on.\r\n"
           "In the game list, B opens the options (joystick port, file, sound).\r\n"
           "\r\n"
           "Built in are the free MEGA65 OpenROMs. If you own the Commodore ROMs,\r\n"
           "copy kernal.rom and basic.rom (optionally chargen.rom) here: some\r\n"
           "games only run with the original KERNAL (901227-02/-03).\r\n"
           "Games with their own fast loader need \"True drive\" in the options\r\n"
           "and the 1541's ROM here as 1541.rom (16 KB).\r\n"
           "\r\n"
           "Controls in game:\r\n"
           "  Joystick + B   joystick + fire\r\n"
           "  A              RETURN\r\n"
           "  Center + B     SPACE (other keys: options)\r\n"
           "  Center + A     choose the disk in drive 8\r\n"
           "\r\n"
           "Back to the game list: RESET.\r\n", &f);
    f_close(&f);
}

/* megadrive, nes and c64 share one drive; give it the common name. */
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
                 "Copy .d64, .prg or .crt",
                 "to the drive, then eject.",
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
static void overclock(int mhz, enum vreg_voltage vreg)
{
    /* At 150 MHz nothing else would set the flash clock: no boot2 is
     * linked in, so the bootrom's setting would stay. Divisor 2 = 75 MHz. */
    if (mhz <= 150)
        set_flash_timings(mhz);
    if (mhz > 150) {
        if (vreg > VREG_VOLTAGE_1_30)
            vreg_disable_voltage_limit();
        vreg_set_voltage(vreg);
        sleep_ms(10);
        set_flash_timings(mhz);
        if (!set_sys_clock_khz(mhz * 1000, false)) {
            set_flash_timings(250);
            set_sys_clock_khz(250000, true);
        }
    }
    /* clk_peri follows clk_sys: re-derive the SPI divider. */
    spi_set_baudrate(PBC_SPI, PBC_SPI_HZ);
}

/* ---- autostart ------------------------------------------------------------
 *
 * There is no keyboard, so the emulator types for the player. The C64
 * sitting in the OpenROMs screen editor's wait-for-key loop means BASIC is
 * idle at READY. Sampled once per frame, so an interrupt
 * that happens to be running at that moment just delays things a frame.
 *
 *   boot   -> idle for a while: mount the image, type LOAD"*",8,1 -- or
 *             LOAD"<file>",8,1 for the program chosen in the options
 *   typed  -> BASIC has taken the line out of the keyboard buffer ($C6 = 0)
 *   loaded -> idle again for half a second: the program did not start
 *             itself, so type RUN. A program that does start itself never
 *             returns to that loop, and RUN is never typed.
 *
 * The keyboard buffer holds 10 characters, so a longer command is fed in
 * pieces, each as soon as the screen editor has emptied the buffer.
 */
enum { AS_BOOT, AS_TYPED, AS_LOADING, AS_DONE };

static bool own_roms;       /* the player's Commodore ROMs from the drive */
static bool true_drive;     /* processor-level 1541: the disk is in from the start */

/* The screen editor waiting for a key: Commodore KERNAL at $E5CD-$E5D5,
 * OpenROMs at the addresses from its build. */
static bool basic_idle(void)
{
    uint16_t pc = c64_get_pc();
    if (own_roms)
        return pc >= 0xE5CD && pc <= 0xE5D5;
    return (pc >= OPENROMS_CHRIN_KEYBOARD_REPEAT && pc < OPENROMS_CHRIN_KEYBOARD_EMPTY_LINE) ||
           (pc >= OPENROMS_CHRIN_KEYBOARD_READ && pc < OPENROMS_CHRIN_KEYBOARD_ENTER) ||
           (pc >= OPENROMS_CURSOR_ENABLE && pc < OPENROMS_CURSOR_ENABLE_END);
}

static bool is_disk(const char *path)
{
    const char *ext = strrchr(path, '.');
    return ext && strcasecmp(ext, ".d64") == 0;
}

static void autostart_step(const char *path, const char *file)
{
    static int state = AS_BOOT, idle_frames, frames;
    static char cmd[40];
    static int cmd_len, cmd_pos;
    uint8_t *ram = c64_get_ram();

    if (cart_store_is_cart_name(path))
        return;                         /* a cartridge starts itself */
    idle_frames = basic_idle() ? idle_frames + 1 : 0;
    frames++;
    switch (state) {
    case AS_BOOT:
        /* the real 1541 boots along with the C64: give it three seconds
         * (in frames since the start; idle_frames starts over whenever an
         * interrupt happens to run at the sample) */
        if (idle_frames >= 10 && (!true_drive || frames >= 150)) {
            if (is_disk(path)) {
                c64_mount_disk(NULL, 0, path);
                /* spelled out: OpenROMs BASIC need not know the abbreviations */
                cmd_len = snprintf(cmd, sizeof cmd, "LOAD\"%s\",8,1\r", file[0] ? file : "*");
                cmd_pos = 0;
            } else {
                c64_load_file(path);   /* .prg: straight into RAM, then RUN */
            }
            state = AS_TYPED;
        }
        break;
    case AS_TYPED:
        if (ram[0xC6] != 0)
            break;                     /* the editor is still reading */
        if (cmd_pos < cmd_len) {
            char chunk[11];
            int n = cmd_len - cmd_pos > 10 ? 10 : cmd_len - cmd_pos;
            memcpy(chunk, cmd + cmd_pos, (size_t)n);
            chunk[n] = 0;
            c64_type_string(chunk);
            cmd_pos += n;
            break;
        }
        idle_frames = 0;
        state = AS_LOADING;
        break;
    case AS_LOADING:
        if (idle_frames >= 25) {
            /* .prg was started with RUN already; only disks need it */
            if (is_disk(path))
                c64_type_string("RUN\r");
            state = AS_DONE;
        }
        break;
    }
}

/* A message for the first seconds of the game, or NULL. */
static const char *start_status;

/* ---- changing disks -------------------------------------------------------
 *
 * Center + A pauses the game and lists every disk image on the drive, as
 * the game list does; the one in drive 8 is marked. The chosen one goes in.
 * The list lives on the heap only while it is shown. */
#define DISK_LIST_MAX 200

static char disk_now[ROM_NAME_MAX];     /* in drive 8, "" for none */

static int by_name(const void *a, const void *b)
{
    return strcasecmp((const char *)a, (const char *)b);
}

/* The game is paused meanwhile. Returns a status line to show, or NULL. */
static const char *disk_menu(void)
{
    static DIR dir;
    static FILINFO fi;
    char (*files)[ROM_NAME_MAX] = malloc(DISK_LIST_MAX * sizeof *files);
    char (*shown)[ROM_NAME_MAX] = malloc(DISK_LIST_MAX * sizeof *shown);
    const char **rows = malloc(DISK_LIST_MAX * sizeof *rows);
    int n = 0;
    if (files && shown && rows && f_opendir(&dir, "/") == FR_OK) {
        while (n < DISK_LIST_MAX && f_readdir(&dir, &fi) == FR_OK && fi.fname[0]) {
            if (fi.fattrib & (AM_DIR | AM_HID | AM_SYS) || fi.fname[0] == '.' ||
                !is_disk(fi.fname) || strlen(fi.fname) >= ROM_NAME_MAX)
                continue;
            strcpy(files[n++], fi.fname);
        }
        f_closedir(&dir);
    }
    const char *status = NULL;
    if (n == 0) {
        status = "No disk images";
    } else {
        qsort(files, (size_t)n, sizeof *files, by_name);
        int cur = -1;
        for (int i = 0; i < n; i++) {
            if (strcmp(files[i], disk_now) == 0)
                cur = i;
            strcpy(shown[i], files[i]);
            char *dot = strrchr(shown[i], '.');
            if (dot)
                *dot = 0;
            rows[i] = shown[i];
        }
        c64_video_wait_idle();
        int sel = menu_pick_disk("Insert disk", rows, n, cur);
        pbc_display_fill(0, 0, PBC_LCD_W, PBC_LCD_H, PBC_BLACK);
        if (sel >= 0 && sel != cur) {
            char path[ROM_NAME_MAX + 2];
            snprintf(path, sizeof path, "/%s", files[sel]);
            c64_mount_disk(NULL, 0, path);
            strcpy(disk_now, files[sel]);
            static char msg[32];
            snprintf(msg, sizeof msg, "%s", shown[sel]);
            status = msg;
        }
    }
    free(rows);
    free(shown);
    free(files);
    return status;
}

/* ---- emulator ------------------------------------------------------------ */

/* fps, speed and load under the picture: from the launcher in the combined
 * firmware, else the SHOW_FPS build option. */
static bool show_stats;

static void __attribute__((noreturn)) emulation_loop(const char *path, const char *file)
{
    uint64_t next_frame = time_us_64();
    uint64_t stat_t0 = time_us_64(), stat_busy_us = 0;
    uint32_t stat_drawn = 0, stat_emulated = 0;
    uint64_t status_until = 0;              /* a message holds the status line */
    if (start_status) {
        c64_video_set_status(start_status);
        status_until = time_us_64() + 4000000;
    }

    for (;;) {
        uint64_t frame_start = time_us_64();
        c64_run_frame();
        c64_audio_flush();
        if (c64_video_present())
            stat_drawn++;
        stat_emulated++;
        autostart_step(path, file);
        if (c64_input_disk_change()) {
            const char *s = disk_menu();
            c64_video_set_status(s ? s : "");
            status_until = s ? time_us_64() + 2000000 : 0;
            next_frame = time_us_64();      /* the pause is not to be caught up */
            stat_t0 = next_frame;
            stat_drawn = stat_emulated = 0;
            stat_busy_us = 0;
            continue;
        }
        fatfs_stdio_sync_idle(25);          /* saved games reach the flash */

        next_frame += FRAME_US;
        uint64_t now = time_us_64();
        stat_busy_us += now - frame_start;
        if (now < next_frame)
            sleep_until(from_us_since_boot(next_frame));
        else if (now > next_frame + 4 * FRAME_US)
            next_frame = now;               /* hopelessly behind: stop catching up */

        now = time_us_64();
        if (status_until && now >= status_until) {
            status_until = 0;
            c64_video_set_status("");
            stat_t0 = now;                  /* the statistics start over */
            stat_drawn = stat_emulated = 0;
            stat_busy_us = 0;
        }
        if (show_stats && !status_until && now - stat_t0 >= 1000000) {
            char s[32];
            snprintf(s, sizeof s, "%lufps %lu%% CPU%lu%%",
                     (unsigned long)stat_drawn,
                     (unsigned long)(stat_emulated * 100 / 50),
                     (unsigned long)(stat_busy_us * 100 / (now - stat_t0)));
            c64_video_set_status(s);
            stat_t0 = now;
            stat_drawn = stat_emulated = 0;
            stat_busy_us = 0;
        }
    }
}

/* ---- crash screen ----------------------------------------------------------
 *
 * There is no serial console on this board, so a fault is shown on the panel:
 * the faulting PC and LR go into arm-none-eabi-addr2line -e build/pbc_c64.elf.
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

#if PBC_MULTI
/* Back to the launcher, once every button is released. */
static void __attribute__((noreturn)) reboot_to_menu(void)
{
    menu_wait_release();
    watchdog_reboot(0, 0, 10);
    for (;;) tight_loop_contents();
}
#endif

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
    /* The volume stays mounted: Frodo reads and writes the disk image while
     * the game runs. */

    own_roms = c64_roms_load();
    if (own_roms)
        c64_set_roms(c64_roms_basic(), c64_roms_kernal(), c64_roms_chargen());
    /* A cartridge: its banks into the ROM region the first time */
    bool cart = cart_store_is_cart_name(game);
    if (cart && !cart_store_matches(game)) {
#if PBC_MULTI
        pbc_loading_begin(game);
        const char *err = cart_store_program(game, pbc_loading_progress);
#else
        menu_message("LOADING CARTRIDGE", game, "Only needed the first time", NULL);
        const char *err = cart_store_program(game, NULL);
#endif
        if (err) {
            menu_message("ERROR", err, game, "Press a button");
            menu_wait_release();
            while (!input_raw())
                sleep_ms(20);
            menu_wait_release();
            watchdog_reboot(0, 0, 10);  /* back to the game list */
            for (;;) tight_loop_contents();
        }
    }

    /* "True drive": the 1541's own ROM from the drive, else DOS level */
    true_drive = settings_true_drive(game) && c64_roms_load_drive();
    if (settings_true_drive(game) && !true_drive)
        start_status = "No 1541.rom: fast drive";

    int port = settings_port(game);
    c64_input_set_port(port);
    static const char *const key_names[] = { C64_KEYS(C64_KEY_NAME) };
    int key = settings_key(game);
    c64_input_set_key(key);
    if (is_disk(game))
        snprintf(disk_now, sizeof disk_now, "%s", game);   /* autostart mounts it */

    char port_text[16];
    snprintf(port_text, sizeof port_text, "Port %d", port);
    const menu_control_t controls[] = {
        { "Stick",   port_text },
        { "Fire",    "Button B" },
        { "RETURN",  "Button A" },
        { key_names[key], "Center + B" },
        { "Disk",    "Center + A" },
        { "ROMs",    own_roms ? "Commodore" : "OpenROMs" },
    };
#if PBC_MULTI
    (void)controls;                 /* combined firmware: the launcher showed them */
#else
    menu_controls(controls, sizeof controls / sizeof controls[0]);
#endif

    pbc_display_fill(0, 0, PBC_LCD_W, PBC_LCD_H, PBC_BLACK);
    menu_wait_release();

    if (true_drive)
        overclock(TRUE_DRIVE_MHZ > CPU_MHZ ? TRUE_DRIVE_MHZ : CPU_MHZ,
                  TRUE_DRIVE_VREG > CPU_VREG ? TRUE_DRIVE_VREG : CPU_VREG);
    else
        overclock(CPU_MHZ, CPU_VREG);
    audio_pwm_init();

    c64_init();
#if PBC_MULTI
    show_stats = pbc_launch_stats();
    c64_video_set_mode(pbc_launch_video());
#else
    show_stats = SHOW_FPS;
#endif
    multicore_launch_core1(c64_video_core1_main);
    c64_video_wait_ready();         /* the sample timer belongs to core 1 */
    audio_pwm_set_volume(g_settings.sound ? g_settings.volume : 0);

    static char path[ROM_NAME_MAX + 2];
    static char file[SETTINGS_FILE_MAX + 1];
    snprintf(path, sizeof path, "/%s", game);
    settings_file(game, file);
    /* Switching to the real 1541 resets its CPU: do it now, so that it
     * boots with the C64 and is ready when LOAD comes (the autostart's
     * mount of the same file then changes nothing). */
    if (true_drive && is_disk(game))
        c64_mount_disk(NULL, 0, path);
    if (cart)
        c64_load_cartridge(path);       /* plugged in, the C64 reset with it */
#if PBC_MULTI
    pbc_fade_in(false);             /* over the first frames */
#endif
    emulation_loop(path, file);
}
