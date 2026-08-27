#include "pico/stdlib.h"
#include "config.h"
#include "st7789.h"
#include "input.h"
#include "save.h"
#include "engine.h"
#include "tiles.h"
#include "render.h"
#include "ui.h"
#include "led.h"

// Single full-frame buffer in SRAM: 240*280*2 = 131,250 bytes.
static uint16_t g_fb[DISPLAY_W * DISPLAY_H];

int main(void) {
    stdio_init_all();

    st7789_init();
    input_init();
    save_init();

    tiles_init();
    engine_init();
    ui_init();                 // starts in UI_TITLE; the title screen picks new/load

    led_init();                // internal SK6805 status LED (crime/traffic)
    core_sync_start();         // launch the simulation on core1

    absolute_time_t next = get_absolute_time();

    for (;;) {
        next = delayed_by_ms(next, FRAME_MS);

        // core0: input + UI + render. The sim runs concurrently on core1;
        // ui_update's mutators (build/disaster/new game/save) lock internally.
        uint32_t fired = input_poll();
        ui_update(fired);

        led_task();            // blink the status LED from crime/traffic state

        render_frame(g_fb);    // reads sim state un-locked (cosmetic tearing ok)
        st7789_blit(g_fb);

        sleep_until(next);
    }
}
