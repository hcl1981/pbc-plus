//
// PicoBoy Color input handler for rp2040-doom-rp2.
//
// Replaces the original USB-keyboard / TinyUSB-host input pipeline with
// direct GPIO polling of the PicoBoy 5-way joystick + A/B action buttons.
//
// Mapping (context-aware: depends on whether the menu is open):
//
//   In-game (menu closed):
//     Joystick UP/DOWN/LEFT/RIGHT  -> arrows (move/turn)
//     Joystick CENTER              -> KEY_USE (' ') - open doors
//     A button                     -> KEY_INS - cycle next weapon
//     B button                     -> KEY_RCTRL - fire
//
//   In the menu:
//     Joystick UP/DOWN/LEFT/RIGHT  -> arrows (navigate / change value)
//     Joystick CENTER              -> KEY_ENTER - confirm
//     A button                     -> KEY_ENTER - confirm
//     B button                     -> KEY_BACKSPACE - back one level
//
// Each button is reported as press/release exactly the way Doom's PC
// keyboard input would: one ev_keydown on the down edge, one ev_keyup
// on the up edge. The button stays "held" between the two so Doom's
// per-tic checks see it.
//
// Debounce: simple stable-for-N-polls. With polling at the Doom tic
// rate (35 Hz), 2 polls = ~57 ms, which is enough for the joystick
// without making the controls feel laggy.
//
// GPLv2 (same as rp2040-doom).
//

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "pico.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/gpio.h"

#include "doomkeys.h"
#include "doomtype.h"
#include "d_event.h"
#include "i_input.h"
#include "i_system.h"
#include "i_video.h"

#include "picoboy_config.h"

// Defined in src/doom/m_menu.c -- true while the in-game menu is open.
extern boolean menuactive;

// ---- Engine-facing globals & stubs -----------------------------------------

int novert = 0;
// vanilla_keyboard_mapping is a compile-time #define (= true) in this build
// of i_video.h, so we must NOT declare it as a variable here -- doing so
// expands to "int true = 1;" and breaks the compile.

void I_StartTextInput(int x1, int y1, int x2, int y2)
{ (void) x1; (void) y1; (void) x2; (void) y2; }

void I_StopTextInput(void) { }

int GetTypedChar(int scancode, boolean shiftdown) { (void) scancode; (void) shiftdown; return 0; }

void I_GetEvent(void)                       { /* polled in I_StartTic */ }
void I_GetEventTimeout(int key_timeout)     { (void) key_timeout; }
void I_ReadMouse(void)                      { }
void I_BindInputVariables(void)             { }

// Engine entry point called from i_system.c's I_Init. Note the spelling:
// 'I_InputInit' (Input...Init), not 'I_InitInput'. We just chain into our
// own picoboy_input_init -- harmless if also called from I_InitGraphics.
void I_InputInit(void)
{
    extern void picoboy_input_init(void);
    picoboy_input_init();
}

// ---- Buttons ---------------------------------------------------------------

enum {
    BTN_UP = 0, BTN_DOWN, BTN_LEFT, BTN_RIGHT,
    BTN_CENTER, BTN_A, BTN_B,
    BTN_COUNT
};

static const uint8_t btn_pins[BTN_COUNT] = {
    PICOBOY_PIN_JOY_UP,
    PICOBOY_PIN_JOY_DOWN,
    PICOBOY_PIN_JOY_LEFT,
    PICOBOY_PIN_JOY_RIGHT,
    PICOBOY_PIN_JOY_CENTER,
    PICOBOY_PIN_BTN_A,
    PICOBOY_PIN_BTN_B,
};

// One Doom key per button. Sent on press edge as keydown, on release
// edge as keyup. Doom keeps the key "held" between the two events.
static const uint16_t btn_doom_key[BTN_COUNT] = {
    KEY_UPARROW,    // UP
    KEY_DOWNARROW,  // DOWN
    KEY_LEFTARROW,  // LEFT
    KEY_RIGHTARROW, // RIGHT
    ' ',            // CENTER -> USE (Doom default key_use is space)
    KEY_ENTER,      // A      -> ENTER (menu confirm; opens menu on title)
    KEY_RCTRL,      // B      -> FIRE
};

// Debounce: each button has a stability counter. Raw reading must be
// stable for DEBOUNCE_TICKS consecutive polls before the latched state
// flips. Reset counter when raw matches latched (no flip needed).
#define DEBOUNCE_TICKS 2
static uint8_t debounce_counter[BTN_COUNT];
static uint8_t btn_state = 0;       // latched state, 1 bit per button
static uint8_t btn_state_prev = 0;  // last tic's latched state, for edges

static inline uint8_t read_buttons_raw(void)
{
    uint32_t all = gpio_get_all();
    uint8_t s = 0;
    for (int i = 0; i < BTN_COUNT; i++) {
        // active-low (pull-up + button to GND)
        if ((all & (1u << btn_pins[i])) == 0) s |= (1u << i);
    }
    return s;
}

static inline void debounce_update(uint8_t raw)
{
    for (int i = 0; i < BTN_COUNT; i++) {
        bool raw_pressed = (raw & (1u << i)) != 0;
        bool deb_pressed = (btn_state & (1u << i)) != 0;
        if (raw_pressed != deb_pressed) {
            if (++debounce_counter[i] >= DEBOUNCE_TICKS) {
                btn_state ^= (1u << i);
                debounce_counter[i] = 0;
            }
        } else {
            debounce_counter[i] = 0;
        }
    }
}

static void post_key_event(int doomkey, bool down)
{
    event_t ev;
    ev.type  = down ? ev_keydown : ev_keyup;
    ev.data1 = doomkey;
    ev.data2 = doomkey;
    ev.data3 = doomkey;
    D_PostEvent(&ev);
}

// ---- Public entry points ---------------------------------------------------

// ---- Boot-time cheat selection ---------------------------------------------
//
// If A and/or B is held down while the device powers on, we enable a cheat
// for the whole session (applied to the console player every time they spawn,
// see P_SpawnPlayer in p_mobj.c):
//
//   A held        -> iddqd  (god mode / invulnerability)
//   B held        -> idkfa  (all weapons, full ammo, all keys, max armor)
//   A + B held    -> both
//
// Sampled exactly once, on the first input init, so that releasing the
// buttons after boot doesn't clear the selection.
boolean picoboy_boot_god = false;   // read by P_SpawnPlayer
boolean picoboy_boot_kfa = false;
static boolean boot_cheats_captured = false;

// Called once at startup from i_video.c (PicoBoy hook in I_InitGraphics).
void picoboy_input_init(void)
{
    for (int i = 0; i < BTN_COUNT; i++) {
        gpio_init(btn_pins[i]);
        gpio_set_dir(btn_pins[i], GPIO_IN);
        gpio_pull_up(btn_pins[i]);
    }

    // Sample A/B for the boot-time cheat selection. Give the pull-ups a
    // moment to settle, then latch the result so it survives the buttons
    // being released later (and a possible second init call).
    if (!boot_cheats_captured) {
        busy_wait_us(3000);
        uint8_t raw = read_buttons_raw();
        if (raw & (1u << BTN_A)) picoboy_boot_god = true;
        if (raw & (1u << BTN_B)) picoboy_boot_kfa = true;
        boot_cheats_captured = true;
    }

    btn_state = btn_state_prev = 0;
    memset(debounce_counter, 0, sizeof(debounce_counter));
}

// Called every Doom tic from I_StartTic.
void picoboy_input_poll(void)
{
    uint8_t raw = read_buttons_raw();
    debounce_update(raw);

    uint8_t pressed_edges  = btn_state & ~btn_state_prev;
    uint8_t released_edges = ~btn_state & btn_state_prev;

    // Special handling for CENTER -> KEY_USE: hold the key down for a
    // minimum of N tics so that even very short presses get reliably
    // picked up by G_BuildTiccmd (which only consults is_gamekeydown
    // once per tic).
    static int center_min_hold = 0;

    for (int i = 0; i < BTN_COUNT; i++) {
        int key = btn_doom_key[i];
        if (i == BTN_CENTER) {
            if (pressed_edges & (1u << i)) {
                post_key_event(key, true);
                center_min_hold = 4;          // ~114 ms minimum hold
                // While the menu is open, also post KEY_ENTER so that
                // CENTER works as the menu-confirm button. Doom in-game
                // doesn't react to KEY_ENTER, so this causes no harm
                // when sent unconditionally.
                if (menuactive) post_key_event(KEY_ENTER, true);
            } else if (released_edges & (1u << i)) {
                if (center_min_hold == 0) post_key_event(key, false);
                // also release the menu-ENTER if we sent it on press
                post_key_event(KEY_ENTER, false);
            }
        } else if (i == BTN_A) {
            // A button:
            //   menu open  -> KEY_ENTER (confirm, like CENTER)
            //   in-game    -> KEY_INS, which is bound to key_nextweapon
            //                 in m_controls.c for the PicoBoy build.
            //                 G_Responder picks this up and the engine's
            //                 G_NextWeapon() cycles to the next OWNED,
            //                 SELECTABLE weapon -- handles ammo, the
            //                 slot-1 fist/chainsaw toggle, and missing-
            //                 weapon WAD slots all on its own.
            if (pressed_edges & (1u << i)) {
                if (menuactive) post_key_event(KEY_ENTER, true);
                else            post_key_event(KEY_INS,   true);
            }
            if (released_edges & (1u << i)) {
                post_key_event(KEY_ENTER, false);
                post_key_event(KEY_INS,   false);
            }
        } else if (i == BTN_B) {
            // B button:
            //   menu open  -> KEY_BACKSPACE (back one menu level)
            //   in-game    -> KEY_RCTRL     (fire)
            if (pressed_edges & (1u << i)) {
                if (menuactive) post_key_event(KEY_BACKSPACE, true);
                else            post_key_event(KEY_RCTRL,     true);
            }
            if (released_edges & (1u << i)) {
                post_key_event(KEY_BACKSPACE, false);
                post_key_event(KEY_RCTRL,     false);
            }
        } else {
            if (pressed_edges  & (1u << i)) post_key_event(key, true);
            if (released_edges & (1u << i)) post_key_event(key, false);
        }
    }

    // Drain the deferred CENTER keyup if the user already let go.
    if (center_min_hold > 0) {
        if (--center_min_hold == 0) {
            bool still_held = (btn_state & (1u << BTN_CENTER)) != 0;
            if (!still_held) post_key_event(' ', false);
        }
    }

    btn_state_prev = btn_state;
}
