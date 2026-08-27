/* render.c -- Indexpuffer -> Panelzeilen, dazu die Anzeige.
 *
 * Das Spielfeld fuellt das ganze Panel; die Anzeige liegt darueber, so wie die
 * Vorlage die Punkte in die obere linke Ecke legt.  Ein dunkles Feld darunter
 * haelt sie auch im dichtesten Kugelhagel lesbar.
 */
#include <string.h>
#include "render.h"
#include "palette.h"
#include "segfont.h"

noiz_render_cfg_t noiz_render;

static uint16_t pal[256];
static uint16_t col_fg, col_dim, col_accent, col_warn, col_plate, col_edge;

/* Helles Schema: die Vorlage spielt auf weissem Grund, also ist die Anzeige
 * dunkle Schrift auf einer aufhellenden Flaeche, nicht umgekehrt. */
void noiz_render_init(void)
{
    noiz_build_palette(pal);
    col_fg     = pbc_rgb24(0x14161Cu);     /* Tinte                         */
    col_dim    = pbc_rgb24(0x6A7280u);     /* Beschriftung                  */
    col_accent = pbc_rgb24(0x1038C0u);     /* Zahlenwerte                   */
    col_warn   = pbc_rgb24(0xC02010u);
    col_plate  = pbc_rgb24(0xFFFFFFu);     /* Flaeche unter der Anzeige     */
    col_edge   = pbc_rgb24(0x6A7280u);
}

/* ---- Text ---------------------------------------------------------------- */
/* y ist bei der Segmentschrift die OBERKANTE des Zeichenkastens. */
static void t_at(uint16_t *row, int py, int x, int y,
                 const segfont_t *f, const char *s, uint16_t col)
{
    segfont_row(row, SCR_W, py, x, y, f, s, col);
}

static void t_centre(uint16_t *row, int py, int y,
                     const segfont_t *f, const char *s, uint16_t col)
{
    segfont_row(row, SCR_W, py, (SCR_W - segfont_text_w(f, s)) / 2, y, f, s, col);
}

static void t_right(uint16_t *row, int py, int rx, int y,
                    const segfont_t *f, const char *s, uint16_t col)
{
    segfont_row(row, SCR_W, py, rx - segfont_text_w(f, s), y, f, s, col);
}

static void num(char *dst, unsigned v, int digits, char pad)
{
    char tmp[12];
    int n = 0;
    do { tmp[n++] = (char)('0' + v % 10u); v /= 10u; } while (v && n < 11);
    while (n < digits) tmp[n++] = pad;
    while (n--) *dst++ = tmp[n];
    *dst = 0;
}
static void cat(char *dst, const char *s) { while (*dst) dst++; while (*s) *dst++ = *s++; *dst = 0; }
static void cat_num(char *dst, unsigned v, int d, char p) { while (*dst) dst++; num(dst, v, d, p); }

/* ---- Feld hinter der Anzeige ---------------------------------------------
 * Die Vorlage hat links und rechts eigene Blenden neben dem Spielfeld.  Hier
 * fuellt das Feld das ganze Panel, also liegt die Anzeige darueber -- und darf
 * deshalb nicht deckend sein, sonst verschwinden Geschosse und Schiff
 * darunter.  Gemischt wird gegen den vorhandenen Bildinhalt. */
#define PLATE_ALPHA 200

static void plate(uint16_t *row, int py, int x0, int y0, int w, int h)
{
    int x;
    if (py < y0 || py >= y0 + h) return;
    if (x0 < 0) { w += x0; x0 = 0; }
    if (x0 + w > SCR_W) w = SCR_W - x0;
    if (w <= 0) return;
    if (py == y0 || py == y0 + h - 1) {
        for (x = x0; x < x0 + w; x++)
            row[x] = pbc_blend565(row[x], col_edge, PLATE_ALPHA);
        return;
    }
    for (x = x0; x < x0 + w; x++)
        row[x] = pbc_blend565(row[x], (x == x0 || x == x0 + w - 1) ? col_edge : col_plate,
                              PLATE_ALPHA);
}

/* ---- Anzeige ------------------------------------------------------------- */
#define HUD_L 18
#define HUD_R 222

static void hud_row(uint16_t *row, int py)
{
    char s[40];

    /* Punkte oben links.  Die Segmentschrift ist breit, deshalb sitzen die
     * Felder eng und die Szene unten rechts zeigt nur noch die Nummer -- die
     * Stufe steht im Titelbild. */
    plate(row, py, 14, 4, 134, 44);
    t_at(row, py, 20, 8, &segfont_s, "SCORE", col_dim);
    s[0] = 0; num(s, (unsigned)(noiz.score % 1000000), 1, ' ');
    t_at(row, py, 20, 24, &segfont_m, s, col_accent);

    /* Bonuswertung oben rechts */
    plate(row, py, 154, 4, 72, 44);
    t_at(row, py, 160, 8, &segfont_s, "BONUS", col_dim);
    s[0] = 0; num(s, (unsigned)noiz.bonus_score, 1, ' ');
    t_right(row, py, 220, 24, &segfont_s, s, col_fg);

    /* Restliche Schiffe unten links */
    plate(row, py, 14, SCR_H - 26, 100, 22);
    s[0] = 0; cat(s, "SHIPS ");
    cat_num(s, (unsigned)(noiz.left < 0 ? 0 : noiz.left), 1, ' ');
    t_at(row, py, 20, SCR_H - 22, &segfont_s, s, noiz.left > 0 ? col_dim : col_warn);

    /* Szene unten rechts */
    plate(row, py, 150, SCR_H - 26, 76, 22);
    s[0] = 0;
    cat_num(s, (unsigned)(noiz.scene < 0 ? 0 : noiz.scene), 1, ' ');
    t_right(row, py, 220, SCR_H - 22, &segfont_s, s, col_dim);
}

/* ---- Meldung ------------------------------------------------------------- */
static void message_row(uint16_t *row, int py)
{
    int h = noiz.msg2[0] ? 68 : 48;
    int y0 = SCR_H / 2 - h / 2;
    int w, x0;

    if (!noiz.msg[0] || py < y0 || py >= y0 + h) return;
    w = segfont_text_w(&segfont_l, noiz.msg) + 24;
    if (noiz.msg2[0]) {
        int w2 = segfont_text_w(&segfont_s, noiz.msg2) + 24;
        if (w2 > w) w = w2;
    }
    x0 = (SCR_W - w) / 2;
    plate(row, py, x0, y0, w, h);
    t_centre(row, py, y0 + 9, &segfont_l, noiz.msg, col_fg);
    if (noiz.msg2[0])
        t_centre(row, py, y0 + 46, &segfont_s, noiz.msg2, col_dim);
}

/* ---- Titelbild ----------------------------------------------------------- */
static const char *const stage_name[14] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "10",
    "ENDLESS EASY", "ENDLESS NORMAL", "ENDLESS HARD", "ENDLESS INSANE"
};

static void title_row(uint16_t *row, int py)
{
    char s[40];
    int y0 = 74;

    plate(row, py, 18, y0, 204, 116);
    t_centre(row, py, y0 + 8, &segfont_xl, "NOIZ", col_accent);
    t_centre(row, py, y0 + 58, &segfont_m, "2SA", col_fg);

    s[0] = 0;
    if (noiz.stage < 10) {
        cat(s, "STAGE ");
        cat(s, stage_name[noiz.stage]);
    } else {
        cat(s, stage_name[noiz.stage < 14 ? noiz.stage : 0]);
    }
    /* Die Namen der Endlosstufen sprengen die grosse Schrift -- dann eine
     * Nummer kleiner, statt ueber den sicheren Rand hinauszulaufen. */
    if (segfont_text_w(&segfont_m, s) <= 200)
        t_centre(row, py, y0 + 86, &segfont_m, s, col_warn);
    else
        t_centre(row, py, y0 + 90, &segfont_s, s, col_warn);

    if ((noiz_render.blink & 32) == 0) {
        plate(row, py, 24, 218, 192, 26);
        t_centre(row, py, 224, &segfont_s, "A OR B = START", col_fg);
    }
}

/* ---- Debugzeile ---------------------------------------------------------- */
static void debug_row(uint16_t *row, int py)
{
    char s[48];
    if (!noiz_render.show_debug || py >= 72 || py < 54) return;
    plate(row, py, 0, 54, 200, 18);
    s[0] = 0;
    cat(s, "MS "); cat_num(s, (unsigned)noiz_render.ms_frame, 2, '0');
    cat(s, " B "); cat_num(s, (unsigned)noiz.n_bullet, 3, '0');
    cat(s, " R "); cat_num(s, (unsigned)noiz.n_runner, 3, '0');
    t_at(row, py, 4, 56, &segfont_s, s, col_accent);
}

/* ---- oeffentlich --------------------------------------------------------- */
void noiz_render_rows(uint16_t *dst, int y0, int rows)
{
    int r;
    for (r = 0; r < rows; r++) {
        int py = y0 + r, x;
        uint16_t *row = dst + (size_t)r * SCR_W;
        const uint8_t *src;

        if (py < 0 || py >= SCR_H) {
            for (x = 0; x < SCR_W; x++) row[x] = 0;
            continue;
        }
        src = noiz.fb + (size_t)py * SCR_W;
        {
            const uint8_t *top = noiz.fb_top + (size_t)py * SCR_W;
            for (x = 0; x < SCR_W; x++)
                row[x] = pal[top[x] ? top[x] : src[x]];
        }

        if (noiz.status == ST_TITLE)
            title_row(row, py);
        else
            hud_row(row, py);
        message_row(row, py);
        debug_row(row, py);
    }
}
