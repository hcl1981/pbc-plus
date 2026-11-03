/* cpc_input.c -- see cpc_input.h */
#include <string.h>
#include "input.h"
#include "cpc_input.h"
#include "cpc.h"

#define HOLD_FRAMES 5

int cpc_joy_mode = CPC_JOY_JOYSTICK;
const char *const cpc_joy_names[CPC_JOYS] = { "Joystick", "Cursor keys", "QAOP" };

#define K(row, bit) CPC_KEY(row, bit)
const cpc_menu_key_t cpc_keys_menu[] = {
    { "RETURN", CPCK_RETURN }, { "SPACE", CPCK_SPACE }, { "ESC", CPCK_ESC },
    { "COPY", CPCK_COPY }, { "DEL", K(9, 7) }, { "SHIFT", CPCK_SHIFT }, { "CTRL", K(2, 7) },
    { "TAB", K(8, 4) }, { "CAPS LOCK", K(8, 6) },
    { "F0", K(1, 7) }, { "F1", K(1, 5) }, { "F2", K(1, 6) }, { "F3", K(0, 5) }, { "F4", K(2, 4) },
    { "F5", K(1, 4) }, { "F6", K(0, 4) }, { "F7", K(1, 2) }, { "F8", K(1, 3) }, { "F9", K(0, 3) },
    { "0", K(4, 0) }, { "1", K(8, 0) }, { "2", K(8, 1) }, { "3", K(7, 1) }, { "4", K(7, 0) },
    { "5", K(6, 1) }, { "6", K(6, 0) }, { "7", K(5, 1) }, { "8", K(5, 0) }, { "9", K(4, 1) },
    { "A", K(8, 5) }, { "B", K(6, 6) }, { "C", K(7, 6) }, { "D", K(7, 5) }, { "E", K(7, 2) },
    { "F", K(6, 5) }, { "G", K(6, 4) }, { "H", K(5, 4) }, { "I", K(4, 3) }, { "J", K(5, 5) },
    { "K", K(4, 5) }, { "L", K(4, 4) }, { "M", K(4, 6) }, { "N", K(5, 6) }, { "O", K(4, 2) },
    { "P", K(3, 3) }, { "Q", K(8, 3) }, { "R", K(6, 2) }, { "S", K(7, 4) }, { "T", K(6, 3) },
    { "U", K(5, 2) }, { "V", K(6, 7) }, { "W", K(7, 3) }, { "X", K(7, 7) }, { "Y", K(5, 3) },
    { "Z", K(8, 7) },
};
const int cpc_key_count = sizeof cpc_keys_menu / sizeof cpc_keys_menu[0];

static int  held_key, held_frames;
static bool menu_held;

uint16_t cpc_input_word(void)
{
    uint32_t r = input_raw() & 0x7F;
    uint16_t w = (uint16_t)r;
    if (held_frames > 0) {
        w |= (uint16_t)((held_key + 1) << CPC_WORD_KEY_SHIFT);
        held_frames--;
    }
    bool m = (r & IN_CENTER) && (r & IN_A);
    if (m && !menu_held)
        w |= CPC_WORD_MENU;
    menu_held = m;
    return w;
}

void cpc_input_press(int index)
{
    if (index < 0 || index >= cpc_key_count)
        return;
    held_key = index;
    held_frames = HOLD_FRAMES;
}

/* up, down, left, right, fire */
static const uint8_t joy_keys[CPC_JOYS][5] = {
    [CPC_JOY_JOYSTICK] = { CPCK_JOY_UP, CPCK_JOY_DOWN, CPCK_JOY_LEFT, CPCK_JOY_RIGHT, CPCK_JOY_FIRE1 },
    [CPC_JOY_CURSOR]   = { CPCK_UP, CPCK_DOWN, CPCK_LEFT, CPCK_RIGHT, CPCK_COPY },
    [CPC_JOY_QAOP]     = { K(8, 3), K(8, 5), K(4, 2), K(3, 3), CPCK_SPACE },
};

void cpc_input_apply(uint16_t w)
{
    /* the keys typed by the core (autoload) stay; cpc_frame adds them */
    memset(cpc_keys, 0, sizeof cpc_keys);
    const uint8_t *j = joy_keys[cpc_joy_mode];
    if (w & IN_UP)    cpc_key_set(j[0]);
    if (w & IN_DOWN)  cpc_key_set(j[1]);
    if (w & IN_LEFT)  cpc_key_set(j[2]);
    if (w & IN_RIGHT) cpc_key_set(j[3]);
    if (w & IN_CENTER) {
        if (w & IN_B) cpc_key_set(CPCK_RETURN);
    } else {
        if (w & IN_B) cpc_key_set(j[4]);
        if (w & IN_A) cpc_key_set(CPCK_SPACE);
    }
    int k = (w >> CPC_WORD_KEY_SHIFT) & CPC_WORD_KEY_MASK;
    if (k > 0 && k <= cpc_key_count)
        cpc_key_set(cpc_keys_menu[k - 1].key);
}
