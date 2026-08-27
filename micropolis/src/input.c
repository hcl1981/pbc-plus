#include "input.h"
#include "config.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"

// First repeat after this many frames of holding, then every REPEAT_RATE.
#define REPEAT_DELAY 6
#define REPEAT_RATE  2

static const struct { uint8_t pin; uint32_t bit; } k_map[] = {
    { PIN_BTN_LEFT,   IN_LEFT  },
    { PIN_BTN_RIGHT,  IN_RIGHT },
    { PIN_BTN_UP,     IN_UP    },
    { PIN_BTN_DOWN,   IN_DOWN  },
    { PIN_BTN_A,      IN_A     },
    { PIN_BTN_B,      IN_B     },
    { PIN_BTN_CENTER, IN_B     },   // centre press = B (matches reference)
};

static uint32_t s_prev = 0;
static int      s_hold[6];   // per-bit hold counter (index = bit position)

void input_init(void) {
    for (unsigned i = 0; i < count_of(k_map); ++i) {
        gpio_init(k_map[i].pin);
        gpio_set_dir(k_map[i].pin, GPIO_IN);
        gpio_pull_up(k_map[i].pin);
    }
    for (int i = 0; i < 6; ++i) s_hold[i] = 0;
}

uint32_t input_raw(void) {
    uint32_t m = 0;
    for (unsigned i = 0; i < count_of(k_map); ++i)
        if (gpio_get(k_map[i].pin) == 0)   // active low
            m |= k_map[i].bit;
    return m;
}

uint32_t input_poll(void) {
    uint32_t now  = input_raw();
    uint32_t fire = 0;

    for (int b = 0; b < 6; ++b) {
        uint32_t mask = (1u << b);
        if (now & mask) {
            if (!(s_prev & mask)) {                 // fresh press
                fire |= mask;
                s_hold[b] = 0;
            } else {                                // held
                if (++s_hold[b] >= REPEAT_DELAY) {
                    if (((s_hold[b] - REPEAT_DELAY) % REPEAT_RATE) == 0)
                        fire |= mask;
                }
            }
        } else {
            s_hold[b] = 0;
        }
    }
    s_prev = now;
    return fire;
}
