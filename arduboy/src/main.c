/*
 * PBC Arduboy -- the Arduboy for the PicoBoy Color Plus (RP2350A, 520 KB
 * SRAM, 16 MB flash, ST7789 240x280), part of the combined firmware only.
 *
 * Emulator core: core/arduboy.c, written for the PicoBoy (ATmega32U4 at
 * 16 MHz, timers, SPI with the SSD1306 OLED, EEPROM, speaker). The platform
 * layer follows the other emulators here:
 *   - the launcher picks the game, an Intel HEX file (.hex) as the Arduino
 *     IDE writes it; it is read into RAM, the program decoded once
 *   - each frame (60 a second) goes to core 1, which scales the 128x64
 *     picture to the panel (ard_video.c); sound on the PWM speaker
 *   - the EEPROM (1 KB) is kept as <game>.sav on the drive
 *
 * Buttons: the D-pad and A/B as on the Arduboy. RESET goes back to the
 * launcher.
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
#include "color565.h"
#include "display.h"
#include "input.h"
#include "menu.h"
#include "audio_pwm.h"
#include "settings.h"
#include "ard_video.h"
#include "arduboy.h"

#if !PBC_MULTI
#error "PBC Arduboy exists only as part of the combined firmware (pbcemu/build.sh)"
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

#define FRAME_US 16667u                 /* 60 frames a second */
#define FRAME_CYCLES (ARD_HZ / 60)

static FATFS g_fs;
static char game[PBC_LAUNCH_NAME_MAX];
static char save_path[PBC_LAUNCH_NAME_MAX + 8];

/* ---- clocks (as in zx) ------------------------------------------------------ */

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

/* ---- the EEPROM as <game>.sav ------------------------------------------------- */

static uint64_t save_due;               /* write after a second without changes */

static void save_load(void)
{
    static FIL f;
    UINT got = 0;
    if (f_open(&f, save_path, FA_READ) == FR_OK) {
        f_read(&f, ard_eeprom(), ARD_EEPROM, &got);
        f_close(&f);
    }
}

static void save_write(void)
{
    static FIL f;
    UINT put = 0;
    if (f_open(&f, save_path, FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {
        f_write(&f, ard_eeprom(), ARD_EEPROM, &put);
        f_close(&f);
    }
    if (put != ARD_EEPROM)
        ard_video_set_status("SAVE FAILED: DRIVE FULL?");
}

static void save_poll(void)
{
    uint64_t now = time_us_64();
    if (ard_eeprom_dirty())
        save_due = now + 1000000;
    if (save_due && now >= save_due) {
        save_due = 0;
        save_write();
    }
}

/* ---- the game ------------------------------------------------------------------ */

static void __attribute__((noreturn)) emulation_loop(bool show_stats)
{
    static int16_t samples[1024];
    uint64_t next_frame = time_us_64();
    uint64_t stat_t0 = next_frame, stat_busy_us = 0;
    uint32_t stat_drawn = 0, stat_emulated = 0;
    uint32_t sample_acc = 0;

    audio_pwm_set_volume(g_settings.sound ? g_settings.volume : 0);

#ifdef ARD_DEVPROF
    extern uint32_t ard_prof_cyc[2];
    *(volatile uint32_t *)0xE000EDFCu |= 1u << 24;     /* DEMCR.TRCENA */
    *(volatile uint32_t *)0xE0001000u |= 1u;           /* DWT_CTRL.CYCCNTENA */
    uint32_t prof_frame = 0;
#endif
    for (;;) {
#ifdef ARD_DEVPROF                      /* the host harness's script: A at 300, B at 500 */
        prof_frame++;
        ard_buttons(prof_frame >= 300 && prof_frame < 306 ? ARD_A :
                    prof_frame >= 500 && prof_frame < 506 ? ARD_B : 0);
#else
        ard_buttons((uint8_t)(input_raw() & 0x3F));     /* IN_* = ARD_* */
#endif

        uint64_t frame_start = time_us_64();
        sample_acc += AUDIO_RATE;
        unsigned n = sample_acc / 60;
        sample_acc -= n * 60;
        ard_run(FRAME_CYCLES, samples, (int)n);
        audio_pwm_push(samples, (int)n);
        if (ard_video_frame(ard_screen(), ard_invert()))
            stat_drawn++;
        stat_emulated++;
        save_poll();

        uint64_t now = time_us_64();
        stat_busy_us += now - frame_start;
        next_frame += FRAME_US;
        if (now < next_frame)
            sleep_until(from_us_since_boot(next_frame));
        else if (now > next_frame + 4 * FRAME_US)
            next_frame = now;           /* hopelessly behind: stop catching up */

        now = time_us_64();
        if (now - stat_t0 >= 1000000) {
            char s[32];
            unsigned long cpu = (unsigned long)(stat_busy_us * 100 / (now - stat_t0));
            snprintf(s, sizeof s, "%lufps %lu%% CPU%lu%%", (unsigned long)stat_drawn,
                     (unsigned long)(stat_emulated * 100 / 60), cpu);
#ifdef ARD_DEVPROF                      /* tempo | exec % << 8 | audio % << 16 */
            {
                uint32_t us = (uint32_t)(now - stat_t0), hz = CPU_MHZ;
                uint32_t ex = (uint32_t)((uint64_t)ard_prof_cyc[0] * 100 / ((uint64_t)us * hz));
                uint32_t au = (uint32_t)((uint64_t)ard_prof_cyc[1] * 1000 / ((uint64_t)us * hz));
                ard_prof_cyc[0] = ard_prof_cyc[1] = 0;
                pbc_test_stats(stat_drawn, stat_emulated * 100 / 60 | ex << 8 | au << 16, cpu);
            }
#else
            pbc_test_stats(stat_drawn, stat_emulated * 100 / 60, cpu);   /* test builds */
#endif
            if (show_stats)
                ard_video_set_status(s);
            stat_t0 = now;
            stat_drawn = stat_emulated = 0;
            stat_busy_us = 0;
        }
    }
}

/* ---- crash screen ----------------------------------------------------------
 *
 * The faulting PC and LR go into arm-none-eabi-addr2line -e build-multi/pbc_arduboy.elf. */
void __attribute__((used, noreturn)) hardfault_report(uint32_t *frame, uint32_t exc_return)
{
    (void)exc_return;
#ifdef ARD_DEVPROF                      /* device test: where it crashed, for picotool */
    if (watchdog_hw->scratch[3] == PBC_TEST_MAGIC) {
        watchdog_hw->scratch[5] = 0xDEAD;
        watchdog_hw->scratch[6] = frame[6];
        watchdog_hw->scratch[7] = frame[5];
    }
#endif
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
    menu_message("Arduboy", what, detail, "Press a button");
    menu_wait_release();
    while (!input_raw())
        sleep_ms(20);
    reboot_to_menu();
}

/* the game's HEX text into RAM, then the program out of it */
static void load_game(void)
{
    static FIL f;
    char path[PBC_LAUNCH_NAME_MAX + 2];
    snprintf(path, sizeof path, "/%s", game);
    if (f_open(&f, path, FA_READ) != FR_OK)
        fatal("Cannot open", game);
    UINT size = (UINT)f_size(&f), got = 0;
    if (size > 160 * 1024) {
        f_close(&f);
        fatal("Too big for an Arduboy:", game);
    }
    char *text = malloc(size ? size : 1);
    if (!text) {
        f_close(&f);
        fatal("Out of memory", "for loading");
    }
    f_read(&f, text, size, &got);
    f_close(&f);
    bool ok = got == size && ard_load_hex(text, size);
    free(text);
    if (!ok)
        fatal("Not an Arduboy .hex", game);
}

int main(void)
{
    input_init();
    settings_load();
    pbc_display_init();

    const pbc_launch_t *launch = pbc_launch_get();
    if (!launch || f_mount(&g_fs, "", 1) != FR_OK)
        reboot_to_menu();
    strncpy(game, launch->game, sizeof game - 1);
    load_game();
    {
        /* the translated program: as much of the free RAM as there is, less
         * some for the C library and FatFs */
        size_t size = 0;
        for (size_t s = 256 * 1024; s >= 48 * 1024; s -= 4 * 1024) {
            void *p = malloc(s);
            if (p) {
                free(p);
                size = s - 24 * 1024;
                break;
            }
        }
        void *buf = size ? malloc(size) : NULL;
        if (buf)
            ard_jit_init(buf, size);
    }
    snprintf(save_path, sizeof save_path, "/%s", game);
    char *dot = strrchr(save_path, '.');
    if (dot)
        *dot = 0;
    strcat(save_path, ".sav");
    save_load();                        /* the volume stays mounted for saving */

    overclock();
    audio_pwm_init();
    ard_video_init(pbc_launch_video());
    ard_reset();

    pbc_display_fill(0, 0, PBC_LCD_W, PBC_LCD_H, PBC_BLACK);
    multicore_launch_core1(ard_video_core1_main);
    pbc_fade_in(false);
    emulation_loop(pbc_launch_stats());
}
