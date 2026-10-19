/* input.h -- joystick + A/B mapped onto a Game Boy pad.
 *
 *   Joystick      -> D-pad
 *   A             -> A
 *   B             -> B
 *   CENTER + B    -> START
 *   CENTER + A    -> SELECT
 *
 * CENTER on its own does nothing; it only turns A and B into START and
 * SELECT, the same as in PicoBoyGB.
 */
#ifndef PBC_INPUT_H
#define PBC_INPUT_H

#include <stdint.h>
#include <stdbool.h>

/* Raw button bits (1 = pressed). */
enum {
    IN_UP = 1u << 0, IN_DOWN = 1u << 1, IN_LEFT = 1u << 2, IN_RIGHT = 1u << 3,
    IN_A = 1u << 4, IN_B = 1u << 5, IN_CENTER = 1u << 6,
};

void     input_init(void);
uint32_t input_raw(void);

/* Game Boy buttons in the bit order of Peanut-GB's joypad byte (which is
 * active low: the caller inverts). */
enum {
    GB_A = 0x01, GB_B = 0x02, GB_SELECT = 0x04, GB_START = 0x08,
    GB_RIGHT = 0x10, GB_LEFT = 0x20, GB_UP = 0x40, GB_DOWN = 0x80,
};

/* Call once per emulated frame. */
uint8_t input_poll_gb(void);

/* Menu navigation: returns the raw bits newly pressed since the last call,
 * with auto-repeat for the joystick. */
uint32_t input_menu_edges(void);

#endif /* PBC_INPUT_H */
