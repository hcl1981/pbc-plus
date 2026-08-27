#pragma once
#include <stdint.h>

// Internal SK6805-EC14 RGB LED on GP11 (WS2812-compatible, one-wire, GRB).

void led_init(void);                              // set up the PIO driver
void led_set(uint8_t r, uint8_t g, uint8_t b);    // push one colour (no-op if unchanged)

// Non-blocking status animation. Call once per frame from core0. Reads the
// city's crime/traffic condition and blinks the LED:
//   crime  (avg > 100) -> red/blue police burst   (priority)
//   traffic(avg >  60) -> amber honk-honk echo
//   neither            -> off
void led_task(void);
