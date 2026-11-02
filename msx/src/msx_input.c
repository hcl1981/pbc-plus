/* msx_input.c -- see msx_input.h */
#include <string.h>
#include "input.h"
#include "msx_input.h"
#include "msx1.h"

#define HOLD_FRAMES 6                   /* a key from the menu: 0.1 s */

#define K(row, bit) MSX_KEY(row, bit)
const msx_key_t msx_keys_menu[] = {
    { "RETURN", MSXK_RETURN }, { "ESC", MSXK_ESC },     { "SPACE", MSXK_SPACE },
    { "F1", MSXK_F1 },         { "F2", MSXK_F2 },       { "F3", MSXK_F3 },
    { "F4", MSXK_F4 },         { "F5", MSXK_F5 },       { "STOP", MSXK_STOP },
    { "SELECT", MSXK_SELECT }, { "HOME", MSXK_HOME },   { "INS", MSXK_INS },
    { "DEL", MSXK_DEL },       { "BS", MSXK_BS },       { "TAB", MSXK_TAB },
    { "SHIFT", MSXK_SHIFT },   { "CTRL", MSXK_CTRL },   { "GRAPH", MSXK_GRAPH },
    { "0", K(0, 0) }, { "1", K(0, 1) }, { "2", K(0, 2) }, { "3", K(0, 3) }, { "4", K(0, 4) },
    { "5", K(0, 5) }, { "6", K(0, 6) }, { "7", K(0, 7) }, { "8", K(1, 0) }, { "9", K(1, 1) },
    { "A", K(2, 6) }, { "B", K(2, 7) }, { "C", K(3, 0) }, { "D", K(3, 1) }, { "E", K(3, 2) },
    { "F", K(3, 3) }, { "G", K(3, 4) }, { "H", K(3, 5) }, { "I", K(3, 6) }, { "J", K(3, 7) },
    { "K", K(4, 0) }, { "L", K(4, 1) }, { "M", K(4, 2) }, { "N", K(4, 3) }, { "O", K(4, 4) },
    { "P", K(4, 5) }, { "Q", K(4, 6) }, { "R", K(4, 7) }, { "S", K(5, 0) }, { "T", K(5, 1) },
    { "U", K(5, 2) }, { "V", K(5, 3) }, { "W", K(5, 4) }, { "X", K(5, 5) }, { "Y", K(5, 6) },
    { "Z", K(5, 7) },
};
const int msx_key_count = sizeof msx_keys_menu / sizeof msx_keys_menu[0];

static int  held_key, held_frames;      /* from the menu */
static bool menu_held;

uint16_t msx_input_word(void)
{
    uint32_t r = input_raw() & 0x7F;
    uint16_t w = (uint16_t)r;
    if (held_frames > 0) {
        w |= (uint16_t)((held_key + 1) << MSX_WORD_KEY_SHIFT);
        held_frames--;
    }
    bool m = (r & IN_CENTER) && (r & IN_A);
    if (m && !menu_held)
        w |= MSX_WORD_MENU;
    menu_held = m;
    return w;
}

void msx_input_press(int index)
{
    if (index < 0 || index >= msx_key_count)
        return;
    held_key = index;
    held_frames = HOLD_FRAMES;
}

/* One player: the joystick bits come back, keys go into the matrix. */
static uint8_t player(uint16_t w, bool keyboard)
{
    uint8_t j = 0;
    if (w & IN_UP)    { j |= MSX_JOY_UP;    if (keyboard) msx_key_set(MSXK_UP); }
    if (w & IN_DOWN)  { j |= MSX_JOY_DOWN;  if (keyboard) msx_key_set(MSXK_DOWN); }
    if (w & IN_LEFT)  { j |= MSX_JOY_LEFT;  if (keyboard) msx_key_set(MSXK_LEFT); }
    if (w & IN_RIGHT) { j |= MSX_JOY_RIGHT; if (keyboard) msx_key_set(MSXK_RIGHT); }
    if (w & IN_CENTER) {
        if ((w & IN_B) && keyboard)
            msx_key_set(MSXK_F1);
    } else {
        if (w & IN_B) { j |= MSX_JOY_A; if (keyboard) msx_key_set(MSXK_SPACE); }
        if (w & IN_A) { j |= MSX_JOY_B; if (keyboard) msx_key_set(MSX_KEY(4, 2)); }   /* M */
    }
    int k = (w >> MSX_WORD_KEY_SHIFT) & MSX_WORD_KEY_MASK;
    if (k > 0 && k <= msx_key_count)
        msx_key_set(msx_keys_menu[k - 1].key);
    return j;
}

void msx_input_apply(uint16_t p1, uint16_t p2)
{
    memset(msx_keys, 0, sizeof msx_keys);
    msx_joy[0] = player(p1, true);
    msx_joy[1] = player(p2, false);
}
