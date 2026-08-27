#ifndef PBC_LEDS_H
#define PBC_LEDS_H
#include <stdbool.h>
void pbc_leds_init(void);
void pbc_led_green(bool on);
void pbc_led_yellow(bool on);
void pbc_led_red(bool on);
#endif
