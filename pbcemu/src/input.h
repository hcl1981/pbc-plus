/* input.h -- joystick, A, B and CENTER of the PicoBoy Color Plus. */
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

/* Menu navigation: returns the raw bits newly pressed since the last call,
 * with auto-repeat for the joystick. */
uint32_t input_menu_edges(void);

#endif /* PBC_INPUT_H */
