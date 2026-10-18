/* input.c -- see input.h */
#include "pico/stdlib.h"
#include "board.h"
#include "input.h"

/* How long CENTER must be held alone before it counts as C. Long enough to
 * catch a slightly late A/B, short enough not to be felt in a game. */
#define C_GRACE_US 40000u

static const uint8_t pins[] = {
    PBC_PIN_UP, PBC_PIN_DOWN, PBC_PIN_LEFT, PBC_PIN_RIGHT,
    PBC_PIN_A, PBC_PIN_B, PBC_PIN_CENTER,
};

void input_init(void)
{
    for (unsigned i = 0; i < sizeof pins; i++) {
        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_IN);
        gpio_pull_up(pins[i]);
    }
    sleep_us(50);   /* let the pull-ups settle before the first read */
}

uint32_t input_raw(void)
{
    uint32_t all = gpio_get_all();
    uint32_t r = 0;
    for (unsigned i = 0; i < sizeof pins; i++)
        if (!(all & (1u << pins[i])))
            r |= 1u << i;
    return r;
}

genesis_input_t input_poll_genesis(void)
{
    static uint64_t center_since;
    static bool     combo_latched;   /* a combo was used during this CENTER press */
    genesis_input_t g = { 0xFF };
    uint32_t r = input_raw();
    uint64_t now = time_us_64();

    if (r & IN_UP)    g.pad &= ~(1u << 0);
    if (r & IN_DOWN)  g.pad &= ~(1u << 1);
    if (r & IN_LEFT)  g.pad &= ~(1u << 2);
    if (r & IN_RIGHT) g.pad &= ~(1u << 3);

    if (r & IN_CENTER) {
        if (center_since == 0)
            center_since = now;
        if (r & (IN_A | IN_B)) {
            combo_latched = true;
            if (r & IN_B) g.pad &= ~(1u << 7);   /* START  */
        } else if (!combo_latched && now - center_since >= C_GRACE_US) {
            g.pad &= ~(1u << 5);                 /* C */
        }
    } else {
        center_since = 0;
        combo_latched = false;
        if (r & IN_A) g.pad &= ~(1u << 6);
        if (r & IN_B) g.pad &= ~(1u << 4);
    }
    return g;
}

uint32_t input_menu_edges(void)
{
    static uint32_t prev;
    static uint64_t repeat_at;
    uint32_t r = input_raw();
    uint32_t edges = r & ~prev;
    uint64_t now = time_us_64();

    if (edges & (IN_UP | IN_DOWN | IN_LEFT | IN_RIGHT))
        repeat_at = now + 350000;
    else if ((r & (IN_UP | IN_DOWN | IN_LEFT | IN_RIGHT)) && now >= repeat_at) {
        edges |= r & (IN_UP | IN_DOWN | IN_LEFT | IN_RIGHT);
        repeat_at = now + 80000;
    }
    prev = r;
    return edges;
}
