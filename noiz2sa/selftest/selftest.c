/* selftest.c -- geraeteseitige Testfirmware, ohne das Spiel.
 *
 * Beantwortet die Fragen, die sich sonst mit der Anwendung vermischen
 * (CLAUDE.md Abschnitt 10).  Eigenes UF2, weiterschalten mit der Joystickmitte:
 *
 *   1 GEOMETRIE  Rahmen ganz aussen, bei 10 px und bei 20 px.  Was fehlt,
 *                sagt genau, wieviel die runden Ecken schlucken.
 *   2 FARBEN     beschriftete Balken.  Steht unter "ROT" ein blauer Balken,
 *                ist die Bitfolge in src/color565.h falsch herum.
 *   3 TASTEN     alle sieben mit Zaehler.
 *   4 TON        A = Klang, B = Rauschen.
 *   5 TEMPO      der echte Renderpfad des Spiels; misst Bild- und Rechenzeit
 *                bei laufendem Kugelvorhang.
 *
 * Die Seiten 1 bis 4 werden nur bei Aenderung neu gezeichnet -- ein Vollbild
 * dauert ueber SPI rund 17 ms, jedes Bild neu zu malen ergaebe Flackern.
 */
#include <string.h>
#include "pico/stdlib.h"
#include "game.h"
#include "render.h"
#include "color565.h"
#include "segfont.h"
#include "pbc/board.h"
#include "pbc/display.h"
#include "pbc/input.h"
#include "pbc/audio.h"
#include "pbc/leds.h"

#define W PBC_LCD_W
#define H PBC_LCD_H

static uint16_t line[W];
static bool audio_ok;
static int page;
static uint32_t press_count[7];
static uint16_t held;

static void fill(uint16_t c) { int i; for (i = 0; i < W; i++) line[i] = c; }

static void box(int x0, int x1, uint16_t c)
{
    int x;
    if (x0 < 0) x0 = 0;
    if (x1 > W) x1 = W;
    for (x = x0; x < x1; x++) line[x] = c;
}

/* --- 1: Geometrie: erst innen, der aeussere Rahmen zuletzt --- */
static void page_geom(int y)
{
    fill(PBC_BLACK);
    if (y == H / 2) box(0, W, PBC_CYAN);
    line[W / 2] = PBC_CYAN;
    if (y == 20 || y == H - 21) box(20, W - 20, PBC_YELLOW);
    else if (y > 20 && y < H - 21) { line[20] = PBC_YELLOW; line[W - 21] = PBC_YELLOW; }
    if (y == 10 || y == H - 11) box(10, W - 10, PBC_MAGENTA);
    else if (y > 10 && y < H - 11) { line[10] = PBC_MAGENTA; line[W - 11] = PBC_MAGENTA; }
    if (y == 0 || y == H - 1) fill(PBC_WHITE);
    else { line[0] = PBC_WHITE; line[W - 1] = PBC_WHITE; }
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
    if (y >= 40 && y < 200) box(20, 120, bar_col[(y - 40) / 20]);
    if (y >= 228 && y < 252) {
        int i;
        for (i = 0; i < 26; i++) {
            unsigned v = (unsigned)(i * 255 / 25);
            box(20 + i * 8, 20 + i * 8 + 8, PBC_RGB(v, v, v));
        }
    }
}

static void page_colour_text(void)
{
    int i;
    for (i = 0; i < 8; i++)
        pbc_display_text(130, 44 + i * 20, &segfont_s, bar_name[i], PBC_WHITE, PBC_BLACK);
    pbc_display_text(20, 208, &segfont_s, "PASST FARBE NICHT ZUM NAMEN,", PBC_YELLOW, PBC_BLACK);
    pbc_display_text(20, 218, &segfont_s, "IST SRC/COLOR565.H FALSCH HERUM.", PBC_YELLOW, PBC_BLACK);
    pbc_display_text(20, 258, &segfont_s, "MITTE = WEITER", PBC_CYAN, PBC_BLACK);
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
        pbc_display_text(58, 40 + i * 30, &segfont_m, key_name[i], PBC_WHITE, PBC_BLACK);
        pbc_display_text(186, 40 + i * 30, &segfont_m, n, PBC_CYAN, PBC_BLACK);
    }
    pbc_display_text(20, 256, &segfont_s, "MITTE ZAEHLT MIT UND SCHALTET WEITER",
                     PBC_CYAN, PBC_BLACK);
}

/* --- 4: Ton --- */
static void page_audio(int y)
{
    fill(PBC_BLACK);
    if (y >= 60 && y < 84) box(20, W - 20, audio_ok ? PBC_GREEN : PBC_RED);
    if (y >= 140 && y < 164 && (held & PAD_A)) box(20, 110, PBC_YELLOW);
    if (y >= 140 && y < 164 && (held & PAD_B)) box(130, W - 20, PBC_YELLOW);
}

static void page_audio_text(void)
{
    pbc_display_text(20, 24, &segfont_m, "4 TON", PBC_WHITE, PBC_BLACK);
    pbc_display_text(28, 66, &segfont_m, audio_ok ? "TONPFAD LAEUFT" : "TONPFAD AUS",
                     PBC_BLACK, audio_ok ? PBC_GREEN : PBC_RED);
    pbc_display_text(20, 110, &segfont_s, "A = KLANG    B = RAUSCHEN", PBC_WHITE, PBC_BLACK);
    pbc_display_text(20, 200, &segfont_s, "NICHTS ZU HOEREN? DANN LIEGT DIE", PBC_WHITE, PBC_BLACK);
    pbc_display_text(20, 212, &segfont_s, "DMA AUF DER FALSCHEN CC-HAELFTE.", PBC_WHITE, PBC_BLACK);
    pbc_display_text(20, 256, &segfont_s, "MITTE = WEITER ZU 5 TEMPO", PBC_CYAN, PBC_BLACK);
}

static void page_geom_text(void)
{
    pbc_display_text(30, 108, &segfont_m, "1 GEOMETRIE", PBC_WHITE, PBC_BLACK);
    pbc_display_text(30, 134, &segfont_s, "WEISS   = RAND GANZ AUSSEN", PBC_WHITE, PBC_BLACK);
    pbc_display_text(30, 146, &segfont_s, "MAGENTA = 10 PX EINGERUECKT", PBC_MAGENTA, PBC_BLACK);
    pbc_display_text(30, 158, &segfont_s, "GELB    = 20 PX SICHERER RAND", PBC_YELLOW, PBC_BLACK);
    pbc_display_text(30, 176, &segfont_s, "MITTE = WEITER", PBC_CYAN, PBC_BLACK);
}

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
        for (i = 0; i < 7; i++) s = s * 31u + press_count[i];
    } else if (page == 3) {
        s ^= (uint32_t)(held & (PAD_A | PAD_B));
        s ^= audio_ok ? 0x8000u : 0u;
    }
    return s;
}

int main(void)
{
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

    noiz_init(0xA5A5A5A5u, 0);
    noiz_render_init();
    noiz_render.show_debug = 1;

    next = time_us_64();
    for (;;) {
        uint16_t pad = pbc_input_read();
        noiz_audio_t a;
        int i;

        held = (uint16_t)(pad & 0xffu);
        for (i = 0; i < 7; i++)
            if (pad & (uint16_t)(key_bit[i] << 8))
                press_count[i]++;

        if (pad & TRG_C) { page = (page + 1) % 5; have_drawn = false; }

        memset(&a, 0, sizeof a);

        if (page == 4) {
            /* Echter Renderpfad des Spiels, damit die Bildzeit gemessen wird
             * und nicht geschaetzt werden muss.  Das Spiel laeuft dabei mit
             * Dauerfeuer weiter, es geht um Last, nicht ums Spielen. */
            uint64_t t0 = time_us_64(), t1;
            int cur = 0, y;
            noiz_tick((uint16_t)(pad | PAD_B));
            noiz_render.blink++;
            a = noiz.audio;
            t1 = time_us_64();
            noiz_render.ms_cpu = (int)((t1 - t0) / 1000u);
            pbc_display_window(0, 0, SCR_W, SCR_H);
            pbc_display_begin();
            for (y = 0; y < SCR_H; y += 8) {
                noiz_render_rows(strip[cur], y, 8);
                pbc_display_wait();
                pbc_display_send(strip[cur], 8 * SCR_W);
                cur ^= 1;
            }
            pbc_display_end();
            noiz_render.ms_frame = (int)((time_us_64() - t0) / 1000u);
            have_drawn = false;
        } else {
            uint32_t sig = page_signature();
            if (!have_drawn || sig != last_sig) {
                draw_page();
                last_sig = sig;
                have_drawn = true;
            }
            if (page == 3) {
                if (held & PAD_B) a.bullets = 160;
                if (pad & TRG_A)  a.ev = EV_BOSS_DIE;
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
