/* pbc_rgb.h -- the RGB LED in the "+" of the PBC+ lettering.
 *
 * One WS2812 (colour order GRB) on GPIO 11, driven by a PIO state machine.
 * It keeps its colour until told otherwise, also across a reset of the
 * chip: the launcher switches it off when it starts.
 */
#ifndef PBC_RGB_H
#define PBC_RGB_H

#include <stdint.h>

#define PBC_PIN_RGB 11

#ifdef __cplusplus
extern "C" {
#endif

/* Sets the colour (0..255 each). The first call sets up the PIO with the
 * system clock of that moment: call it after any change of the clock. */
void pbc_rgb_set(uint8_t r, uint8_t g, uint8_t b);

#ifdef __cplusplus
}
#endif

#endif
