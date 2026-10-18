/* input.h -- joystick + A/B mapped onto an NES pad.
 *
 *   Joystick      -> D-pad
 *   A             -> A
 *   B             -> B
 *   CENTER + B    -> START
 *   CENTER + A    -> SELECT (held 1 s: back to the ROM list)
 *   CENTER + up/down -> volume (handled in main.c, not sent to the game)
 *
 * CENTER on its own does nothing; it only turns A and B into START and
 * SELECT, the same as in PicoBoyGB, and the joystick into a volume control.
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

/* NES joypad bits as QuickNES expects them. */
enum {
    NES_A = 0x01, NES_B = 0x02, NES_SELECT = 0x04, NES_START = 0x08,
    NES_UP = 0x10, NES_DOWN = 0x20, NES_LEFT = 0x40, NES_RIGHT = 0x80,
};

/* Call once per emulated frame. */
uint8_t input_poll_nes(void);

/* Menu navigation: returns the raw bits newly pressed since the last call,
 * with auto-repeat for the joystick. */
uint32_t input_menu_edges(void);

#endif /* PBC_INPUT_H */
