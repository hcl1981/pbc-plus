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

int input_poll_sms(bool *start)
{
    uint32_t r = input_raw();
    int pad = 0;

    /* SMS Plus input.pad[] bits (core/smsplus/system.h) */
    if (r & IN_UP)    pad |= 0x01;
    if (r & IN_DOWN)  pad |= 0x02;
    if (r & IN_LEFT)  pad |= 0x04;
    if (r & IN_RIGHT) pad |= 0x08;

    *start = false;
    if (r & IN_CENTER) {
        if (r & IN_B) *start = true;
    } else {
        if (r & IN_B) pad |= 0x20;     /* button 1 */
        if (r & IN_A) pad |= 0x10;     /* button 2 */
    }
    return pad;
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
