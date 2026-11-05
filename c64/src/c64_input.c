/* c64_input.c -- see c64_input.h */
#include <string.h>
#include "pico/stdlib.h"
#include "input.h"
#include "c64_input.h"
#include "settings.h"
#include "pbc_link.h"

/* C64 keyboard matrix positions as Frodo uses them: key_matrix[row] bit col
 * (and the transposed rev_matrix[col] bit row), active low. */
#define K_RETURN   0, 1

#define C64_KEY_POS(name, row, col) { row, col },
static const uint8_t key_pos[C64_KEY_COUNT][2] = { C64_KEYS(C64_KEY_POS) };
static int key;                         /* on Center + B */
static bool disk_held;
static volatile bool disk_change;
static bool disk_mine = true;

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

int c64_input_key(void)
{
    return key;
}

bool c64_input_disk_change(void)
{
    bool d = disk_change;
    disk_change = false;
    return d;
}

bool c64_input_disk_mine(void)
{
    return disk_mine;
}

static void press(uint8_t *km, uint8_t *rm, int row, int col)
{
    km[row] &= (uint8_t)~(1u << col);
    rm[col] &= (uint8_t)~(1u << row);
}

/* What a player's buttons do: the joystick comes back, the keys go into the
 * matrix. `word`: bits IN_*, above them the player's key on Center + B. */
#define WORD_KEY_SHIFT 8
#define WORD_KEY_MASK  0x7F
#define WORD_DISK      0x8000           /* Center + A just pressed */

static uint8_t player(uint16_t word, uint8_t *km, uint8_t *rm)
{
    uint8_t j = 0xFF;
    int k = (word >> WORD_KEY_SHIFT) & WORD_KEY_MASK;

    if (word & IN_UP)    j &= ~J_UP;
    if (word & IN_DOWN)  j &= ~J_DOWN;
    if (word & IN_LEFT)  j &= ~J_LEFT;
    if (word & IN_RIGHT) j &= ~J_RIGHT;
    if (word & IN_CENTER) {
        if ((word & IN_B) && k < C64_KEY_COUNT)
            press(km, rm, key_pos[k][0], key_pos[k][1]);
    } else {
        if (word & IN_B) j &= ~J_FIRE;
        if (word & IN_A) press(km, rm, K_RETURN);
    }
    return j;
}

static uint16_t local_word(void)
{
    return (uint16_t)((input_raw() & 0x7F) | key << WORD_KEY_SHIFT);
}

static uint16_t link_word[2];

void c64_input_frame(const uint8_t *ram)
{
    if (!pbc_link_active())
        return;
    uint16_t w = local_word();
    bool d = (w & IN_CENTER) && (w & IN_A);
    if (d && !disk_held)
        w |= WORD_DISK;
    disk_held = d;
    pbc_link_frame(w, ram, 0x10000, &link_word[0], &link_word[1]);
    /* Both devices see the same two words: they stop after the same frame.
     * Both at once: player 1 chooses. */
    if ((link_word[0] | link_word[1]) & WORD_DISK) {
        bool first = (link_word[0] & WORD_DISK) != 0;
        disk_mine = first == (pbc_link_player() == 1);
        disk_change = true;
    }
}

void input_rp2350_poll(uint8_t *km, uint8_t *rm, uint8_t *joystick)
{
    uint8_t j, other = 0xFF;

    memset(km, 0xFF, 8);
    memset(rm, 0xFF, 8);

    if (pbc_link_active()) {
        /* player 1 on the game's port, player 2 on the other */
        j = player(link_word[0], km, rm);
        other = player(link_word[1], km, rm);
    } else {
        uint16_t w = local_word();
        j = player(w, km, rm);
        bool d = (w & IN_CENTER) && (w & IN_A);
        if (d && !disk_held)
            disk_change = true;
        disk_held = d;
    }

    /* Port 1 is CIA1 port B ($DC01), port 2 is port A ($DC00). */
    *joystick = (port == 1) ? j : other;
    joy2      = (port == 2) ? j : other;
}

uint8_t input_get_joystick2(void)
{
    return joy2;
}
