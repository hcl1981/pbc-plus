/*
 * PBC 2600 -- Atari 2600 emulator for the PicoBoy Color Plus (RP2350A,
 * 520 KB SRAM, 16 MB flash, ST7789 240x280), part of the combined firmware
 * only.
 *
 * Emulator core: core/, written for the PicoBoy (6507, TIA, RIOT, bank
 * switching). The platform layer follows the MSX emulator:
 *   - the launcher picks the game; the cartridge (.a26) is copied into the
 *     flash ROM region and used in place
 *   - every line goes to the panel as it is drawn, sent by core 1
 *     (a26_video.c); sound on the PWM speaker
 *   - link play: player 1 left joystick, player 2 right joystick; both
 *     can work the console switches
 *
 * Buttons: joystick and B (or A) the joystick and its button, Center + B
 * the RESET switch, Center + A SELECT. RESET goes back to the launcher.
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
#include "a26_video.h"
#include "a2600.h"
#include "a7800.h"

#if !PBC_MULTI
#error "PBC 2600 exists only as part of the combined firmware (pbcemu/build.sh)"
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

static FATFS g_fs;

static bool fs_mount(void) { return f_mount(&g_fs, "", 1) == FR_OK; }

/* ---- clocks (as in msx) -------------------------------------------------- */

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

/* 75 MHz display SPI from the USB PLL (as in megadrive, pce, msx) */
static void display_clock(void)
{
    clock_stop(clk_usb);
    clock_stop(clk_adc);
    pll_init(pll_usb, 1, 1500 * MHZ, 5, 2);          /* 12 MHz x 125 / 10 */
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

/* ---- input ------------------------------------------------------------------
 *
 * A player's state for one frame is the IN_* bits of the buttons (link
 * play swaps them with the partner). 2600: B or A is the button; 7800: B
 * the left one, A the right one, Center + Down PAUSE. */
static bool is7800;

static uint8_t joy_of(uint16_t w)
{
    uint8_t j = 0;
    bool center = w & IN_CENTER;
    if ((w & IN_UP) && !center)    j |= A26_UP;
    if ((w & IN_DOWN) && !center)  j |= A26_DOWN;
    if (w & IN_LEFT)  j |= A26_LEFT;
    if (w & IN_RIGHT) j |= A26_RIGHT;
    if (!center) {
        if (is7800) {
            if (w & IN_B) j |= A26_FIRE;
            if (w & IN_A) j |= A26_FIRE2;
        } else if (w & (IN_A | IN_B)) {
            j |= A26_FIRE;
        }
    }
    return j;
}

static uint8_t switches_of(uint16_t w)
{
    uint8_t s = 0;
    if ((w & IN_CENTER) && (w & IN_B)) s |= A26_RESET;
    if ((w & IN_CENTER) && (w & IN_A)) s |= A26_SELECT;
    if (is7800 && (w & IN_CENTER) && (w & IN_DOWN)) s |= A26_BW;      /* PAUSE */
    return s;
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
    /* PAL or NTSC: what the game makes, 312 or 262 lines a frame; the
     * switch waits for 30 frames in a row (the lines count is the
     * emulation's, so both devices switch in the same frame) */
    bool pal = is7800 && a7800_pal();
    int pal_votes = 0;

    if (start_status) {
        a26_video_set_status(start_status);
        status_until = time_us_64() + 3000000;
    }
    audio_pwm_set_volume(g_settings.sound ? g_settings.volume : 0);

    for (;;) {
        uint16_t p1, p2;
        pbc_link_frame((uint16_t)(input_raw() & 0x7F), is7800 ? a7800_ram() : a2600_ram(),
                       is7800 ? 0x1000 : 128, &p1, &p2);
        a26_joy[0] = joy_of(p1);
        a26_joy[1] = joy_of(p2);
        a26_switches = switches_of(p1 | p2);

        uint64_t frame_start = time_us_64();
        const unsigned fps = pal ? 50 : 60;
        sample_acc += AUDIO_RATE;
        unsigned n = sample_acc / fps;
        sample_acc -= n * fps;
        int lines;
        if (is7800) {
            a7800_frame(a26_video_line320, samples, (int)n);
            lines = pal ? 313 : 263;
        } else {
            lines = a2600_frame(a26_video_line, samples, (int)n);
        }
        audio_pwm_push(samples, (int)n);
        if (!is7800 && (lines >= 290) != pal) {
            if (++pal_votes >= 30) {
                pal = !pal;
                pal_votes = 0;
                a26_video_set_pal(pal);
            }
        } else {
            pal_votes = 0;
        }

        if (a26_video_frame_drawn())
            stat_drawn++;
        stat_emulated++;

        /* 262 lines at 1.193 MHz (NTSC), 312 at 1.182 MHz (PAL); the 7800
         * 263 or 313 lines of 454 clocks at 7.16 / 7.09 MHz */
        const uint32_t period = is7800 ? (pal ? 20033 : 16683) : pal ? 20056 : 16688;
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
            a26_video_set_status("LINK OUT OF SYNC");
        now = time_us_64();
        if (pbc_link_out_of_sync())
            continue;
        if (status_until && now >= status_until) {
            status_until = 0;
            a26_video_set_status("");
            stat_t0 = now;
            stat_drawn = stat_emulated = 0;
            stat_busy_us = 0;
        }
        if (show_stats && !status_until && now - stat_t0 >= 1000000) {
            char s[32];
            snprintf(s, sizeof s, "%lufps %lu%% CPU%lu%%",
                     (unsigned long)stat_drawn,
                     (unsigned long)(stat_emulated * 100 / fps),
                     (unsigned long)(stat_busy_us * 100 / (now - stat_t0)));
            a26_video_set_status(s);
            stat_t0 = now;
            stat_drawn = stat_emulated = 0;
            stat_busy_us = 0;
        }
    }
}

/* ---- crash screen ---------------------------------------------------------- */

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
    menu_message("ATARI 2600", what, detail, "Press a button");
    menu_wait_release();
    while (!input_raw())
        sleep_ms(20);
    reboot_to_menu();
}

int main(void)
{
    static char game[ROM_NAME_MAX];
    input_init();
    settings_load();
    pbc_display_init();

    const pbc_launch_t *launch = pbc_launch_get();
    if (!launch || !fs_mount())
        reboot_to_menu();
    strncpy(game, launch->game, sizeof game - 1);

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
    a26_video_init(pbc_launch_video());

    const char *ext = strrchr(game, '.');
    is7800 = ext && strcasecmp(ext, ".a78") == 0;
    if (is7800) {
        if (!a7800_insert(rom_store_data(), rom_store_size()))
            fatal("Not a 7800 cartridge", game);
        a26_video_set_pal(a7800_pal());
        a7800_reset();
    } else {
        int type = a2600_insert(rom_store_data(), rom_store_size());
        if (type < 0)
            fatal("Unknown cartridge type", game);
        a2600_reset();
    }

    pbc_link_start(pbc_link_hash(PBC_LINK_HASH_START ^ (is7800 ? 9 : 8), rom_store_data(), rom_store_size()));
    pbc_display_fill(0, 0, PBC_LCD_W, PBC_LCD_H, PBC_BLACK);
    multicore_launch_core1(a26_video_core1_main);
    pbc_fade_in(false);
    emulation_loop(pbc_launch_stats(), NULL);
}
