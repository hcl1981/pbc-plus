/* battery.h -- supply voltage of the PicoBoy Color Plus (GP29 = ADC 3,
 * behind a 1:3 divider; it reads the batteries, or about 5 V when the
 * BAT/USB switch is on USB). */
#ifndef PBC_BATTERY_H
#define PBC_BATTERY_H

#include <stdbool.h>

void battery_init(void);
/* Call once per frame; measures four times a second. */
void battery_poll(void);
bool battery_on_usb(void);
/* 0..4 bars, for two AAA cells */
int  battery_bars(void);

#endif
