/*
 * PBC MSX -- MSX1 emulator for the PicoBoy Color Plus (RP2350A, 520 KB SRAM,
 * 16 MB flash, ST7789 240x280), part of the combined firmware only.
 *
 * Emulator core: core/msx1, written for the PicoBoy (TMS9918A video,
 * AY-3-8910 sound, Konami SCC, slots, PPI, MegaROM mappers), with the Z80
 * redcode/Z80 (core/redcode, LGPL-3.0-or-later) and C-BIOS 0.29a as the
 * system ROM (core/cbios, BSD-style). The platform layer follows the other
 * emulators here:
 *   - the launcher picks the game; the cartridge (.rom .mx1) is copied into
 *     the flash ROM region and used in place
 *   - every scanline goes to the panel as it is drawn, sent by core 1
 *     (msx_video.c); sound on the PWM speaker (audio_pwm.c)
 *   - link play: player 1 joystick 1 and keyboard, player 2 joystick 2
 *
 * Buttons: msx_input.h. RESET goes back to the launcher.
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
#include "msx_video.h"
#include "msx_input.h"
#include "msx1.h"

#if !PBC_MULTI
#error "PBC MSX exists only as part of the combined firmware (pbcemu/build.sh)"
#endif

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

/* C-BIOS for MSX1, built in (core/cbios/cbios.S) */
extern const uint8_t cbios_main[], cbios_logo[];

static FATFS g_fs;

static bool fs_mount(void) { return f_mount(&g_fs, "", 1) == FR_OK; }

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
 * follows clk_sys: at 200 MHz the display got only 50 MHz, too slow for
 * 192 lines a frame. USB is used only before the game (stick mode), so in
 * the game the USB PLL runs at 150 MHz and drives clk_peri alone: 75 MHz
 * SPI whatever the system clock (as in megadrive and pce). */
static void display_clock(void)
{
    clock_stop(clk_usb);
    clock_stop(clk_adc);
    pll_init(pll_usb, 1, 1500 * MHZ, 5, 2);          /* 12 MHz x 125 / 10 */
    clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
                    150 * MHZ, 150 * MHZ);
    spi_set_baudrate(PBC_SPI, PBC_SPI_HZ);
}

/* After the ROM region has been written. */
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

/* ---- the game being played ------------------------------------------------- */

static char game[ROM_NAME_MAX];

/* ---- the in-game menu (Center + A) -----------------------------------------------
 *
 * Pressing any key of the MSX keyboard. In link play both devices stop
 * after the same frame (MSX_WORD_MENU in one of the two words); the player
 * who asked chooses, the other one waits, and both go on swapping words
 * without emulating until the chooser sends DONE. The key goes out with
 * the chooser's next words, so both machines see it in the same frame. */
#define LINK_DONE 0x8000u

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

static const char *menu(void)
{
    static const char *rows[64];
    static char names[64][16];
    int n = 0;
    for (int i = 0; i < msx_key_count && n < 64; i++) {
        snprintf(names[n], sizeof names[n], "Key %s", msx_keys_menu[i].name);
        rows[n] = names[n];
        n++;
    }
    int sel = menu_pick("MSX", rows, n, 0, -1, NULL, pbc_link_active() ? link_idle : NULL);
    if (sel >= 0)
        msx_input_press(sel);           /* goes out with the next words */
    if (pbc_link_active())
        link_swap(LINK_DONE);
    return sel >= 0 ? msx_keys_menu[sel].name : NULL;
}

/* The player who is not choosing. */
static const char *menu_wait(void)
{
    pbc_loading_message("MSX", "Your partner is", "in the menu", NULL);
    while (!(link_swap(0) & LINK_DONE))
        ;
    return NULL;
}

/* ---- emulation ------------------------------------------------------------- */

static void __attribute__((noreturn)) emulation_loop(bool show_stats, const char *start_status)
{
    static int16_t samples[512];
    uint64_t next_frame = time_us_64();
    uint64_t stat_t0 = next_frame, stat_busy_us = 0;
    uint32_t stat_drawn = 0, stat_emulated = 0;
    uint64_t status_until = 0;
    uint32_t sample_acc = 0;
    bool menu_mine = true;

    if (start_status) {
        msx_video_set_status(start_status);
        status_until = time_us_64() + 4000000;
    }
    audio_pwm_set_volume(g_settings.sound ? g_settings.volume : 0);

    for (;;) {
        uint16_t p1, p2;
        pbc_link_frame(msx_input_word(), msx1_ram(), 0x10000, &p1, &p2);
        bool menu_now = ((p1 | p2) & MSX_WORD_MENU) != 0;
        if (menu_now && pbc_link_active())
            menu_mine = ((p1 & MSX_WORD_MENU) != 0) == (pbc_link_player() == 1);
        msx_input_apply(p1 & ~MSX_WORD_MENU, p2 & ~MSX_WORD_MENU);

        uint64_t frame_start = time_us_64();
        /* sound: the frame's share of the sample rate (60 frames per second) */
        sample_acc += AUDIO_RATE;
        unsigned n = sample_acc / 60;
        sample_acc -= n * 60;
        msx1_frame(msx_video_line, samples, (int)n);
        audio_pwm_push(samples, (int)n);

        if (msx_video_frame_drawn())
            stat_drawn++;
        stat_emulated++;

        if (menu_now) {
            msx_video_pause();
            const char *s = menu_mine ? menu() : menu_wait();
            pbc_display_fill(0, 0, PBC_LCD_W, PBC_LCD_H, PBC_BLACK);
            msx_video_resume();
            msx_video_set_status(s ? s : "");
            status_until = s ? time_us_64() + 2000000 : 0;
            next_frame = time_us_64();      /* the pause is not to be caught up */
            stat_t0 = next_frame;
            stat_drawn = stat_emulated = 0;
            stat_busy_us = 0;
            continue;
        }

        const uint32_t period = 16688;      /* 262 lines */
        next_frame += period;
        uint64_t now = time_us_64();
        stat_busy_us += now - frame_start;
        if (pbc_link_follows())
            next_frame = now;               /* the partner's frames are the clock */
        else if (now < next_frame)
            sleep_until(from_us_since_boot(next_frame));
        else if (now > next_frame + 4 * period)
            next_frame = now;               /* hopelessly behind: stop catching up */

        if (pbc_link_out_of_sync_news())
            msx_video_set_status("LINK OUT OF SYNC");
        now = time_us_64();
        if (pbc_link_out_of_sync())
            continue;
        if (status_until && now >= status_until) {
            status_until = 0;
            msx_video_set_status("");
            stat_t0 = now;
            stat_drawn = stat_emulated = 0;
            stat_busy_us = 0;
        }
        if (show_stats && !status_until && now - stat_t0 >= 1000000) {
            char s[32];
            snprintf(s, sizeof s, "%lufps %lu%% CPU%lu%%",
                     (unsigned long)stat_drawn,
                     (unsigned long)(stat_emulated * 100 / 60),
                     (unsigned long)(stat_busy_us * 100 / (now - stat_t0)));
            msx_video_set_status(s);
            stat_t0 = now;
            stat_drawn = stat_emulated = 0;
            stat_busy_us = 0;
        }
    }
}

/* ---- crash screen ----------------------------------------------------------
 *
 * There is no serial console on this board, so a fault is shown on the panel:
 * the faulting PC and LR go into arm-none-eabi-addr2line -e build-multi/pbc_msx.elf.
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

/* Back to the launcher, once every button is released. */
static void __attribute__((noreturn)) reboot_to_menu(void)
{
    menu_wait_release();
    watchdog_reboot(0, 0, 10);
    for (;;) tight_loop_contents();
}

static void __attribute__((noreturn)) fatal(const char *what, const char *detail, const char *l3)
{
    menu_message("MSX", what, detail, l3 ? l3 : "Press a button");
    menu_wait_release();
    while (!input_raw())
        sleep_ms(20);
    reboot_to_menu();
}

/* All that makes the game start differently on two devices. */
static uint32_t link_config(void)
{
    uint32_t h = PBC_LINK_HASH_START ^ 7;
    h = pbc_link_hash(h, rom_store_data(), rom_store_size());
    h = pbc_link_hash(h, cbios_main, 0x8000);
    return h;
}

int main(void)
{
    input_init();
    settings_load();
    pbc_display_init();

    /* the launcher has picked the game and owns the USB drive mode */
    const pbc_launch_t *launch = pbc_launch_get();
    if (!launch || !fs_mount())
        reboot_to_menu();
    strncpy(game, launch->game, sizeof game - 1);

    if (!rom_store_matches(game)) {
        uint8_t *scratch = malloc(0x8000);
        if (!scratch)
            fatal("Out of memory", "for loading", NULL);
        menu_progress_begin(game);
        const char *err = rom_store_program(game, scratch, menu_progress);
        free(scratch);
        if (err)
            fatal(err, game, NULL);
    }
    f_unmount("");

    overclock();
    audio_pwm_init();
    msx_video_init(pbc_launch_video());

    msx1_init(cbios_main, cbios_logo);
    int mapper = msx1_insert(rom_store_data(), rom_store_size());
    if (mapper < 0)
        fatal("Not an MSX cartridge", game, NULL);
    msx1_reset();

    pbc_link_start(link_config());
    pbc_display_fill(0, 0, PBC_LCD_W, PBC_LCD_H, PBC_BLACK);
    multicore_launch_core1(msx_video_core1_main);
    pbc_fade_in(false);                 /* over the first frames */
    emulation_loop(pbc_launch_stats(), mapper ? msx1_mapper_name(mapper) : NULL);
}
