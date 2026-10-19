/* input.c -- see input.h */
#include "pico/stdlib.h"
#include "board.h"
#include "input.h"

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

uint8_t input_poll_gb(void)
{
    uint32_t r = input_raw();
    uint8_t j = 0;

    if (r & IN_UP)    j |= GB_UP;
    if (r & IN_DOWN)  j |= GB_DOWN;
    if (r & IN_LEFT)  j |= GB_LEFT;
    if (r & IN_RIGHT) j |= GB_RIGHT;

    if (r & IN_CENTER) {
        if (r & IN_B) j |= GB_START;
        if (r & IN_A) j |= GB_SELECT;
    } else {
        if (r & IN_A) j |= GB_A;
        if (r & IN_B) j |= GB_B;
    }
    return j;
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
