/* main.c -- Noiz2sa auf dem PicoBoy Color Plus.
 *
 * Startreihenfolge wie beim Spout-Port: LEDs zuerst, dann Display, erst danach
 * der Tonpfad.  Bleibt das Geraet mit gelber LED stehen, war es die
 * Audio-Initialisierung (CLAUDE.md Abschnitt 4).
 *
 * Beim Booten gehaltene Tasten:
 *   B   Ton aus
 *   A   Debugzeile: Bildzeit, Geschosse, Laeufer
 */
#include <string.h>
#include "pico/stdlib.h"

#include "game.h"
#include "render.h"
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
        noiz_render_rows(strip[cur], y, n);    /* der andere Puffer ist in der DMA */
        pbc_display_wait();
        pbc_display_send(strip[cur], n * SCR_W);
        cur ^= 1;
    }
    pbc_display_end();
}

int main(void)
{
    uint16_t boot_keys;
    int hiscore[2];
    int saved;
    uint64_t next;
    int mute;

    pbc_leds_init();
    pbc_input_init();
    sleep_ms(20);
    boot_keys = pbc_input_raw();
    mute = (boot_keys & PAD_B) ? 1 : 0;

    pbc_display_init();
    pbc_led_green(true);

    noiz_render_init();
    noiz_render.show_debug = (boot_keys & PAD_A) ? 1 : 0;
    noiz_render.mute = mute;

    pbc_store_load(hiscore);
    saved = hiscore[0];
    noiz_init((uint32_t)time_us_64() ^ 0x9E3779B9u, hiscore[0]);

    noiz_tick(0);
    push_frame();
    pbc_display_backlight(255);

    if (!mute) {
        pbc_led_yellow(true);
        if (pbc_audio_init())
            pbc_led_yellow(false);
        else
            noiz_render.mute = 1;
    }

    next = time_us_64();
    for (;;) {
        uint64_t t0 = time_us_64(), t1;
        uint16_t pad = pbc_input_read();

        noiz_tick(pad);
        noiz_render.blink++;

        if (!noiz_render.mute)
            pbc_audio_frame(&noiz.audio);

        pbc_led_red(noiz.status == ST_GAME && noiz.left <= 0);

        t1 = time_us_64();
        noiz_render.ms_cpu = (int)((t1 - t0) / 1000u);

        push_frame();

        if (noiz.hiscore != saved && noiz.status == ST_TITLE) {
            int hs[2];
            hs[0] = noiz.hiscore;
            hs[1] = 0;
            if (pbc_store_save(hs))
                saved = noiz.hiscore;
        }

        noiz_render.ms_frame = (int)((time_us_64() - t0) / 1000u);

        /* Die Vorlage streckt den Bildabstand, wenn viele Geschosse im Bild
         * sind -- das ist Teil des Spielgefuehls und wird hier uebernommen.
         * Grundwert 16 entspricht rund 62 Bildern je Sekunde. */
        next += (uint64_t)noiz.interval * 1000u;
        {
            int64_t wait = (int64_t)(next - time_us_64());
            if (wait > 0)
                sleep_us((uint64_t)wait);
            else if (wait < -100000)
                next = time_us_64();
        }
    }
}
