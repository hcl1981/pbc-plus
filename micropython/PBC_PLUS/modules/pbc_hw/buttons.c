#include "buttons.h"
#include "gpio_irq.h"
#include "pbc_hw_config.h"

#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "pico/time.h"

#include "py/runtime.h"
#include "py/mphal.h"
#include "py/gc.h"

static const uint8_t button_pins[BTN_COUNT] = {
    [BTN_IDX_UP]     = BTN_UP_PIN,
    [BTN_IDX_DOWN]   = BTN_DOWN_PIN,
    [BTN_IDX_LEFT]   = BTN_LEFT_PIN,
    [BTN_IDX_RIGHT]  = BTN_RIGHT_PIN,
    [BTN_IDX_CENTER] = BTN_CENTER_PIN,
    [BTN_IDX_A]      = BTN_A_PIN,
    [BTN_IDX_B]      = BTN_B_PIN,
};

static volatile bool     button_latch[BTN_COUNT];
static volatile uint32_t button_last_ms[BTN_COUNT];
static mp_obj_t          button_callbacks[BTN_COUNT];

MP_REGISTER_ROOT_POINTER(mp_obj_t pbc_hw_button_callbacks_root[BTN_COUNT]);

#define DEBOUNCE_MS 50

// One-time gate: the SDK's gpio_add_raw_irq_handler_masked doesn't
// dedupe, so we install our handler exactly once per cold boot.
static bool buttons_handler_installed = false;

static void buttons_raw_irq_handler(void);

// HW-only init. Idempotent. Re-applies pin direction, pull-ups,
// and IRQ enables -- which MicroPython's machine_pin_init() wipes
// on every soft-reboot. Does NOT touch callback state; that's
// buttons_clear_callbacks's job.
//
// Cheap on second-and-later calls (a handful of register writes
// per pin), so safe to call from every Python entry point via
// pbc_hw_sync_lifecycle().
void buttons_init(void) {
    uint64_t mask = 0;
    for (int i = 0; i < BTN_COUNT; i++) {
        mask |= 1ULL << button_pins[i];
    }

    if (!buttons_handler_installed) {
        // First-ever cold-boot setup: initial callback state.
        for (int i = 0; i < BTN_COUNT; i++) {
            button_callbacks[i] = mp_const_none;
            button_latch[i]     = false;
            button_last_ms[i]   = 0;
            MP_STATE_VM(pbc_hw_button_callbacks_root)[i] = mp_const_none;
        }
    }

    // Re-install our raw IRQ handler EVERY time at the SDK's
    // highest order priority, so we run before MicroPython's
    // machine_pin gpio_irq handler.
    //
    // machine_pin_init's gpio_irq (priority
    // PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY = 0x80) calls
    // gpio_acknowledge_irq() for every pending event on every pin
    // -- not just the ones it manages. If it runs before us, our
    // events are gone by the time gpio_get_irq_event_mask() reads
    // them, and we silently never schedule callbacks. Registering
    // at HIGHEST priority (0xff) guarantees we ack our pins first.
    if (buttons_handler_installed) {
        gpio_remove_raw_irq_handler_masked(mask, buttons_raw_irq_handler);
    }
    gpio_add_raw_irq_handler_with_order_priority_masked(
        mask, buttons_raw_irq_handler,
        PICO_SHARED_IRQ_HANDLER_HIGHEST_ORDER_PRIORITY);
    buttons_handler_installed = true;

    // Always-do: full pin re-init + NVIC re-enable. MicroPython's
    // soft-reboot path (machine_pin_init / mp_init) can reset any
    // of these. We don't bother trying to figure out which -- just
    // reapply everything. All of these are idempotent and combined
    // they cost a few dozen register writes.
    for (int i = 0; i < BTN_COUNT; i++) {
        gpio_init(button_pins[i]);                // SIO mode, dir IN, output cleared
        gpio_set_dir(button_pins[i], GPIO_IN);
        gpio_pull_up(button_pins[i]);
        gpio_set_irq_enabled(button_pins[i], GPIO_IRQ_EDGE_FALL, true);
    }
    irq_set_enabled(IO_IRQ_BANK0, true);
}

// Drop all cached Python callback references and reset the latch.
// Called from pbc_hw.init() (which is in turn called from pbc.py's
// module body on every soft-reboot) to make sure the IRQ handler
// never schedules a stale Python pointer left over from the
// previous VM session.
void buttons_clear_callbacks(void) {
    uint32_t save = save_and_disable_interrupts();
    for (int i = 0; i < BTN_COUNT; i++) {
        button_callbacks[i] = mp_const_none;
        button_latch[i]     = false;
        MP_STATE_VM(pbc_hw_button_callbacks_root)[i] = mp_const_none;
    }
    restore_interrupts(save);
}

bool button_pressed(int idx) {
    if (idx < 0 || idx >= BTN_COUNT) return false;
    if (!buttons_handler_installed) buttons_init();
    return !gpio_get(button_pins[idx]);
}

bool button_was_pressed(int idx) {
    if (idx < 0 || idx >= BTN_COUNT) return false;
    if (!buttons_handler_installed) buttons_init();
    bool was = button_latch[idx];
    button_latch[idx] = false;
    return was;
}

void button_set_callback(int idx, mp_obj_t cb) {
    if (idx < 0 || idx >= BTN_COUNT) return;
    if (!buttons_handler_installed) buttons_init();
    button_callbacks[idx] = cb;
    MP_STATE_VM(pbc_hw_button_callbacks_root)[idx] = cb;
}

// Shared raw GPIO IRQ handler. By the time we get here, pbc.py's
// module body has run pbc_hw.init() which cleared any stale
// callback pointers from the previous session, so reading
// button_callbacks[i] is safe -- it's either mp_const_none (skip)
// or a freshly-registered Python callable (schedule).
static void buttons_raw_irq_handler(void) {
    for (int i = 0; i < BTN_COUNT; i++) {
        uint32_t pin = button_pins[i];
        uint32_t events = gpio_get_irq_event_mask(pin);
        if (events) {
            gpio_acknowledge_irq(pin, events);
            buttons_handle_gpio_irq(pin, events);
        }
    }
}

void buttons_handle_gpio_irq(uint32_t gpio, uint32_t events) {
    (void)events;
    for (int i = 0; i < BTN_COUNT; i++) {
        if (button_pins[i] != gpio) continue;
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if ((now - button_last_ms[i]) < DEBOUNCE_MS) return;
        button_last_ms[i] = now;
        button_latch[i]   = true;

        mp_obj_t cb = button_callbacks[i];
        if (cb != mp_const_none) {
            mp_sched_schedule(cb, MP_OBJ_NEW_SMALL_INT(i));
        }
        return;
    }
}
