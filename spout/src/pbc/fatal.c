/* fatal.c -- das Panel ist die Konsole (CLAUDE.md Abschnitt 6).
 *
 * Es gibt hier keine serielle Ausgabe: UART0 liegt per Default auf GP0/GP1 =
 * Joystick CENTER und RIGHT, und USB-stdio ist aus.  Deshalb landen panic(),
 * hard_assertion_failure() und der HardFault auf dem Bildschirm, zusammen mit
 * der Adresse, die man in addr2line stecken kann.  Der Ersatz-panic darf
 * nicht zurueckkehren.
 */
#include <stdint.h>
#include <stdarg.h>
#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "board.h"
#include "../color565.h"
#include "display.h"
#include "../font.h"
#include "leds.h"

#define COL_BG   PBC_RGB(0x40, 0x00, 0x08) /* dunkelrot                     */
#define COL_FG   PBC_WHITE
#define COL_HL   PBC_RGB(0xFF, 0xA0, 0x20) /* orange                        */

static void hex32(char *dst, uint32_t v)
{
    static const char d[] = "0123456789ABCDEF";
    int i;
    for (i = 0; i < 8; i++)
        dst[i] = d[(v >> (28 - 4 * i)) & 0xf];
    dst[8] = 0;
}

static void fatal_screen(const char *what, const char *detail, uint32_t addr)
{
    char buf[16];

    pbc_led_red(true);
    pbc_led_green(false);
    pbc_led_yellow(false);

    pbc_display_backlight(200);
    pbc_display_fill(0, 0, PBC_LCD_W, PBC_LCD_H, COL_BG);
    pbc_display_text(20,  24, &font_ui_l, "SPOUT", COL_HL, COL_BG);
    pbc_display_text(20,  64, &font_ui_m, what, COL_FG, COL_BG);
    if (detail && detail[0])
        pbc_display_text(20,  94, &font_ui_s, detail, COL_FG, COL_BG);
    pbc_display_text(20, 120, &font_ui_s, "ADDR", COL_HL, COL_BG);
    hex32(buf, addr);
    pbc_display_text(20, 138, &font_ui_l, buf, COL_FG, COL_BG);
    pbc_display_text(20, 190, &font_ui_s, "ARM-NONE-EABI-ADDR2LINE -E", COL_FG, COL_BG);
    pbc_display_text(20, 206, &font_ui_s, "SPOUT.ELF -F -C <ADDR>", COL_FG, COL_BG);
    pbc_display_text(20, 240, &font_ui_s, "BOOTSEL HALTEN UND USB STECKEN", COL_HL, COL_BG);

    for (;;) {
        /* Rote LED blinken lassen, damit ein eingefrorenes Geraet auch dann
         * erkennbar ist, wenn der Fehler vor dem Display-Init lag. */
        pbc_led_red(true);  sleep_ms(120);
        pbc_led_red(false); sleep_ms(120);
    }
}

void __wrap_panic(const char *fmt, ...);
void __wrap_panic(const char *fmt, ...)
{
    fatal_screen("PANIC", fmt, (uint32_t)(uintptr_t)__builtin_return_address(0));
}

void __wrap_hard_assertion_failure(void);
void __wrap_hard_assertion_failure(void)
{
    fatal_screen("ASSERT", "HARD ASSERTION FAILED",
                 (uint32_t)(uintptr_t)__builtin_return_address(0));
}

/* HardFault: der gestapelte PC steht an Offset 6 des Ausnahmerahmens. */
void pbc_hardfault_report(uint32_t *sp);
void pbc_hardfault_report(uint32_t *sp)
{
    fatal_screen("HARD FAULT", "PC AUS DEM AUSNAHMERAHMEN", sp[6]);
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
