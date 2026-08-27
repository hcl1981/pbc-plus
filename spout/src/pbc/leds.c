/* leds.c -- die drei Status-LEDs.  Erste Instrumentierung eines toten Boards:
 * sie funktionieren auch dann, wenn Display-Init oder SPI die Ursache sind
 * (CLAUDE.md Abschnitt 6). */
#include "hardware/gpio.h"
#include "board.h"
#include "leds.h"

static void set(int pin, bool on) { gpio_put(pin, on); }

void pbc_leds_init(void)
{
    const int pins[3] = { PBC_PIN_LED_GREEN, PBC_PIN_LED_YELLOW, PBC_PIN_LED_RED };
    int i;
    for (i = 0; i < 3; i++) {
        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_OUT);
        gpio_put(pins[i], 0);
    }
}

void pbc_led_green(bool on)  { set(PBC_PIN_LED_GREEN, on); }
void pbc_led_yellow(bool on) { set(PBC_PIN_LED_YELLOW, on); }
void pbc_led_red(bool on)    { set(PBC_PIN_LED_RED, on); }
