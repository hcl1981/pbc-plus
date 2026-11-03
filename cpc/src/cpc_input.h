/* cpc_input.h -- PicoBoy buttons -> CPC joystick and keyboard.
 *
 *   Joystick      -> as set in the menu: the joystick (port 1), the cursor
 *                    keys (fire COPY) or QAOP (fire SPACE)
 *   B             -> fire
 *   A             -> SPACE
 *   CENTER + B    -> RETURN
 *   CENTER + A    -> menu: joystick, restart, tape, any key (main.c)
 *
 * A frame's state is one 16-bit word: bits 0..6 the buttons (IN_*), 8..14
 * a key chosen in the menu (index into cpc_keys_menu + 1), 15 "Center + A
 * just pressed".
 */
#ifndef PBC_CPC_INPUT_H
#define PBC_CPC_INPUT_H

#include <stdint.h>
#include <stdbool.h>

#define CPC_WORD_KEY_SHIFT 8
#define CPC_WORD_KEY_MASK  0x7F
#define CPC_WORD_MENU      0x8000

enum { CPC_JOY_JOYSTICK, CPC_JOY_CURSOR, CPC_JOY_QAOP, CPC_JOYS };
extern int cpc_joy_mode;
extern const char *const cpc_joy_names[CPC_JOYS];

typedef struct { const char *name; uint8_t key; } cpc_menu_key_t;
extern const cpc_menu_key_t cpc_keys_menu[];
extern const int cpc_key_count;

uint16_t cpc_input_word(void);
void cpc_input_press(int index);
void cpc_input_apply(uint16_t w);

#endif /* PBC_CPC_INPUT_H */
