/* main.c -- Rushtown on the PicoBoy Color Plus.
 *
 * 125 MHz, because the panel's 62.5 MHz are then exactly half the system
 * clock. Two whole frames in RAM: one is drawn while DMA sends the other,
 * which takes 17 ms, so the game runs its 50 ticks a second and shows
 * every one of them.
 *
 * There is no serial console (UART0's pins are the joystick): the LEDs say
 * how far the start got -- green: the display is up, yellow stays on: the
 * sound did not start -- and a crash writes itself on the panel.
 *
 * Held while switching on: B starts without sound.
 */
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "board.h"
#include "pbc.h"
#include "../rt.h"

static px_t frames[2][SCR_W * SCR_H];

static void led(int pin, bool on)
{
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
    gpio_put(pin, on);
}

static void __attribute__((noreturn)) fatal(const char *what, uint32_t addr)
{
    static const char hex[] = "0123456789ABCDEF";
    char a[9];
    for (int i = 0; i < 8; i++)
        a[i] = hex[(addr >> (28 - 4 * i)) & 15];
    a[8] = 0;
    uint16_t bg = RGB(64, 0, 8), fg = RGB(255, 255, 255);
    pbc_display_backlight(200);
    pbc_display_fill(0, 0, PBC_LCD_W, PBC_LCD_H, bg);
    pbc_display_text(16, 30, "RUSHTOWN STOPPED", RGB(255, 160, 32), bg);
    pbc_display_text(16, 60, what, fg, bg);
    pbc_display_text(16, 80, "AT", fg, bg);
    pbc_display_text(40, 80, a, fg, bg);
    pbc_display_text(16, 120, "arm-none-eabi-addr2line", fg, bg);
    pbc_display_text(16, 132, " -e rushtown.elf <AT>", fg, bg);
    for (;;) {
        led(PBC_PIN_LED_RED, true);
        busy_wait_ms(120);
        led(PBC_PIN_LED_RED, false);
        busy_wait_ms(120);
    }
}

void __wrap_panic(const char *fmt, ...);
void __wrap_panic(const char *fmt, ...)
{
    fatal(fmt ? fmt : "PANIC", (uint32_t)(uintptr_t)__builtin_return_address(0));
}

void __wrap_hard_assertion_failure(void);
void __wrap_hard_assertion_failure(void)
{
    fatal("ASSERTION", (uint32_t)(uintptr_t)__builtin_return_address(0));
}

void pbc_hardfault_report(uint32_t *sp);
void pbc_hardfault_report(uint32_t *sp)
{
    fatal("HARD FAULT", sp[6]);
}

void __attribute__((naked)) isr_hardfault(void);
void __attribute__((naked)) isr_hardfault(void)
{
    __asm volatile (
        "tst   lr, #4          \n"
        "ite   eq              \n"
        "mrseq r0, msp         \n"
        "mrsne r0, psp         \n"
        "ldr   r1, =pbc_hardfault_report \n"
        "bx    r1              \n"
    );
}

int main(void)
{
    set_sys_clock_khz(125000, true);
    /* set_sys_clock_khz left clk_peri on the USB PLL (48 MHz): back to the
     * system clock, or the panel gets a third of its speed */
    uint32_t hz = clock_get_hz(clk_sys);
    clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS, hz, hz);

    led(PBC_PIN_LED_GREEN, false);
    led(PBC_PIN_LED_YELLOW, false);
    led(PBC_PIN_LED_RED, false);
    pbc_input_init();
    bool quiet = (pbc_input() & BTN_B) != 0;
    pbc_display_init();
    led(PBC_PIN_LED_GREEN, true);

    int back = 0;
    g_fb = frames[0];
    game_init((uint32_t)time_us_64() ^ 0x9e3779b9u);
    if (quiet)
        snd_mute(true);

    /* the first picture, then the light: nothing old flashes up */
    game_draw();
    pbc_display_frame(frames[back]);
    pbc_display_wait();
    for (int i = 1; i <= 16; i++) {
        pbc_display_backlight((uint8_t)(i * 13));
        sleep_ms(12);
    }
    back ^= 1;
    g_fb = frames[back];

    led(PBC_PIN_LED_YELLOW, true);
    if (pbc_audio_init())
        led(PBC_PIN_LED_YELLOW, false);
    led(PBC_PIN_LED_GREEN, false);

    uint64_t next = time_us_64();
    for (;;) {
        /* as many ticks as time has passed, but no more than three: a
         * frame that took too long slows the game instead of piling up */
        uint64_t now = time_us_64();
        int n = 0;
        while (now >= next && n < 3) {
            game_tick(pbc_input());
            next += 1000000 / TICK_HZ;
            n++;
        }
        if (now > next + 100000)
            next = now;
        pbc_audio_fill();
        if (n) {
            game_draw();
            pbc_display_frame(frames[back]);    /* waits for the one before */
            back ^= 1;
            g_fb = frames[back];
            pbc_audio_fill();
        }
        now = time_us_64();
        if (now < next)
            sleep_us(next - now);
    }
}
