/* main.c -- Super Transball 2 auf dem PicoBoy Color Plus.
 *
 * Startreihenfolge wie bei den anderen Ports: LEDs, Display, dann erst Ton.
 * Bleibt das Geraet mit gelber LED stehen, war es die Ton-Initialisierung.
 *
 * Beim Booten gehaltene Tasten:  B = Ton aus,  A = Debugzeile.
 */
#include <string.h>
#include "pico/stdlib.h"

#include "game.h"
#include "pbc/board.h"
#include "pbc/display.h"
#include "pbc/input.h"
#include "pbc/audio.h"
#include "pbc/store.h"
#include "pbc/leds.h"

#define STRIP_ROWS 8
static uint16_t strip[2][SCR_W * STRIP_ROWS];

static void push_frame(void)
{
    int cur = 0, y;
    pbc_display_window(0, 0, SCR_W, SCR_H);
    pbc_display_begin();
    for (y = 0; y < SCR_H; y += STRIP_ROWS) {
        int n = (y + STRIP_ROWS <= SCR_H) ? STRIP_ROWS : (SCR_H - y);
        stb_render_rows(strip[cur], y, n);     /* der andere Puffer ist in der DMA */
        pbc_display_wait();
        pbc_display_send(strip[cur], n * SCR_W);
        cur ^= 1;
    }
    pbc_display_end();
}

int main(void)
{
    uint16_t boot_keys;
    int saved[2];
    uint64_t next;
    int mute;

    pbc_leds_init();
    pbc_input_init();
    sleep_ms(20);
    boot_keys = pbc_input_raw();
    mute = (boot_keys & PAD_B) ? 1 : 0;

    pbc_display_init();
    pbc_led_green(true);

    stb_render_init();
    stb_render.show_debug = (boot_keys & PAD_A) ? 1 : 0;
    stb_render.mute = mute;

    pbc_store_load(saved);                     /* [0] = weitester Level */
    stb_init((uint32_t)time_us_64() ^ 0x9E3779B9u, saved[0]);

    stb_tick(0);
    push_frame();
    pbc_display_backlight(255);

    if (!mute) {
        pbc_led_yellow(true);
        if (pbc_audio_init())
            pbc_led_yellow(false);
        else
            stb_render.mute = 1;
    }

    next = time_us_64();
    for (;;) {
        uint64_t t0 = time_us_64();
        uint16_t pad = pbc_input_read();

        stb_tick(pad);
        stb_render.blink++;

        if (!stb_render.mute)
            pbc_audio_frame(&stb.audio);

        pbc_led_red(stb.status == ST_GAME && stb.fuel * 4 < stb.fuel_max);

        push_frame();

        /* Weitesten Level sichern, sobald ein neuer erreicht ist */
        if (stb.level > saved[0]) {
            int hs[2];
            hs[0] = stb.level;
            hs[1] = 0;
            if (pbc_store_save(hs))
                saved[0] = stb.level;
        }

        stb_render.ms_frame = (int)((time_us_64() - t0) / 1000u);

        next += 20000u;                        /* 50 Bilder je Sekunde */
        {
            int64_t wait = (int64_t)(next - time_us_64());
            if (wait > 0)
                sleep_us((uint64_t)wait);
            else if (wait < -100000)
                next = time_us_64();
        }
    }
}
