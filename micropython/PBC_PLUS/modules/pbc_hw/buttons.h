#ifndef PBC_HW_BUTTONS_H
#define PBC_HW_BUTTONS_H

#include <stdint.h>
#include <stdbool.h>

#include "py/obj.h"

// Stable button indices used by all three API variants and exposed
// to Python as module-level constants (BTN_UP … BTN_B).
#define BTN_IDX_UP     0
#define BTN_IDX_DOWN   1
#define BTN_IDX_LEFT   2
#define BTN_IDX_RIGHT  3
#define BTN_IDX_CENTER 4
#define BTN_IDX_A      5
#define BTN_IDX_B      6
#define BTN_COUNT      7

// Idempotent. Configures every button pin as input-with-pull-up,
// enables falling-edge IRQs, and registers with the shared GPIO IRQ
// dispatcher.
void buttons_init(void);

// Live state — true while the button is held down.
bool button_pressed(int idx);

// Edge-latched state — true if there has been at least one falling
// edge since the last time this function returned true for this
// button. Reading clears the latch.
bool button_was_pressed(int idx);

// Register a Python callable as an on-press IRQ handler. The
// callable runs from the MicroPython scheduler (not in interrupt
// context) and is called with one argument: the button index.
// Pass mp_const_none to remove the callback.
void button_set_callback(int idx, mp_obj_t cb);

#endif // PBC_HW_BUTTONS_H
