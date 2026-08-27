#ifndef PBC_HW_RGB_LED_H
#define PBC_HW_RGB_LED_H

#include <stdint.h>
#include <stdbool.h>

// Idempotent. Claims an unused PIO state machine, loads the WS2812
// program (compatible with SK6805-EC14 timing), and primes the LED
// off. Returns false if no PIO/SM is available.
bool rgb_led_init(void);

// Set the RGB LED to (r, g, b), each 0..255. Driver internally
// reorders to GRB for SK6805 transmission. Non-blocking — the call
// pushes one 24-bit word to the PIO TX FIFO.
void rgb_led_set(uint8_t r, uint8_t g, uint8_t b);

// Convenience: equivalent to rgb_led_set(0, 0, 0).
void rgb_led_off(void);

#endif // PBC_HW_RGB_LED_H
