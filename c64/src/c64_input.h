/* c64_input.h -- PicoBoy buttons -> C64 joystick and a few keys.
 *
 *   Joystick         -> joystick (on the port chosen per game)
 *   B                -> fire
 *   A                -> RETURN  (VVVVVV and many others use it as action key)
 *   CENTER + B       -> a key chosen per game (settings.h, SPACE by default)
 *   CENTER + A       -> disk menu (main.c asks c64_input_disk_change())
 */
#ifndef PBC_C64_INPUT_H
#define PBC_C64_INPUT_H

#include <stdint.h>
#include <stdbool.h>

/* 1 or 2. Most single-player games read port 2. */
void c64_input_set_port(int port);

/* The key on Center + B: index into C64_KEYS (settings.h). */
void c64_input_set_key(int key);

/* True once per press of Center + A. */
bool c64_input_disk_change(void);

/* Called by the Frodo glue (Display::PollKeyboard, c64_run_frame). */
void    input_rp2350_poll(uint8_t *key_matrix, uint8_t *rev_matrix, uint8_t *joystick);
uint8_t input_get_joystick2(void);

#endif /* PBC_C64_INPUT_H */
