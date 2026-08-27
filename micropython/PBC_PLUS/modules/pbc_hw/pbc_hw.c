// pbc_hw — top-level MicroPython binding for PicoBoy Color Plus
// hardware peripherals.
//
// Subsystems (each in its own .c file): tone, battery, buttons,
// rgb_led, accel.
//
// Soft-reboot story: MicroPython's machine_pin_init() runs on
// every soft-reboot and resets all GPIO state -- pull-ups, IRQ
// enables, the lot. Without intervention, our previously-set IRQs
// would silently stop firing. The frozen pbc.py module's body
// calls pbc_hw.init() on every import to (a) drop stale callback
// pointers from the previous VM session and (b) re-apply pin
// config + IRQ enables. Since sys.modules is wiped on every
// soft-reboot, `import pbc` re-runs the body and so init() runs
// every reboot without the user having to do anything.
//
// Every public Python entry point also calls buttons_init()
// defensively to refresh HW state in case someone uses pbc_hw
// directly without going through pbc.

#include "py/runtime.h"
#include "py/obj.h"
#include "py/objstr.h"
#include "py/mphal.h"

#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/sync.h"

#include "pbc_hw_config.h"
#include "gpio_irq.h"
#include "tone.h"
#include "battery.h"
#include "buttons.h"
#include "rgb_led.h"
#include "accel.h"

// =====================================================================
// GPIO IRQ enable
// =====================================================================
//
// Idempotent. The static-bool gate that used to live here was the
// reason buttons stopped working after soft-reboot: MicroPython
// can disable IO_IRQ_BANK0 at the NVIC during mp_init/mp_deinit
// transitions, and our gate prevented us re-enabling it. It's
// just one register write, so just always do it.

void pbc_hw_gpio_irq_dispatcher_init(void) {
    irq_set_enabled(IO_IRQ_BANK0, true);
}

// Defensive HW refresh on every Python call. buttons_init is
// idempotent and cheap (a handful of register writes per pin
// after the one-time handler install). This belts the suspenders
// of pbc.py's module-body init() call -- if a user reaches for
// pbc_hw directly without importing pbc, this still keeps IRQs
// enabled across soft-reboots.
//
// Does NOT clear callbacks; that's pbc_hw.init()'s job, called
// from pbc.py's module body once per reboot. Clearing on every
// Python call would silently delete callbacks the user just
// registered.
void pbc_hw_sync_lifecycle(void) {
    buttons_init();
}

// =====================================================================
// Tone
// =====================================================================

static mp_obj_t pbc_hw_tone(size_t n_args, const mp_obj_t *args) {
    pbc_hw_sync_lifecycle();
    uint32_t freq = mp_obj_get_int(args[0]);
    uint32_t dur  = (n_args > 1) ? mp_obj_get_int(args[1]) : 0;
    tone_play(freq, dur);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(pbc_hw_tone_obj, 1, 2, pbc_hw_tone);

static mp_obj_t pbc_hw_tone_off(void) {
    pbc_hw_sync_lifecycle();
    tone_off();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(pbc_hw_tone_off_obj, pbc_hw_tone_off);

// =====================================================================
// Battery
// =====================================================================

static mp_obj_t pbc_hw_battery_voltage(void) {
    pbc_hw_sync_lifecycle();
    return mp_obj_new_float(battery_voltage());
}
static MP_DEFINE_CONST_FUN_OBJ_0(pbc_hw_battery_voltage_obj, pbc_hw_battery_voltage);

// =====================================================================
// Buttons
// =====================================================================

#define DEFINE_BUTTON_FNS(name, idx)                                          \
    static mp_obj_t pbc_hw_pressed_##name(void) {                             \
        pbc_hw_sync_lifecycle();                                              \
        return mp_obj_new_bool(button_pressed(idx));                          \
    }                                                                         \
    static MP_DEFINE_CONST_FUN_OBJ_0(pbc_hw_pressed_##name##_obj,             \
                                     pbc_hw_pressed_##name);                  \
    static mp_obj_t pbc_hw_was_pressed_##name(void) {                         \
        pbc_hw_sync_lifecycle();                                              \
        return mp_obj_new_bool(button_was_pressed(idx));                      \
    }                                                                         \
    static MP_DEFINE_CONST_FUN_OBJ_0(pbc_hw_was_pressed_##name##_obj,         \
                                     pbc_hw_was_pressed_##name);              \
    static mp_obj_t pbc_hw_on_press_##name(mp_obj_t cb) {                     \
        pbc_hw_sync_lifecycle();                                              \
        if (cb != mp_const_none && !mp_obj_is_callable(cb)) {                 \
            mp_raise_TypeError(MP_ERROR_TEXT("callback must be callable"));   \
        }                                                                     \
        button_set_callback(idx, cb);                                         \
        return mp_const_none;                                                 \
    }                                                                         \
    static MP_DEFINE_CONST_FUN_OBJ_1(pbc_hw_on_press_##name##_obj,            \
                                     pbc_hw_on_press_##name);

DEFINE_BUTTON_FNS(up,     BTN_IDX_UP)
DEFINE_BUTTON_FNS(down,   BTN_IDX_DOWN)
DEFINE_BUTTON_FNS(left,   BTN_IDX_LEFT)
DEFINE_BUTTON_FNS(right,  BTN_IDX_RIGHT)
DEFINE_BUTTON_FNS(center, BTN_IDX_CENTER)
DEFINE_BUTTON_FNS(a,      BTN_IDX_A)
DEFINE_BUTTON_FNS(b,      BTN_IDX_B)

// =====================================================================
// RGB LED
// =====================================================================

static mp_obj_t pbc_hw_rgb_led(mp_obj_t r_obj, mp_obj_t g_obj, mp_obj_t b_obj) {
    pbc_hw_sync_lifecycle();
    int r = mp_obj_get_int(r_obj);
    int g = mp_obj_get_int(g_obj);
    int b = mp_obj_get_int(b_obj);
    if (r < 0)   r = 0;
    if (r > 255) r = 255;
    if (g < 0)   g = 0;
    if (g > 255) g = 255;
    if (b < 0)   b = 0;
    if (b > 255) b = 255;
    rgb_led_set((uint8_t)r, (uint8_t)g, (uint8_t)b);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_3(pbc_hw_rgb_led_obj, pbc_hw_rgb_led);

static mp_obj_t pbc_hw_rgb_led_off(void) {
    pbc_hw_sync_lifecycle();
    rgb_led_off();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(pbc_hw_rgb_led_off_obj, pbc_hw_rgb_led_off);

// =====================================================================
// Accelerometer
// =====================================================================

static mp_obj_t pbc_hw_accel_x(void)   { pbc_hw_sync_lifecycle(); return mp_obj_new_float(accel_x_ms2()); }
static mp_obj_t pbc_hw_accel_y(void)   { pbc_hw_sync_lifecycle(); return mp_obj_new_float(accel_y_ms2()); }
static mp_obj_t pbc_hw_accel_z(void)   { pbc_hw_sync_lifecycle(); return mp_obj_new_float(accel_z_ms2()); }
static mp_obj_t pbc_hw_accel_x_g(void) { pbc_hw_sync_lifecycle(); return mp_obj_new_float(accel_x_g());   }
static mp_obj_t pbc_hw_accel_y_g(void) { pbc_hw_sync_lifecycle(); return mp_obj_new_float(accel_y_g());   }
static mp_obj_t pbc_hw_accel_z_g(void) { pbc_hw_sync_lifecycle(); return mp_obj_new_float(accel_z_g());   }
static MP_DEFINE_CONST_FUN_OBJ_0(pbc_hw_accel_x_obj,   pbc_hw_accel_x);
static MP_DEFINE_CONST_FUN_OBJ_0(pbc_hw_accel_y_obj,   pbc_hw_accel_y);
static MP_DEFINE_CONST_FUN_OBJ_0(pbc_hw_accel_z_obj,   pbc_hw_accel_z);
static MP_DEFINE_CONST_FUN_OBJ_0(pbc_hw_accel_x_g_obj, pbc_hw_accel_x_g);
static MP_DEFINE_CONST_FUN_OBJ_0(pbc_hw_accel_y_g_obj, pbc_hw_accel_y_g);
static MP_DEFINE_CONST_FUN_OBJ_0(pbc_hw_accel_z_g_obj, pbc_hw_accel_z_g);

// =====================================================================
// Module init -- the explicit reset entry point. pbc.py's module
// body calls this on every soft-reboot (sys.modules is wiped, so
// `import pbc` re-runs the body) to drop stale callback pointers
// from the previous VM session before any user code can register
// new ones.
// =====================================================================

static mp_obj_t pbc_hw_init(void) {
    buttons_init();             // applies HW config (idempotent)
    buttons_clear_callbacks();  // drops stale Python pointers
    rgb_led_init();
    accel_init();
    battery_init();             // ADC bring-up + filter-cap settle
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(pbc_hw_init_obj, pbc_hw_init);

// =====================================================================
// Module table
// =====================================================================

static const mp_rom_map_elem_t pbc_hw_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),         MP_ROM_QSTR(MP_QSTR_pbc_hw) },
    { MP_ROM_QSTR(MP_QSTR_init),             MP_ROM_PTR(&pbc_hw_init_obj) },

    { MP_ROM_QSTR(MP_QSTR_tone),             MP_ROM_PTR(&pbc_hw_tone_obj) },
    { MP_ROM_QSTR(MP_QSTR_tone_off),         MP_ROM_PTR(&pbc_hw_tone_off_obj) },

    { MP_ROM_QSTR(MP_QSTR_battery_voltage),  MP_ROM_PTR(&pbc_hw_battery_voltage_obj) },

    { MP_ROM_QSTR(MP_QSTR_pressed_up),       MP_ROM_PTR(&pbc_hw_pressed_up_obj) },
    { MP_ROM_QSTR(MP_QSTR_pressed_down),     MP_ROM_PTR(&pbc_hw_pressed_down_obj) },
    { MP_ROM_QSTR(MP_QSTR_pressed_left),     MP_ROM_PTR(&pbc_hw_pressed_left_obj) },
    { MP_ROM_QSTR(MP_QSTR_pressed_right),    MP_ROM_PTR(&pbc_hw_pressed_right_obj) },
    { MP_ROM_QSTR(MP_QSTR_pressed_center),   MP_ROM_PTR(&pbc_hw_pressed_center_obj) },
    { MP_ROM_QSTR(MP_QSTR_pressed_a),        MP_ROM_PTR(&pbc_hw_pressed_a_obj) },
    { MP_ROM_QSTR(MP_QSTR_pressed_b),        MP_ROM_PTR(&pbc_hw_pressed_b_obj) },

    { MP_ROM_QSTR(MP_QSTR_was_pressed_up),     MP_ROM_PTR(&pbc_hw_was_pressed_up_obj) },
    { MP_ROM_QSTR(MP_QSTR_was_pressed_down),   MP_ROM_PTR(&pbc_hw_was_pressed_down_obj) },
    { MP_ROM_QSTR(MP_QSTR_was_pressed_left),   MP_ROM_PTR(&pbc_hw_was_pressed_left_obj) },
    { MP_ROM_QSTR(MP_QSTR_was_pressed_right),  MP_ROM_PTR(&pbc_hw_was_pressed_right_obj) },
    { MP_ROM_QSTR(MP_QSTR_was_pressed_center), MP_ROM_PTR(&pbc_hw_was_pressed_center_obj) },
    { MP_ROM_QSTR(MP_QSTR_was_pressed_a),      MP_ROM_PTR(&pbc_hw_was_pressed_a_obj) },
    { MP_ROM_QSTR(MP_QSTR_was_pressed_b),      MP_ROM_PTR(&pbc_hw_was_pressed_b_obj) },

    { MP_ROM_QSTR(MP_QSTR_on_press_up),      MP_ROM_PTR(&pbc_hw_on_press_up_obj) },
    { MP_ROM_QSTR(MP_QSTR_on_press_down),    MP_ROM_PTR(&pbc_hw_on_press_down_obj) },
    { MP_ROM_QSTR(MP_QSTR_on_press_left),    MP_ROM_PTR(&pbc_hw_on_press_left_obj) },
    { MP_ROM_QSTR(MP_QSTR_on_press_right),   MP_ROM_PTR(&pbc_hw_on_press_right_obj) },
    { MP_ROM_QSTR(MP_QSTR_on_press_center),  MP_ROM_PTR(&pbc_hw_on_press_center_obj) },
    { MP_ROM_QSTR(MP_QSTR_on_press_a),       MP_ROM_PTR(&pbc_hw_on_press_a_obj) },
    { MP_ROM_QSTR(MP_QSTR_on_press_b),       MP_ROM_PTR(&pbc_hw_on_press_b_obj) },

    { MP_ROM_QSTR(MP_QSTR_rgb_led),          MP_ROM_PTR(&pbc_hw_rgb_led_obj) },
    { MP_ROM_QSTR(MP_QSTR_rgb_led_off),      MP_ROM_PTR(&pbc_hw_rgb_led_off_obj) },

    { MP_ROM_QSTR(MP_QSTR_accel_x),          MP_ROM_PTR(&pbc_hw_accel_x_obj) },
    { MP_ROM_QSTR(MP_QSTR_accel_y),          MP_ROM_PTR(&pbc_hw_accel_y_obj) },
    { MP_ROM_QSTR(MP_QSTR_accel_z),          MP_ROM_PTR(&pbc_hw_accel_z_obj) },
    { MP_ROM_QSTR(MP_QSTR_accel_x_g),        MP_ROM_PTR(&pbc_hw_accel_x_g_obj) },
    { MP_ROM_QSTR(MP_QSTR_accel_y_g),        MP_ROM_PTR(&pbc_hw_accel_y_g_obj) },
    { MP_ROM_QSTR(MP_QSTR_accel_z_g),        MP_ROM_PTR(&pbc_hw_accel_z_g_obj) },
};
static MP_DEFINE_CONST_DICT(pbc_hw_module_globals, pbc_hw_module_globals_table);

const mp_obj_module_t pbc_hw_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&pbc_hw_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_pbc_hw, pbc_hw_user_cmodule);
