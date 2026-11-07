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
/* press a key once (row, column of Frodo's key matrix), from the menu */
void c64_input_tap(int row, int col);
int  c64_input_key(void);
int  c64_input_port(void);

/* True once per press of Center + A; in link play on both devices, after
 * the same frame. _mine: this device's player asked, the other one waits. */
bool c64_input_disk_change(void);
bool c64_input_disk_mine(void);

/* Before each frame: link play swaps the buttons here, with the partner. */
void c64_input_frame(const uint8_t *ram);

/* Called by the Frodo glue (Display::PollKeyboard, c64_run_frame). */
void    input_rp2350_poll(uint8_t *key_matrix, uint8_t *rev_matrix, uint8_t *joystick);
uint8_t input_get_joystick2(void);

#endif /* PBC_C64_INPUT_H */
