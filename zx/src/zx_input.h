/* zx_input.h -- PicoBoy buttons -> Spectrum joystick and keyboard.
 *
 *   Joystick      -> the joystick, as set in the menu: Kempston (port 1Fh),
 *                    Sinclair (keys 6-0), Cursor (5-8, 0) or QAOP (fire SPACE)
 *   B             -> fire
 *   A             -> SPACE
 *   CENTER + B    -> ENTER
 *   CENTER + A    -> menu: joystick, machine, tape, any key (main.c)
 *
 * A frame's state is one 16-bit word: bits 0..6 the buttons (IN_*), 8..14
 * a key chosen in the menu (index into zx_keys_menu + 1, 0: none), 15
 * "Center + A just pressed".
 */
#ifndef PBC_ZX_INPUT_H
#define PBC_ZX_INPUT_H

#include <stdint.h>
#include <stdbool.h>

#define ZX_WORD_KEY_SHIFT 8
#define ZX_WORD_KEY_MASK  0x7F
#define ZX_WORD_MENU      0x8000

enum { ZX_JOY_KEMPSTON, ZX_JOY_SINCLAIR, ZX_JOY_CURSOR, ZX_JOY_QAOP, ZX_JOYS };
extern int zx_joy_mode;
extern const char *const zx_joy_names[ZX_JOYS];

/* Keys the menu offers: name, one or two keys (0xff: none) */
typedef struct { const char *name; uint8_t k1, k2; } zx_menu_key_t;
extern const zx_menu_key_t zx_keys_menu[];
extern const int zx_key_count;

uint16_t zx_input_word(void);
void zx_input_press(int index);
/* the word into zx_keys and zx_kempston (before each frame) */
void zx_input_apply(uint16_t w);

#endif /* PBC_ZX_INPUT_H */
