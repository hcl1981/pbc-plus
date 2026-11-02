/* msx_input.h -- PicoBoy buttons -> MSX joysticks and keyboard.
 *
 *   Joystick      -> joystick 1 and the cursor keys
 *   B             -> trigger A and SPACE
 *   A             -> trigger B and M
 *   CENTER + B    -> F1
 *   CENTER + A    -> menu: any other key (main.c)
 *
 * A player's state for one frame is one 16-bit word (link play swaps these
 * with the partner): bits 0..6 the buttons (IN_*), 8..14 a key chosen in the
 * menu (index into msx_keys_menu + 1, 0: none), 15 "Center + A just
 * pressed". In link play player 1 has joystick 1 and the keyboard, player 2
 * joystick 2.
 */
#ifndef PBC_MSX_INPUT_H
#define PBC_MSX_INPUT_H

#include <stdint.h>
#include <stdbool.h>

#define MSX_WORD_KEY_SHIFT 8
#define MSX_WORD_KEY_MASK  0x7F
#define MSX_WORD_MENU      0x8000

/* Keys the menu offers: name and matrix position (msx1.h, MSX_KEY) */
typedef struct { const char *name; uint8_t key; } msx_key_t;
extern const msx_key_t msx_keys_menu[];
extern const int msx_key_count;

/* This device's word for the next frame: the buttons, a key from the menu
 * while it is being held, and MSX_WORD_MENU on the press of Center + A. */
uint16_t msx_input_word(void);

/* The menu chose key `index` (into msx_keys_menu): held for a few frames. */
void msx_input_press(int index);

/* Before each frame: player 1's and player 2's words (p2 = 0 alone) into
 * the keyboard matrix and the joysticks (msx_keys, msx_joy). */
void msx_input_apply(uint16_t p1, uint16_t p2);

#endif /* PBC_MSX_INPUT_H */
