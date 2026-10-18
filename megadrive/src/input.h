/* input.h -- joystick + A/B mapped onto a 3-button Genesis pad.
 *
 *   Joystick      -> D-pad
 *   A             -> A
 *   B             -> B
 *   CENTER        -> C
 *   CENTER + B    -> START
 *   CENTER + A    -> SELECT (no such button on the Genesis: opens the menu)
 *
 * CENTER alone becomes C only after a short grace period, so that pressing
 * CENTER first and A/B a moment later reads as START/SELECT instead of a
 * stray C. While a combination is held, C, A and B are not reported.
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

/* Genesis pad state in gwenesis' active-low layout:
 * bit0 up, 1 down, 2 left, 3 right, 4 B, 5 C, 6 A, 7 START. */
typedef struct {
    uint8_t pad;
    bool    select;      /* CENTER + A held */
} genesis_input_t;

/* Call once per emulated frame. */
genesis_input_t input_poll_genesis(void);

/* Menu navigation: returns the raw bits newly pressed since the last call,
 * with auto-repeat for up/down. */
uint32_t input_menu_edges(void);

#endif /* PBC_INPUT_H */
