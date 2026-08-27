/* selftest.c -- geraeteseitige Testfirmware, ohne das Spiel.
 *
 * Sie beantwortet die Fragen, die sich sonst mit der Anwendung vermischen
 * (CLAUDE.md Abschnitt 10).  Eigene UF2, eigenes Bild, weiterschalten mit
 * der Joystick-Mitte:
 *
 *   1 GEOMETRIE  Rahmen ganz aussen (weiss), bei 10 px (magenta) und bei
 *                20 px (gelb), dazu ein Mittenkreuz.  Was von welchem Rahmen
 *                fehlt, sagt genau, wieviel die runden Ecken schlucken.
 *   2 FARBEN     beschriftete Balken.  Steht unter "ROT" ein blauer Balken,
 *                ist die Bitfolge in src/color565.h falsch herum.
 *   3 TASTEN     alle sieben, mit Zaehler.  Zeigt auch, ob GP0/GP1 durch ein
 *                versehentlich aktiviertes UART belegt sind.
 *   4 TON        A = Klang, B = Rauschen.  Meldet, ob der Tonpfad startet.
 *   5 TEMPO      der echte Renderpfad des Spiels; misst Bild- und Rechenzeit.
 *
 * Die Seiten 1 bis 4 werden nur neu gezeichnet, wenn sich etwas geaendert hat
 * -- ein Vollbild dauert ueber SPI rund 17 ms, und wer danach jedes Mal den
 * Text darueberlegt, bekommt genau das Flackern, das man vermeiden will.
 */
#include <string.h>
#include "pico/stdlib.h"
#include "game.h"
#include "render.h"
#include "color565.h"
#include "font.h"
#include "pbc/board.h"
#include "pbc/display.h"
#include "pbc/input.h"
#include "pbc/audio.h"
#include "pbc/leds.h"

#define W PBC_LCD_W
#define H PBC_LCD_H

static uint16_t line[W];
static bool audio_ok;
static int  page;
static uint32_t press_count[7];
static uint16_t held;

static void fill(uint16_t c) { int i; for (i = 0; i < W; i++) line[i] = c; }

static void box(int x0, int x1, uint16_t c)
{
    int x;
    if (x0 < 0) x0 = 0;
    if (x1 > W) x1 = W;
    for (x = x0; x < x1; x++)
        line[x] = c;
}

/* --- 1: Geometrie -------------------------------------------------------
 * Reihenfolge ist wichtig: erst innen, der aeussere Rahmen zuletzt, damit ihn
 * nichts mehr unterbricht.  Was dann trotzdem fehlt, hat das Panel gefressen.
 */
static void page_geom(int y)
{
    fill(PBC_BLACK);

    if (y == H / 2) box(0, W, PBC_CYAN);            /* Mittenkreuz          */
    line[W / 2] = PBC_CYAN;

    if (y == 20 || y == H - 21)                     /* sicherer Rand 20 px  */
        box(20, W - 20, PBC_YELLOW);
    else if (y > 20 && y < H - 21) {
        line[20] = PBC_YELLOW;
        line[W - 21] = PBC_YELLOW;
    }

    if (y == 10 || y == H - 11)                     /* Zwischenmarke 10 px  */
        box(10, W - 10, PBC_MAGENTA);
    else if (y > 10 && y < H - 11) {
        line[10] = PBC_MAGENTA;
        line[W - 11] = PBC_MAGENTA;
    }

    if (y == 0 || y == H - 1)                       /* aeusserster Rand     */
        fill(PBC_WHITE);
    else {
        line[0] = PBC_WHITE;
        line[W - 1] = PBC_WHITE;
    }
}

/* --- 2: Farben --- */
static const uint16_t bar_col[8] = {
    PBC_RED, PBC_GREEN, PBC_BLUE, PBC_WHITE,
    PBC_YELLOW, PBC_CYAN, PBC_MAGENTA, PBC_GREY
};
static const char *bar_name[8] = {
    "ROT", "GRUEN", "BLAU", "WEISS", "GELB", "CYAN", "MAGENTA", "GRAU"
};

static void page_colour(int y)
{
    fill(PBC_BLACK);
    if (y >= 40 && y < 200)
        box(20, 120, bar_col[(y - 40) / 20]);
    if (y >= 228 && y < 252) {
        int i;
        for (i = 0; i < 26; i++) {                  /* Graukeil             */
            unsigned v = (unsigned)(i * 255 / 25);
            box(20 + i * 8, 20 + i * 8 + 8, PBC_RGB(v, v, v));
        }
    }
}

static void page_colour_text(void)
{
    int i;
    for (i = 0; i < 8; i++)
        pbc_display_text(130, 44 + i * 20, &font_ui_s, bar_name[i], PBC_WHITE, PBC_BLACK);
    pbc_display_text(20, 208, &font_ui_s, "PASST FARBE NICHT ZUM NAMEN,", PBC_YELLOW, PBC_BLACK);
    pbc_display_text(20, 218, &font_ui_s, "IST SRC/COLOR565.H FALSCH HERUM.", PBC_YELLOW, PBC_BLACK);
    pbc_display_text(20, 258, &font_ui_s, "MITTE = WEITER", PBC_CYAN, PBC_BLACK);
}

/* --- 3: Tasten --- */
static const char *key_name[7] = { "RECHTS", "LINKS", "RUNTER", "HOCH", "B", "A", "MITTE" };
static const uint16_t key_bit[7] = { PAD_RI, PAD_LF, PAD_DN, PAD_UP, PAD_B, PAD_A, PAD_C };

static void page_keys(int y)
{
    int i;
    fill(PBC_BLACK);
    for (i = 0; i < 7; i++) {
        int y0 = 36 + i * 30;
        if (y >= y0 && y < y0 + 22)
            box(20, 48, (held & key_bit[i]) ? PBC_GREEN : PBC_RGB(60, 60, 60));
    }
}

static void page_keys_text(void)
{
    char n[8];
    int i;
    for (i = 0; i < 7; i++) {
        unsigned v = press_count[i] % 1000u;
        n[0] = (char)('0' + v / 100u);
        n[1] = (char)('0' + (v / 10u) % 10u);
        n[2] = (char)('0' + v % 10u);
        n[3] = 0;
        pbc_display_text(58, 40 + i * 30, &font_ui_m, key_name[i], PBC_WHITE, PBC_BLACK);
        pbc_display_text(186, 40 + i * 30, &font_ui_m, n, PBC_CYAN, PBC_BLACK);
    }
    pbc_display_text(20, 256, &font_ui_s, "MITTE ZAEHLT MIT UND SCHALTET WEITER", PBC_CYAN, PBC_BLACK);
}

/* --- 4: Ton --- */
static void page_audio(int y)
{
    fill(PBC_BLACK);
    if (y >= 60 && y < 84)
        box(20, W - 20, audio_ok ? PBC_GREEN : PBC_RED);
    if (y >= 140 && y < 164 && (held & PAD_A))
        box(20, 110, PBC_YELLOW);
    if (y >= 140 && y < 164 && (held & PAD_B))
        box(130, W - 20, PBC_YELLOW);
}

static void page_audio_text(void)
{
    pbc_display_text(20, 24, &font_ui_m, "4 TON", PBC_WHITE, PBC_BLACK);
    pbc_display_text(28, 66, &font_ui_m, audio_ok ? "TONPFAD LAEUFT" : "TONPFAD AUS",
                     PBC_BLACK, audio_ok ? PBC_GREEN : PBC_RED);
    pbc_display_text(20, 110, &font_ui_s, "A = KLANG    B = RAUSCHEN", PBC_WHITE, PBC_BLACK);
    pbc_display_text(20, 200, &font_ui_s, "NICHTS ZU HOEREN? DANN LIEGT DIE", PBC_WHITE, PBC_BLACK);
    pbc_display_text(20, 212, &font_ui_s, "DMA AUF DER FALSCHEN CC-HAELFTE.", PBC_WHITE, PBC_BLACK);
    pbc_display_text(20, 256, &font_ui_s, "MITTE = WEITER ZU 5 TEMPO", PBC_CYAN, PBC_BLACK);
}

static void page_geom_text(void)
{
    pbc_display_text(30, 108, &font_ui_m, "1 GEOMETRIE", PBC_WHITE, PBC_BLACK);
    pbc_display_text(30, 134, &font_ui_s, "WEISS   = RAND GANZ AUSSEN", PBC_WHITE, PBC_BLACK);
    pbc_display_text(30, 146, &font_ui_s, "MAGENTA = 10 PX EINGERUECKT", PBC_MAGENTA, PBC_BLACK);
    pbc_display_text(30, 158, &font_ui_s, "GELB    = 20 PX SICHERER RAND", PBC_YELLOW, PBC_BLACK);
    pbc_display_text(30, 176, &font_ui_s, "MITTE = WEITER", PBC_CYAN, PBC_BLACK);
}

/* --- Vollbild einer statischen Seite --- */
static void draw_page(void)
{
    int y;
    pbc_display_window(0, 0, W, H);
    pbc_display_begin();
    for (y = 0; y < H; y++) {
        switch (page) {
        case 0: page_geom(y);   break;
        case 1: page_colour(y); break;
        case 2: page_keys(y);   break;
        default: page_audio(y); break;
        }
        pbc_display_wait();
        pbc_display_send(line, W);
    }
    pbc_display_end();

    switch (page) {
    case 0:  page_geom_text();   break;
    case 1:  page_colour_text(); break;
    case 2:  page_keys_text();   break;
    default: page_audio_text();  break;
    }
}

static uint32_t page_signature(void)
{
    uint32_t s = (uint32_t)page * 2654435761u;
    int i;
    if (page == 2) {
        s ^= held;
        for (i = 0; i < 7; i++)
            s = s * 31u + press_count[i];
    } else if (page == 3) {
        s ^= (uint32_t)(held & (PAD_A | PAD_B));
        s ^= audio_ok ? 0x8000u : 0u;
    }
    return s;
}

int main(void)
{
    int test_hiscore[2] = { 0, 0 };
    uint32_t last_sig = 0;
    bool have_drawn = false;
    uint64_t next;
    static uint16_t strip[2][W * 8];

    pbc_leds_init();
    pbc_input_init();
    pbc_display_init();
    pbc_led_green(true);
    pbc_display_backlight(255);

    pbc_led_yellow(true);
    audio_ok = pbc_audio_init();
    pbc_led_yellow(false);

    spout_init(0xA5A5A5A5u, test_hiscore);
    spout_render_init(0);
    spout_render.show_debug = 1;

    next = time_us_64();
    for (;;) {
        uint16_t pad = pbc_input_read();
        spout_audio_t a;
        int i;

        held = (uint16_t)(pad & 0xffu);
        for (i = 0; i < 7; i++)
            if (pad & (uint16_t)(key_bit[i] << 8))
                press_count[i]++;

        if (pad & TRG_C) {
            page = (page + 1) % 5;
            have_drawn = false;
        }

        memset(&a, 0, sizeof a);

        if (page == 4) {
            /* echter Renderpfad, damit die Bildzeit auf dem Geraet gemessen
             * wird und nicht geschaetzt werden muss */
            uint64_t t0 = time_us_64(), t1;
            int cur = 0, y;
            spout_tick((uint16_t)(pad | PAD_B));
            spout_render.blink++;
            a = spout.audio;
            t1 = time_us_64();
            spout_render.ms_cpu = (int)((t1 - t0) / 1000u);
            pbc_display_window(0, 0, SCR_W, SCR_H);
            pbc_display_begin();
            for (y = 0; y < SCR_H; y += 8) {
                spout_render_rows(strip[cur], y, 8);
                pbc_display_wait();
                pbc_display_send(strip[cur], 8 * SCR_W);
                cur ^= 1;
            }
            pbc_display_end();
            spout_render.ms_frame = (int)((time_us_64() - t0) / 1000u);
            have_drawn = false;
        } else {
            uint32_t sig = page_signature();
            if (!have_drawn || sig != last_sig) {
                draw_page();
                last_sig = sig;
                have_drawn = true;
            }
            if (page == 3) {
                if (held & PAD_B) { a.thrust = 1; a.hits = 10; }
                if (pad & TRG_A)  a.ev = SPOUT_EV_BONUS;
            }
        }

        if (audio_ok)
            pbc_audio_frame(&a);

        next += 20000u;
        {
            int64_t w = (int64_t)(next - time_us_64());
            if (w > 0) sleep_us((uint64_t)w); else next = time_us_64();
        }
    }
}
