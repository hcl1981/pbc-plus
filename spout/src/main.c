/* main.c -- Spout auf dem PicoBoy Color Plus.
 *
 * Ablauf beim Start ist bewusst so sortiert, dass ein Fehler sichtbar wird:
 * LEDs zuerst, dann Display, erst danach der Tonpfad.  Bleibt das Geraet mit
 * gelber LED stehen, war es die Audio-Initialisierung (CLAUDE.md Abschnitt 4);
 * bleibt es dunkel ohne LED, kam es nicht bis hinter die GPIO-Einrichtung.
 *
 * Beim Booten gehaltene Tasten (CLAUDE.md Abschnitt 9, "Optionen latchen"):
 *   B      Ton aus -- damit laesst sich ein haengender Tonpfad ausschliessen
 *   A      Debugzeile: Bildzeit, Rechenzeit, Zahl der Koerner
 *   UP     dunkles Farbschema
 */
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"

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

/* ---- HUD nur schicken, wenn sich etwas geaendert hat --------------------- */
static uint32_t hud_signature(void)
{
    uint32_t h = 2166136261u;
    int v[9], i;
    v[0] = spout.phase;
    v[1] = spout.gameover ? 1 : 0;
    v[2] = (spout.timeleft + FRAMERATE - 1) / FRAMERATE;
    v[3] = spout.height > 0 ? spout.height : 0;
    v[4] = spout.dispscore;
    v[5] = spout.hiscore[0];
    v[6] = spout.hiscore[1];
    v[7] = (spout_render.blink >> 3) & 1;      /* Alarmblinken               */
    v[8] = (spout_render.blink >> 5) & 1;      /* "PRESS A" blinkt           */
    for (i = 0; i < 9; i++) {
        h ^= (uint32_t)v[i];
        h *= 16777619u;
    }
    return h;
}

static void push_rows(int y0, int rows)
{
    int cur = 0, y;
    pbc_display_window(0, y0, SCR_W, rows);
    pbc_display_begin();
    for (y = y0; y < y0 + rows; y += STRIP_ROWS) {
        int n = (y + STRIP_ROWS <= y0 + rows) ? STRIP_ROWS : (y0 + rows - y);
        spout_render_rows(strip[cur], y, n);   /* der andere Puffer ist in der DMA */
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
    int saved[2];
    uint32_t hud_sig = 0;
    bool hud_valid = false;
    uint64_t next;
    int mute;

    pbc_leds_init();

    pbc_input_init();
    sleep_ms(20);                              /* Pullups einschwingen lassen */
    boot_keys = pbc_input_raw();
    mute = (boot_keys & PAD_B) ? 1 : 0;

    pbc_display_init();
    pbc_led_green(true);                       /* Display ist oben            */

    spout_render_init((boot_keys & PAD_UP) ? 1 : 0);
    spout_render.show_debug = (boot_keys & PAD_A) ? 1 : 0;
    spout_render.mute = mute;

    pbc_store_load(hiscore);
    saved[0] = hiscore[0];
    saved[1] = hiscore[1];
    spout_init((uint32_t)time_us_64() ^ 0x9E3779B9u, hiscore);

    /* Erst Bild aufbauen, dann Licht an -- kein Aufblitzen von altem Inhalt */
    spout_tick(0);
    push_rows(0, SCR_H);
    pbc_display_backlight(255);

    if (!mute) {
        pbc_led_yellow(true);                  /* bleibt an, falls es haengt   */
        if (pbc_audio_init())
            pbc_led_yellow(false);
        else
            spout_render.mute = 1;
    }

    next = time_us_64();
    for (;;) {
        uint64_t t0 = time_us_64(), t1;
        uint16_t pad = pbc_input_read();
        uint32_t sig;

        /* Beim Start eines Spiels neu wuerfeln: die Reaktionszeit des Spielers
         * ist die einzige brauchbare Entropiequelle auf diesem Geraet. */
        if (spout.phase == PH_TITLE && (pad & (TRG_A | TRG_B)))
            spout_reseed((uint32_t)time_us_64());

        spout_tick(pad);
        spout_render.blink++;

        if (!spout_render.mute)
            pbc_audio_frame(&spout.audio);

        pbc_led_red(spout.phase == PH_GAME && !spout.gameover &&
                    spout.timeleft <= 5 * FRAMERATE && (spout_render.blink & 8) != 0);

        t1 = time_us_64();
        spout_render.ms_cpu = (int)((t1 - t0) / 1000u);

        push_rows(0, PF_H);
        sig = hud_signature();
        if (!hud_valid || sig != hud_sig) {
            push_rows(PF_H, HUD_H);
            hud_sig = sig;
            hud_valid = true;
        }

        /* Bestwert sichern, sobald er im Titelbild festgeschrieben wurde */
        if (spout.phase == PH_TITLE &&
            (spout.hiscore[0] != saved[0] || spout.hiscore[1] != saved[1])) {
            if (pbc_store_save(spout.hiscore)) {
                saved[0] = spout.hiscore[0];
                saved[1] = spout.hiscore[1];
            }
            hud_valid = false;                 /* nach dem Flash neu zeichnen */
        }

        spout_render.ms_frame = (int)((time_us_64() - t0) / 1000u);

        next += 1000000u / FRAMERATE;
        {
            int64_t wait = (int64_t)(next - time_us_64());
            if (wait > 0)
                sleep_us((uint64_t)wait);
            else if (wait < -100000)
                next = time_us_64();           /* zu weit zurueck: nicht aufholen */
        }
    }
}
