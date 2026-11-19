/* input.c -- joystick, A, B and the joystick's press (CENTER). */
#include "pico/stdlib.h"
#include "board.h"
#include "pbc.h"

/* in the order of BTN_UP .. BTN_C in rt.h */
static const uint8_t pins[] = {
    PBC_PIN_UP, PBC_PIN_DOWN, PBC_PIN_LEFT, PBC_PIN_RIGHT, PBC_PIN_A, PBC_PIN_B, PBC_PIN_CENTER,
};

void pbc_input_init(void)
{
    for (unsigned i = 0; i < sizeof pins; i++) {
        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_IN);
        gpio_pull_up(pins[i]);
    }
    sleep_ms(2);                            /* the pull-ups settle */
}

/* Real switches bounce: for a few milliseconds after being pressed or let
 * go they flicker. A button counts as changed only when two readings in a
 * row (the game asks every 20 ms) agree; one stray reading is ignored. */
uint32_t pbc_input(void)
{
    static uint32_t before, state;
    uint32_t all = gpio_get_all(), r = 0;
    for (unsigned i = 0; i < sizeof pins; i++)
        if (!(all & (1u << pins[i])))
            r |= 1u << i;
    state = (state & (r | before)) | (r & before);
    before = r;
    return state;
}
