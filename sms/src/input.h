/* input.h -- joystick + A/B mapped onto a Master System / Game Gear pad.
 *
 *   Joystick      -> D-pad
 *   B             -> button 1
 *   A             -> button 2
 *   CENTER + B    -> START (Game Gear) / PAUSE (Master System)
 *
 * CENTER on its own does nothing; it only turns B into START/PAUSE.
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

/* Call once per emulated frame. Fills the pad bits (INPUT_UP ... BUTTON1 of
 * SMS Plus) and reports whether START/PAUSE is held. */
int input_poll_sms(bool *start);

/* Menu navigation: returns the raw bits newly pressed since the last call,
 * with auto-repeat for the joystick. */
uint32_t input_menu_edges(void);

#endif /* PBC_INPUT_H */
