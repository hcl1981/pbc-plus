/* input.h -- joystick + A/B mapped onto a PC Engine pad.
 *
 *   Joystick      -> D-pad
 *   A             -> I
 *   B             -> II
 *   CENTER + B    -> RUN
 *   CENTER + A    -> SELECT
 *
 * CENTER on its own does nothing; it only turns A and B into RUN and
 * SELECT, the same as in PicoBoyGB and the NES firmware.
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

/* PC Engine pad bits as pce-go expects them (active high, pce-go.h). */
enum {
    PCE_I = 0x01, PCE_II = 0x02, PCE_SELECT = 0x04, PCE_RUN = 0x08,
    PCE_UP = 0x10, PCE_RIGHT = 0x20, PCE_DOWN = 0x40, PCE_LEFT = 0x80,
};

/* Call once per emulated frame. */
uint8_t input_poll_pce(void);

/* Menu navigation: returns the raw bits newly pressed since the last call,
 * with auto-repeat for the joystick. */
uint32_t input_menu_edges(void);

#endif /* PBC_INPUT_H */
