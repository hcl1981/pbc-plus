/*
 * PBC CPC -- Amstrad CPC 6128 emulator for the PicoBoy Color Plus (RP2350A,
 * 520 KB SRAM, 16 MB flash, ST7789 240x280), part of the combined firmware
 * only.
 *
 * Emulator core: core/, written for the PicoBoy (gate array, CRTC, PPI,
 * AY, floppy controller, tapes, snapshots), with the Z80 redcode/Z80
 * (core/redcode, LGPL-3.0-or-later) and the 6128's ROMs (roms/, with
 * Amstrad's permission). The platform layer follows the other emulators:
 *   - the launcher picks the game; the file (.dsk .cdt .sna) is copied into
 *     the flash ROM region and read in place
 *   - every scanline goes to the panel as it is drawn, sent by core 1
 *     (cpc_video.c); sound on the PWM speaker (audio_pwm.c)
 *   - disks and tapes start by themselves (RUN"..., |CPM or |TAPE RUN");
 *     while the tape motor runs the emulation goes as fast as it can and
 *     draws only now and then
 *
 * Buttons: cpc_input.h. RESET goes back to the launcher.
 *
 * Link play (common/pbc_link.h): the two devices emulate in lockstep and
 * swap their buttons every frame; player 2 is joystick 1. The machine has
 * no clock or chance of its own, the game lies in flash on both, and
 * disk writes are dropped (fdc.c), so both run the same. The menu stops
 * both; the player who opened it chooses, and the choice goes over the
 * link so both carry it out in the same frame.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#include "hardware/pll.h"
#include "hardware/vreg.h"
#include "hardware/spi.h"
#include "hardware/watchdog.h"
#include "hardware/structs/qmi.h"

#include "ff.h"

#include "board.h"
#include "pbc_launch.h"
#include "pbc_link.h"
#include "pbc_loading.h"
#include "color565.h"
#include "display.h"
#include "input.h"
#include "menu.h"
#include "rom_store.h"
#include "audio_pwm.h"
#include "settings.h"
#include "cpc_video.h"
#include "cpc_input.h"
#include "cpc.h"

#if !PBC_MULTI
#error "PBC CPC exists only as part of the combined firmware (pbcemu/build.sh)"
#endif

#ifndef CPU_MHZ
#define CPU_MHZ 200
#endif
#ifndef CPU_VREG
#define CPU_VREG VREG_VOLTAGE_1_15
#endif
#ifndef FLASH_MAX_FREQ_MHZ
#define FLASH_MAX_FREQ_MHZ 88
#endif

/* the ROMs, built in (core/roms.S) */
extern const uint8_t cpc_rom6128[], cpc_rom_amsdos[];

static FATFS g_fs;

static bool fs_mount(void) { return f_mount(&g_fs, "", 1) == FR_OK; }

/* ---- clocks (as in msx) ----------------------------------------------------- */

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

/* clk_peri from the USB PLL at 150 MHz: 75 MHz SPI whatever the system clock */
static void display_clock(void)
{
    clock_stop(clk_usb);
    clock_stop(clk_adc);
    pll_init(pll_usb, 1, 1500 * MHZ, 5, 2);
    clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
                    150 * MHZ, 150 * MHZ);
    spi_set_baudrate(PBC_SPI, PBC_SPI_HZ);
}

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

/* ---- the game ------------------------------------------------------------------ */

static char game[ROM_NAME_MAX];
enum { G_DSK, G_CDT, G_SNA };
static int game_kind;

static bool ends_ci(const char *s, const char *ext)
{
    size_t n = strlen(s), m = strlen(ext);
    return n >= m && strcasecmp(s + n - m, ext) == 0;
}

/* (re)start the machine with the game: NULL, or what is wrong */
static const char *start(void)
{
    cpc_init(cpc_rom6128, cpc_rom_amsdos);
    const uint8_t *d = rom_store_data();
    uint32_t n = rom_store_size();
    switch (game_kind) {
    case G_SNA:
        return cpc_load_snapshot(d, n) ? NULL : "Not a CPC snapshot";
    case G_CDT:
        if (!cpc_insert_tape(d, n))
            return "Not a CDT tape";
        cpc_autoload_tape();
        return NULL;
    default:
        if (!cpc_insert_disk(d, n))
            return "Not a CPC disk";
        if (!cpc_autoload_disk())
            return "";                  /* no program found: BASIC waits */
        return NULL;
    }
}

/* ---- the in-game menu (Center + A) ------------------------------------------------ */

enum { M_JOY = 0, M_RESTART = CPC_JOYS, M_REWIND, M_DISK, M_CENTER_B, M_KEYS,
       M_SET_CENTER_B = M_KEYS + 64,    /* + key index: the choice for Center + B */
       M_DISK_DO = M_SET_CENTER_B + 64 };   /* a disk was chosen: its name follows */

#define LINK_DONE 0x8000u               /* with the choice + 1 in bits 0..6 */

static uint16_t link_swap(uint16_t mine)
{
    uint16_t p1, p2;
    pbc_link_frame(mine, NULL, 0, &p1, &p2);
    return pbc_link_player() == 1 ? p2 : p1;
}

static void link_idle(void)
{
    link_swap(0);
}

/* ---- changing disks ---------------------------------------------------------------
 *
 * "Insert disk" lists the drive's CPC disks; the chosen one is copied into
 * the ROM region (the disk the game reads, in flash) and goes into the
 * drive without a restart. In link play the chooser's device sends the
 * hash of the file's name, both look for that file and swap the hash of
 * its start and size; only if they match do both copy it -- piece by piece,
 * a link frame after each, so neither waits long for the other. */
#define DISK_LIST_MAX 100

static uint32_t fold15(uint32_t h)
{
    uint32_t v = (h ^ h >> 15 ^ h >> 30) & 0x7fff;
    return v ? v : 1;
}

static void disk_progress(uint32_t done, uint32_t total)
{
    menu_progress(done, total);
    if (pbc_link_active())
        link_swap(0);
}

/* the disks on the drive (mounted), sorted; returns how many */
static int disk_list(char (*files)[ROM_NAME_MAX], int max)
{
    static DIR dir;
    static FILINFO fi;
    int n = 0;
    if (f_opendir(&dir, "/") != FR_OK)
        return 0;
    while (n < max && f_readdir(&dir, &fi) == FR_OK && fi.fname[0]) {
        if ((fi.fattrib & (AM_DIR | AM_HID | AM_SYS)) || fi.fname[0] == '.' ||
            !ends_ci(fi.fname, ".dsk") || strlen(fi.fname) >= ROM_NAME_MAX)
            continue;
        strcpy(files[n++], fi.fname);
    }
    f_closedir(&dir);
    for (int i = 1; i < n; i++)         /* by name */
        for (int j = i; j > 0 && strcasecmp(files[j - 1], files[j]) > 0; j--) {
            char t[ROM_NAME_MAX];
            strcpy(t, files[j]);
            strcpy(files[j], files[j - 1]);
            strcpy(files[j - 1], t);
        }
    return n;
}

/* what both compare: the file's size and its first 4 KB */
static uint32_t disk_print(const char *name)
{
    static FIL f;
    static uint8_t buf[4096];
    char path[ROM_NAME_MAX + 2];
    UINT got = 0;
    snprintf(path, sizeof path, "/%s", name);
    if (f_open(&f, path, FA_READ) != FR_OK)
        return 0;
    uint32_t size = (uint32_t)f_size(&f);
    f_read(&f, buf, sizeof buf, &got);
    f_close(&f);
    return fold15(pbc_link_hash(pbc_link_hash(PBC_LINK_HASH_START, &size, 4), buf, got));
}

/* `name`: the chooser's file; NULL on the partner, who gets the name's
 * hash over the link. Returns the status line. */
static const char *disk_change(const char *name)
{
    static char files[DISK_LIST_MAX][ROM_NAME_MAX];
    static char shown[32];
    const char *pick = name;
    if (pbc_link_active()) {
        uint32_t want = name ? fold15(pbc_link_hash(PBC_LINK_HASH_START, name, strlen(name))) : 0;
        uint16_t got = link_swap((uint16_t)(LINK_DONE | want));
        if (!name) {                    /* the partner: find the chooser's file */
            want = got & 0x7fff;
            int n = disk_list(files, DISK_LIST_MAX);
            for (int i = 0; i < n && !pick; i++)
                if (fold15(pbc_link_hash(PBC_LINK_HASH_START, files[i], strlen(files[i]))) == want)
                    pick = files[i];
        }
        uint16_t mine = pick ? (uint16_t)disk_print(pick) : 0;
        uint16_t other = link_swap((uint16_t)(LINK_DONE | mine)) & 0x7fff;
        if (!pick || !mine || mine != other)
            return "Partner has no such disk";
    }
    static uint8_t *scratch;
    if (!scratch)
        scratch = malloc(0x4000);
    if (!scratch)
        return "Out of memory";
    menu_progress_begin(pick);
    rom_store_live = true;
    const char *err = rom_store_program(pick, scratch, disk_progress);
    rom_store_live = false;
    if (err)
        return err;
    if (!cpc_insert_disk(rom_store_data(), rom_store_size()))
        return "Not a CPC disk";
    snprintf(shown, sizeof shown, "%s", pick);
    char *dot = strrchr(shown, '.');
    if (dot)
        *dot = 0;
    return shown;
}

/* the chooser: the list, then the change; NULL if nothing was chosen */
static const char *disk_menu(void)
{
    static char files[DISK_LIST_MAX][ROM_NAME_MAX];
    static const char *rows[DISK_LIST_MAX];
    int n = disk_list(files, DISK_LIST_MAX);
    if (n == 0)
        return NULL;
    int cur = -1;
    for (int i = 0; i < n; i++) {
        rows[i] = files[i];
        if (strcmp(files[i], rom_store_current() ? rom_store_current() : "") == 0)
            cur = i;
    }
    int k = menu_pick("Insert disk", rows, n, cur >= 0 ? cur : 0, cur, "in drive",
                      pbc_link_active() ? link_idle : NULL);
    return k >= 0 && k != cur ? files[k] : NULL;
}

static const char *menu_do(int sel)
{
    if (sel < 0)
        return NULL;
    if (sel < CPC_JOYS) {
        cpc_joy_mode = sel;
        return cpc_joy_names[sel];
    }
    switch (sel) {
    case M_RESTART:
        start();
        return "Restart";
    case M_REWIND:
        if (game_kind == G_CDT) {
            cpc_tape_rewind();
            return "Tape rewound";
        }
        cpc_reset();
        return "Reset";
    case M_CENTER_B:                    /* became M_SET_CENTER_B + key in menu() */
    case M_DISK:                        /* became M_DISK_DO in menu() */
        return NULL;
    default:
        if (sel == M_DISK_DO)               /* the partner's side; the chooser's is in menu() */
            return NULL;
        if (sel >= M_SET_CENTER_B) {
            static char s[24];
            if (sel - M_SET_CENTER_B >= cpc_key_count)
                return NULL;
            cpc_center_b = sel - M_SET_CENTER_B;
            snprintf(s, sizeof s, "Center + B: %s", cpc_keys_menu[cpc_center_b].name);
            return s;
        }
        cpc_input_press(sel - M_KEYS);
        return cpc_keys_menu[sel - M_KEYS].name;
    }
}

static const char *menu(void)
{
    static const char *rows[M_KEYS + 64];
    static const char *plain[64];
    static char names[64][20];
    static char joy[CPC_JOYS][28];
    static char center_b[28];
    int n = 0;
    for (int j = 0; j < CPC_JOYS; j++) {
        snprintf(joy[j], sizeof joy[j], "Joystick: %s", cpc_joy_names[j]);
        rows[n++] = joy[j];
    }
    rows[n++] = "Restart";
    rows[n++] = game_kind == G_CDT ? "Rewind tape" : "Reset";
    rows[n++] = game_kind == G_DSK ? "Insert disk" : "(no disk)";
    snprintf(center_b, sizeof center_b, "Center + B: %s", cpc_keys_menu[cpc_center_b].name);
    rows[n++] = center_b;
    for (int i = 0; i < cpc_key_count && i < 64; i++) {
        snprintf(names[i], sizeof names[i], "Key %s", cpc_keys_menu[i].name);
        rows[n++] = names[i];
        plain[i] = cpc_keys_menu[i].name;
    }
    void (*idle)(void) = pbc_link_active() ? link_idle : NULL;
    int sel = menu_pick("Amstrad CPC", rows, n, cpc_joy_mode, cpc_joy_mode, NULL, idle);
    const char *disk = NULL;
    if (sel == M_DISK) {
        /* the drive's file system only for this; the game reads its disk
         * from flash */
        if (game_kind == G_DSK && fs_mount())
            disk = disk_menu();
        sel = disk ? M_DISK_DO : -1;
        if (!disk)
            f_unmount("");
    }
    if (sel == M_CENTER_B) {
        /* which key Center + B gives from now on: in link play for both */
        int k = menu_pick("Center + B", plain, cpc_key_count, cpc_center_b, cpc_center_b,
                          "in use", idle);
        sel = k >= 0 ? M_SET_CENTER_B + k : -1;
    }
    if (pbc_link_active())
        link_swap((uint16_t)(LINK_DONE | (sel + 1)));
    if (sel == M_DISK_DO) {
        const char *s = disk_change(disk);
        f_unmount("");
        return s;
    }
    return menu_do(sel);
}

/* The player who is not choosing: the same choice, from the partner. */
static const char *menu_wait(void)
{
    pbc_loading_message("Amstrad CPC", "Your partner is", "in the menu", NULL);
    uint16_t w;
    while (!((w = link_swap(0)) & LINK_DONE))
        ;
    int sel = (int)(w & 0x7fff) - 1;
    if (sel == M_DISK_DO) {             /* the partner chose a disk: the same one, if here */
        fs_mount();                     /* failing, the list is empty and both say so */
        const char *s = disk_change(NULL);
        f_unmount("");
        return s;
    }
    return menu_do(sel);
}

/* ---- emulation ------------------------------------------------------------------- */

#define FAST_DRAW_EVERY 16              /* real-time loading: one frame in so many is drawn */

static void __attribute__((noreturn)) emulation_loop(bool show_stats)
{
    static int16_t samples[1024];
    uint64_t next_frame = time_us_64();
    uint64_t stat_t0 = next_frame, stat_busy_us = 0;
    uint32_t stat_drawn = 0, stat_emulated = 0;
    uint64_t status_until = 0;
    uint64_t sample_acc = 0;
    uint32_t fast_n = 0;
    bool was_fast = false;

    audio_pwm_set_volume(g_settings.sound ? g_settings.volume : 0);

    for (;;) {
        uint16_t p1, p2;
        pbc_link_frame(cpc_input_word(), cpc_ram(), CPC_RAM_SIZE, &p1, &p2);
        bool menu_now = ((p1 | p2) & CPC_WORD_MENU) != 0;
        /* which player opened it (both at once: player 1) */
        bool menu_mine = !pbc_link_active() ||
                         ((p1 & CPC_WORD_MENU) != 0) == (pbc_link_player() == 1);
        cpc_input_apply(p1 & ~CPC_WORD_MENU, p2 & ~CPC_WORD_MENU);

        uint64_t frame_start = time_us_64();
        uint32_t period = CPC_FRAME_US;
        sample_acc += (uint64_t)AUDIO_RATE * period;
        unsigned n = (unsigned)(sample_acc / 1000000);
        sample_acc -= (uint64_t)n * 1000000;

        bool fast = cpc_tape_busy();
        if (fast != was_fast) {
            cpc_video_set_status(fast ? "LOADING" : "");
            status_until = 0;
            was_fast = fast;
            next_frame = time_us_64();
        }
        bool draw = !fast || (++fast_n % FAST_DRAW_EVERY) == 0;
        cpc_frame(draw ? cpc_video_line : NULL, samples, (int)n);
        if (!fast)
            audio_pwm_push(samples, (int)n);

        if (cpc_video_frame_drawn())
            stat_drawn++;
        stat_emulated++;

        if (menu_now) {
            cpc_video_pause();
            const char *s = menu_mine ? menu() : menu_wait();
            pbc_display_fill(0, 0, PBC_LCD_W, PBC_LCD_H, PBC_BLACK);
            cpc_video_resume();
            cpc_video_set_status(s ? s : "");
            status_until = s ? time_us_64() + 2000000 : 0;
            next_frame = time_us_64();
            stat_t0 = next_frame;
            stat_drawn = stat_emulated = 0;
            stat_busy_us = 0;
            continue;
        }

        uint64_t now = time_us_64();
        stat_busy_us += now - frame_start;
        if (fast || pbc_link_follows()) {
            next_frame = now;           /* as fast as it goes, or the partner's pace */
        } else {
            next_frame += period;
            if (now < next_frame)
                sleep_until(from_us_since_boot(next_frame));
            else if (now > next_frame + 4 * period)
                next_frame = now;       /* hopelessly behind: stop catching up */
        }

        if (pbc_link_out_of_sync_news())
            cpc_video_set_status("LINK OUT OF SYNC");
        now = time_us_64();
        if (pbc_link_out_of_sync())
            continue;
        if (status_until && now >= status_until) {
            status_until = 0;
            cpc_video_set_status("");
            stat_t0 = now;
            stat_drawn = stat_emulated = 0;
            stat_busy_us = 0;
        }
        if (show_stats && !fast && !status_until && now - stat_t0 >= 1000000) {
            char s[32];
            snprintf(s, sizeof s, "%lufps %lu%% CPU%lu%%",
                     (unsigned long)stat_drawn,
                     (unsigned long)(stat_emulated * 100 / 50),
                     (unsigned long)(stat_busy_us * 100 / (now - stat_t0)));
            cpc_video_set_status(s);
            stat_t0 = now;
            stat_drawn = stat_emulated = 0;
            stat_busy_us = 0;
        }
    }
}

/* ---- crash screen ----------------------------------------------------------
 *
 * The faulting PC and LR go into arm-none-eabi-addr2line -e build-multi/pbc_cpc.elf. */
void __attribute__((used, noreturn)) hardfault_report(uint32_t *frame, uint32_t exc_return)
{
    (void)exc_return;
    char l1[32], l2[32], l3[32];
    snprintf(l1, sizeof l1, "Core %u  PC %08lx", get_core_num(), (unsigned long)frame[6]);
    snprintf(l2, sizeof l2, "LR %08lx", (unsigned long)frame[5]);
    snprintf(l3, sizeof l3, "CFSR %08lx", (unsigned long)(*(volatile uint32_t *)0xE000ED28));
    if (get_core_num() == 0)
        multicore_reset_core1();
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

static void __attribute__((noreturn)) reboot_to_menu(void)
{
    menu_wait_release();
    watchdog_reboot(0, 0, 10);
    for (;;) tight_loop_contents();
}

static void __attribute__((noreturn)) fatal(const char *what, const char *detail)
{
    menu_message("Amstrad CPC", what, detail, "Press a button");
    menu_wait_release();
    while (!input_raw())
        sleep_ms(20);
    reboot_to_menu();
}

/* All that makes the game start differently on two devices. */
static uint32_t link_config(void)
{
    uint32_t h = PBC_LINK_HASH_START ^ 10;
    h = pbc_link_hash(h, &game_kind, sizeof game_kind);
    h = pbc_link_hash(h, rom_store_data(), rom_store_size());
    h = pbc_link_hash(h, cpc_rom6128, 0x8000);
    return pbc_link_hash(h, cpc_rom_amsdos, 0x4000);
}

int main(void)
{
    input_init();
    settings_load();
    pbc_display_init();

    const pbc_launch_t *launch = pbc_launch_get();
    if (!launch || !fs_mount())
        reboot_to_menu();
    strncpy(game, launch->game, sizeof game - 1);
    /* the game's options (link play: RETURN and the joystick): bits 0..5
     * the key on Center + B, 6..7 the joystick kind */
    if (!(launch->flags & (PBC_LAUNCH_LINK1 | PBC_LAUNCH_LINK2))) {
        if ((launch->key & 0x3f) < cpc_key_count)
            cpc_center_b = launch->key & 0x3f;
        if ((launch->key >> 6) < CPC_JOYS)
            cpc_joy_mode = launch->key >> 6;
    }
    game_kind = ends_ci(game, ".cdt") ? G_CDT : ends_ci(game, ".sna") ? G_SNA : G_DSK;

    if (!rom_store_matches(game)) {
        uint8_t *scratch = malloc(0x4000);
        if (!scratch)
            fatal("Out of memory", "for loading");
        menu_progress_begin(game);
        const char *err = rom_store_program(game, scratch, menu_progress);
        free(scratch);
        if (err)
            fatal(err, game);
    }
    f_unmount("");

    overclock();
    audio_pwm_init();
    cpc_video_init(pbc_launch_video());

    const char *err = start();
    if (err && err[0])
        fatal(err, game);

    pbc_link_start(link_config());
    pbc_display_fill(0, 0, PBC_LCD_W, PBC_LCD_H, PBC_BLACK);
    multicore_launch_core1(cpc_video_core1_main);
    pbc_fade_in(false);
    emulation_loop(pbc_launch_stats());
}
