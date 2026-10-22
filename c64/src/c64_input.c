/* c64_input.c -- see c64_input.h */
#include <string.h>
#include "pico/stdlib.h"
#include "input.h"
#include "c64_input.h"
#include "settings.h"

/* C64 keyboard matrix positions as Frodo uses them: key_matrix[row] bit col
 * (and the transposed rev_matrix[col] bit row), active low. */
#define K_RETURN   0, 1

#define C64_KEY_POS(name, row, col) { row, col },
static const uint8_t key_pos[C64_KEY_COUNT][2] = { C64_KEYS(C64_KEY_POS) };
static int key;                         /* on Center + B */
static bool disk_held;
static volatile bool disk_change;

/* Joystick bits, active low. */
#define J_UP    0x01
#define J_DOWN  0x02
#define J_LEFT  0x04
#define J_RIGHT 0x08
#define J_FIRE  0x10

static int port = 2;
static uint8_t joy2 = 0xFF;

void c64_input_set_port(int p)
{
    port = (p == 1) ? 1 : 2;
}

void c64_input_set_key(int k)
{
    key = (k >= 0 && k < C64_KEY_COUNT) ? k : 0;
}

bool c64_input_disk_change(void)
{
    bool d = disk_change;
    disk_change = false;
    return d;
}

static void press(uint8_t *km, uint8_t *rm, int row, int col)
{
    km[row] &= (uint8_t)~(1u << col);
    rm[col] &= (uint8_t)~(1u << row);
}

void input_rp2350_poll(uint8_t *km, uint8_t *rm, uint8_t *joystick)
{
    uint32_t r = input_raw();
    uint8_t j = 0xFF;

    memset(km, 0xFF, 8);
    memset(rm, 0xFF, 8);

    if (r & IN_UP)    j &= ~J_UP;
    if (r & IN_DOWN)  j &= ~J_DOWN;
    if (r & IN_LEFT)  j &= ~J_LEFT;
    if (r & IN_RIGHT) j &= ~J_RIGHT;
    if (r & IN_CENTER) {
        if (r & IN_B)
            press(km, rm, key_pos[key][0], key_pos[key][1]);
        bool d = (r & IN_A) != 0;
        if (d && !disk_held)
            disk_change = true;
        disk_held = d;
    } else {
        disk_held = false;
        if (r & IN_B) j &= ~J_FIRE;
        if (r & IN_A) press(km, rm, K_RETURN);
    }

    /* Port 1 is CIA1 port B ($DC01), port 2 is port A ($DC00). */
    *joystick = (port == 1) ? j : 0xFF;
    joy2      = (port == 2) ? j : 0xFF;
}

uint8_t input_get_joystick2(void)
{
    return joy2;
}
