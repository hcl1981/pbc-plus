/* input.c -- sieben Tasten, alle aktiv LOW mit Pullup.
 *
 * Entprellung: ein Zustandswechsel wird erst uebernommen, wenn er zweimal
 * hintereinander gelesen wurde.  Bei 50 Hz Abtastung sind das 20 ms; das
 * Prellen dieser Taster liegt darunter.
 */
#include "hardware/gpio.h"
#include "board.h"
#include "input.h"
#include "../game.h"

static const uint8_t pins[7] = {
    PBC_PIN_RIGHT, PBC_PIN_LEFT, PBC_PIN_DOWN, PBC_PIN_UP,
    PBC_PIN_B, PBC_PIN_A, PBC_PIN_CENTER
};
static const uint16_t bits[7] = { PAD_RI, PAD_LF, PAD_DN, PAD_UP, PAD_B, PAD_A, PAD_C };

static uint16_t stable, candidate, prev;

void pbc_input_init(void)
{
    int i;
    for (i = 0; i < 7; i++) {
        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_IN);
        gpio_pull_up(pins[i]);
    }
    stable = candidate = prev = 0;
}

uint16_t pbc_input_raw(void)
{
    uint32_t all = gpio_get_all();
    uint16_t v = 0;
    int i;
    for (i = 0; i < 7; i++)
        if (!(all & (1u << pins[i])))      /* aktiv LOW */
            v |= bits[i];
    return v;
}

uint16_t pbc_input_read(void)
{
    uint16_t now = pbc_input_raw();
    uint16_t trg;

    if (now == candidate)
        stable = now;
    candidate = now;

    trg = (uint16_t)((stable & ~prev) << 8);
    prev = stable;
    return (uint16_t)(stable | trg);
}
