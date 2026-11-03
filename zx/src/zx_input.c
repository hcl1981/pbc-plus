/* zx_input.c -- see zx_input.h */
#include <string.h>
#include "input.h"
#include "zx_input.h"
#include "zx.h"

#define HOLD_FRAMES 5                   /* a key from the menu: 0.1 s */
#define N 0xff

int zx_joy_mode = ZX_JOY_KEMPSTON;
const char *const zx_joy_names[ZX_JOYS] = { "Kempston", "Sinclair", "Cursor", "QAOP" };

#define K(row, bit) ZX_KEY(row, bit)
const zx_menu_key_t zx_keys_menu[] = {
    { "ENTER", ZXK_ENTER, N }, { "SPACE", ZXK_SPACE, N },
    { "BREAK", ZXK_CAPS, ZXK_SPACE }, { "DELETE", ZXK_CAPS, K(4, 0) },
    { "CAPS SHIFT", ZXK_CAPS, N }, { "SYMBOL SHIFT", ZXK_SYM, N },
    { "0", K(4, 0), N }, { "1", K(3, 0), N }, { "2", K(3, 1), N }, { "3", K(3, 2), N },
    { "4", K(3, 3), N }, { "5", K(3, 4), N }, { "6", K(4, 4), N }, { "7", K(4, 3), N },
    { "8", K(4, 2), N }, { "9", K(4, 1), N },
    { "A", K(1, 0), N }, { "B", K(7, 4), N }, { "C", K(0, 3), N }, { "D", K(1, 2), N },
    { "E", K(2, 2), N }, { "F", K(1, 3), N }, { "G", K(1, 4), N }, { "H", K(6, 4), N },
    { "I", K(5, 2), N }, { "J", K(6, 3), N }, { "K", K(6, 2), N }, { "L", K(6, 1), N },
    { "M", K(7, 2), N }, { "N", K(7, 3), N }, { "O", K(5, 1), N }, { "P", K(5, 0), N },
    { "Q", K(2, 0), N }, { "R", K(2, 3), N }, { "S", K(1, 1), N }, { "T", K(2, 4), N },
    { "U", K(5, 3), N }, { "V", K(0, 4), N }, { "W", K(2, 1), N }, { "X", K(0, 2), N },
    { "Y", K(5, 4), N }, { "Z", K(0, 1), N },
};
const int zx_key_count = sizeof zx_keys_menu / sizeof zx_keys_menu[0];

static int  held_key, held_frames;
static bool menu_held;

uint16_t zx_input_word(void)
{
    uint32_t r = input_raw() & 0x7F;
    uint16_t w = (uint16_t)r;
    if (held_frames > 0) {
        w |= (uint16_t)((held_key + 1) << ZX_WORD_KEY_SHIFT);
        held_frames--;
    }
    bool m = (r & IN_CENTER) && (r & IN_A);
    if (m && !menu_held)
        w |= ZX_WORD_MENU;
    menu_held = m;
    return w;
}

void zx_input_press(int index)
{
    if (index < 0 || index >= zx_key_count)
        return;
    held_key = index;
    held_frames = HOLD_FRAMES;
}

/* the joystick as keys: up, down, left, right, fire */
static const uint8_t joy_keys[ZX_JOYS][5] = {
    [ZX_JOY_SINCLAIR] = { K(4, 1), K(4, 2), K(4, 4), K(4, 3), K(4, 0) },   /* 9 8 6 7 0 */
    [ZX_JOY_CURSOR]   = { K(4, 3), K(4, 4), K(3, 4), K(4, 2), K(4, 0) },   /* 7 6 5 8 0 */
    [ZX_JOY_QAOP]     = { K(2, 0), K(1, 0), K(5, 1), K(5, 0), ZXK_SPACE }, /* Q A O P SPACE */
};

void zx_input_apply(uint16_t w)
{
    memset(zx_keys, 0, sizeof zx_keys);
    bool fire = !(w & IN_CENTER) && (w & IN_B);
    bool dir[4] = { (w & IN_UP) != 0, (w & IN_DOWN) != 0, (w & IN_LEFT) != 0, (w & IN_RIGHT) != 0 };
    uint8_t kj = 0;
    if (zx_joy_mode == ZX_JOY_KEMPSTON) {
        if (dir[3]) kj |= 1;
        if (dir[2]) kj |= 2;
        if (dir[1]) kj |= 4;
        if (dir[0]) kj |= 8;
        if (fire)   kj |= 16;
    } else {
        for (int i = 0; i < 4; i++)
            if (dir[i]) zx_key_set(joy_keys[zx_joy_mode][i]);
        if (fire) zx_key_set(joy_keys[zx_joy_mode][4]);
    }
    zx_kempston = kj;
    if (w & IN_CENTER) {
        if (w & IN_B) zx_key_set(ZXK_ENTER);
    } else if (w & IN_A) {
        zx_key_set(ZXK_SPACE);
    }
    int k = (w >> ZX_WORD_KEY_SHIFT) & ZX_WORD_KEY_MASK;
    if (k > 0 && k <= zx_key_count) {
        zx_key_set(zx_keys_menu[k - 1].k1);
        if (zx_keys_menu[k - 1].k2 != N)
            zx_key_set(zx_keys_menu[k - 1].k2);
    }
}
