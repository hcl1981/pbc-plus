/*
 * PBC ZX -- ZX Spectrum 48K/128K emulator for the PicoBoy Color Plus
 * (RP2350A, 520 KB SRAM, 16 MB flash, ST7789 240x280), part of the combined
 * firmware only.
 *
 * Emulator core: core/, written for the PicoBoy (ULA, 128K paging, AY,
 * tapes, snapshots), with the Z80 redcode/Z80 (core/redcode,
 * LGPL-3.0-or-later) and Amstrad's ROMs (roms/, with their permission).
 * The platform layer follows the other emulators here:
 *   - the launcher picks the game; the file (.tap .tzx .z80 .sna) is copied
 *     into the flash ROM region and read in place
 *   - every scanline goes to the panel as it is drawn, sent by core 1
 *     (zx_video.c); sound on the PWM speaker (audio_pwm.c)
 *   - tapes start by themselves (LOAD "" or the 128K's Tape Loader);
 *     standard blocks load at once, the rest in real time, meanwhile the
 *     emulation runs as fast as it can and draws only now and then
 *
 * Buttons: zx_input.h. RESET goes back to the launcher.
 *
 * Link play (common/pbc_link.h): the two devices emulate in lockstep and
 * swap their buttons every frame; player 2 is the second Sinclair joystick
 * (keys 1-5). The machine has no clock or chance of its own, RAM starts
 * cleared and the game lies in flash on both, so both run the same. The
 * menu stops both; the player who opened it chooses, and the choice goes
 * over the link so both carry it out in the same frame.
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
#include "pbc_loading.h"
#include "pbc_link.h"
#include "color565.h"
#include "display.h"
#include "input.h"
#include "menu.h"
#include "rom_store.h"
#include "audio_pwm.h"
#include "settings.h"
#include "zx_video.h"
#include "zx_input.h"
#include "zx.h"

#if !PBC_MULTI
#error "PBC ZX exists only as part of the combined firmware (pbcemu/build.sh)"
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
extern const uint8_t zx_rom48[], zx_rom128_0[], zx_rom128_1[];

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
enum { G_TAP, G_TZX, G_Z80, G_SNA };
static int game_kind;

static bool ends_ci(const char *s, const char *ext)
{
    size_t n = strlen(s), m = strlen(ext);
    return n >= m && strcasecmp(s + n - m, ext) == 0;
}

/* (re)start the machine with the game; false if the file is no good */
static bool start(int model)
{
    zx_init(model, zx_rom48, zx_rom128_0, zx_rom128_1);
    const uint8_t *d = rom_store_data();
    uint32_t n = rom_store_size();
    switch (game_kind) {
    case G_Z80: return zx_load_snapshot(d, n, true);
    case G_SNA: return zx_load_snapshot(d, n, false);
    default:
        if (!zx_insert_tape(d, n, game_kind == G_TZX))
            return false;
        zx_autoload();
        return true;
    }
}

/* ---- the in-game menu (Center + A) ------------------------------------------------ */

enum { M_JOY = 0, M_48K = ZX_JOYS, M_128K, M_REWIND, M_KEYS };

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

static const char *menu_do(int sel)
{
    bool tape = game_kind == G_TAP || game_kind == G_TZX;
    if (sel < 0)
        return NULL;
    if (sel < ZX_JOYS) {
        zx_joy_mode = sel;
        return zx_joy_names[sel];
    }
    switch (sel) {
    case M_48K:
    case M_128K:
        if (game_kind == G_Z80 || game_kind == G_SNA)
            start(zx_model());          /* a snapshot sets its machine */
        else
            start(sel == M_48K ? ZX_48K : ZX_128K);
        return zx_model() == ZX_48K ? "48K" : "128K";
    case M_REWIND:
        if (tape) {
            zx_tape_rewind();
            return "Tape rewound";
        }
        start(zx_model());
        return "Reset";
    default:
        zx_input_press(sel - M_KEYS);
        return zx_keys_menu[sel - M_KEYS].name;
    }
}

static const char *menu(void)
{
    static const char *rows[M_KEYS + 64];
    static char names[64][20];
    static char joy[ZX_JOYS][24];
    int n = 0;
    for (int j = 0; j < ZX_JOYS; j++) {
        snprintf(joy[j], sizeof joy[j], "Joystick: %s", zx_joy_names[j]);
        rows[n++] = joy[j];
    }
    rows[n++] = "Restart as 48K";
    rows[n++] = "Restart as 128K";
    bool tape = game_kind == G_TAP || game_kind == G_TZX;
    rows[n++] = tape ? "Rewind tape" : "Reset";
    for (int i = 0; i < zx_key_count && i < 64; i++) {
        snprintf(names[i], sizeof names[i], "Key %s", zx_keys_menu[i].name);
        rows[n++] = names[i];
    }
    int sel = menu_pick("ZX Spectrum", rows, n, zx_joy_mode, zx_joy_mode, NULL,
                        pbc_link_active() ? link_idle : NULL);
    if (pbc_link_active())
        link_swap((uint16_t)(LINK_DONE | (sel + 1)));
    return menu_do(sel);
}

/* The player who is not choosing: the same choice, from the partner. */
static const char *menu_wait(void)
{
    pbc_loading_message("ZX Spectrum", "Your partner is", "in the menu", NULL);
    uint16_t w;
    while (!((w = link_swap(0)) & LINK_DONE))
        ;
    return menu_do((int)(w & 0x7f) - 1);
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
        pbc_link_frame(zx_input_word(), zx_ram(), ZX_RAM_SIZE, &p1, &p2);
        bool menu_now = ((p1 | p2) & ZX_WORD_MENU) != 0;
        /* which player opened it (both at once: player 1) */
        bool menu_mine = !pbc_link_active() ||
                         ((p1 & ZX_WORD_MENU) != 0) == (pbc_link_player() == 1);
        zx_input_apply(p1 & ~ZX_WORD_MENU, p2 & ~ZX_WORD_MENU);

        uint64_t frame_start = time_us_64();
        uint32_t period = zx_frame_us();
        sample_acc += (uint64_t)AUDIO_RATE * period;
        unsigned n = (unsigned)(sample_acc / 1000000);
        sample_acc -= (uint64_t)n * 1000000;

        bool fast = zx_tape_busy();
        if (fast != was_fast) {
            zx_video_set_status(fast ? "LOADING" : "");
            status_until = 0;
            was_fast = fast;
            next_frame = time_us_64();
        }
        bool draw = !fast || (++fast_n % FAST_DRAW_EVERY) == 0;
        zx_frame(draw ? zx_video_line : NULL, samples, (int)n);
        if (!fast)
            audio_pwm_push(samples, (int)n);

        if (zx_video_frame_drawn())
            stat_drawn++;
        stat_emulated++;

        if (menu_now) {
            zx_video_pause();
            const char *s = menu_mine ? menu() : menu_wait();
            pbc_display_fill(0, 0, PBC_LCD_W, PBC_LCD_H, PBC_BLACK);
            zx_video_resume();
            zx_video_set_status(s ? s : "");
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
            zx_video_set_status("LINK OUT OF SYNC");
        now = time_us_64();
        if (pbc_link_out_of_sync())
            continue;
        if (status_until && now >= status_until) {
            status_until = 0;
            zx_video_set_status("");
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
            zx_video_set_status(s);
            stat_t0 = now;
            stat_drawn = stat_emulated = 0;
            stat_busy_us = 0;
        }
    }
}

/* ---- crash screen ----------------------------------------------------------
 *
 * The faulting PC and LR go into arm-none-eabi-addr2line -e build-multi/pbc_zx.elf. */
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
    menu_message("ZX Spectrum", what, detail, "Press a button");
    menu_wait_release();
    while (!input_raw())
        sleep_ms(20);
    reboot_to_menu();
}

/* All that makes the game start differently on two devices. */
static uint32_t link_config(void)
{
    uint32_t h = PBC_LINK_HASH_START ^ 11;
    h = pbc_link_hash(h, &game_kind, sizeof game_kind);
    h = pbc_link_hash(h, rom_store_data(), rom_store_size());
    h = pbc_link_hash(h, zx_rom48, 0x4000);
    h = pbc_link_hash(h, zx_rom128_0, 0x4000);
    return pbc_link_hash(h, zx_rom128_1, 0x4000);
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
    game_kind = ends_ci(game, ".tzx") ? G_TZX : ends_ci(game, ".z80") ? G_Z80 :
                ends_ci(game, ".sna") ? G_SNA : G_TAP;

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
    zx_video_init(pbc_launch_video());

    /* tapes on the 128K (its music; the menu restarts as 48K) */
    if (!start(ZX_128K))
        fatal(game_kind == G_TZX ? "Not a TZX tape" : "Not a Spectrum file", game);

    pbc_link_start(link_config());
    pbc_display_fill(0, 0, PBC_LCD_W, PBC_LCD_H, PBC_BLACK);
    multicore_launch_core1(zx_video_core1_main);
    pbc_fade_in(false);
    emulation_loop(pbc_launch_stats());
}
