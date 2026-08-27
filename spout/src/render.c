/* render.c -- Bildaufbau fuer das 240x280-Panel. */

#include <string.h>
#include "render.h"
#include "palette.h"
#include "color565.h"
#include "font.h"

spout_render_cfg_t spout_render;

/* Zellbyte -> zwei nebeneinanderliegende Panelpixel (Zoom 2 waagerecht) */
static uint32_t pal2[256];
static uint16_t col_bg, col_fg, col_dim, col_accent, col_warn, col_alarm, col_panel, col_ink, col_jet;

static uint16_t c565(uint32_t rgb) { return spout_rgb565(rgb); }

void spout_render_init(int dark)
{
    int i;
    spout_render.dark = dark;
    for (i = 0; i < 256; i++) {
        uint16_t p = c565(spout_cell_rgb((uint8_t)i, dark));
        pal2[i] = ((uint32_t)p << 16) | p;
    }
    col_bg     = c565(spout_cell_rgb(0, dark));
    col_panel  = c565(dark ? 0x101218u : 0x1A1D24u);
    col_fg     = c565(dark ? 0xE8E8E0u : 0xF2F2ECu);
    col_dim    = c565(dark ? 0x7A828Eu : 0x939BA7u);
    col_accent = c565(dark ? SPOUT_UI_ACCENT_D : 0x5AB0FFu);
    col_warn   = c565(0xFFB020u);
    col_alarm  = c565(0xFF4030u);
    col_ink    = c565(spout_cell_rgb(0x03, dark));   /* Titeltext wie fester Fels */
    col_jet    = c565(spout_ovl_rgb(0x02, dark));    /* Zielhilfe                 */
}

/* ---- Textablage ---------------------------------------------------------- */
static void t_at(uint16_t *row, int py, int x, int base,
                 const font_t *f, const char *s, uint16_t col)
{
    font_row(row, SCR_W, py, x, base, f, s, col);
}

static void t_centre(uint16_t *row, int py, int base,
                     const font_t *f, const char *s, uint16_t col)
{
    font_row(row, SCR_W, py, (SCR_W - font_text_w(f, s)) / 2, base, f, s, col);
}

static void t_right(uint16_t *row, int py, int rx, int base,
                    const font_t *f, const char *s, uint16_t col)
{
    font_row(row, SCR_W, py, rx - font_text_w(f, s), base, f, s, col);
}

/* ---- Zahlen ohne printf -------------------------------------------------- */
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

static void banners_row(uint16_t *row, int py);

/* ---- Zielhilfe: kantengeglaettete Punkte --------------------------------
 * Jeder Punkt ist ein Quadrat von zwei Bildpunkten Kantenlaenge, dessen Lage
 * auf 1/64 Bildpunkt genau bekannt ist.  Die Deckung eines Bildpunktes ist die
 * Ueberlappungsflaeche -- dieselbe Rechnung wie bei einem Kastenfilter, nur
 * von Hand, weil es hier um hoechstens zwoelf Punkte je Bild geht.
 */
static void dots_row(uint16_t *row, int py)
{
    int rowtop = py * 64, rowbot = rowtop + 64;
    int k;

    for (k = 0; k < spout.n_dot; k++) {
        int fx = spout.dot[k].x, fy = spout.dot[k].y;
        int y0 = fy - 64, y1 = fy + 64;
        int x0, x1, c, cs, ce, ovy;

        ovy = (y1 < rowbot ? y1 : rowbot) - (y0 > rowtop ? y0 : rowtop);
        if (ovy <= 0)
            continue;                      /* Punkt beruehrt diese Zeile nicht */

        x0 = fx - 64;
        x1 = fx + 64;
        cs = x0 >> 6;
        ce = (x1 - 1) >> 6;
        for (c = cs; c <= ce; c++) {
            int left = c * 64, right = left + 64;
            int ovx = (x1 < right ? x1 : right) - (x0 > left ? x0 : left);
            unsigned a;
            if (ovx <= 0 || c < 0 || c >= SCR_W)
                continue;
            a = (unsigned)(ovx * ovy) >> 4;          /* 0..4096 -> 0..256 */
            if (a > 255u)
                a = 255u;
            row[c] = pbc_blend565(row[c], col_jet, a);
        }
    }
}

/* ---- Spielfeldzeile ------------------------------------------------------ */
static void playfield_row(uint16_t *row, int py)
{
    const uint8_t *src = spout.cells + (((spout.disp_pos + (py / ZOOM)) & ROW_MASK) * CELL_W + VIEW_X0);
    uint32_t *d = (uint32_t *)(void *)row;
    int i;

    for (i = 0; i < VIEW_W; i++)
        d[i] = pal2[src[i]];

    banners_row(row, py);
    dots_row(row, py);

    /* Schiff liegt ueber allem -- es markiert die Zelle, in der es stirbt */
    {
        int cy = py / ZOOM, k;
        for (k = 0; k < spout.n_ovl; k++) {
            if (spout.ovl[k].y == (uint8_t)cy) {
                uint16_t c = c565(spout_ovl_rgb(spout.ovl[k].c, spout_render.dark));
                int x = spout.ovl[k].x * ZOOM;
                row[x] = c;
                row[x + 1] = c;
            }
        }
    }
}

/* ---- Titelbloecke -------------------------------------------------------
 * Sie wandern mit dem Ring durchs Bild, liegen aber nicht darin: so laesst
 * sich der Text mit voller Kantenglaettung zeichnen statt in Zellrastern.
 */
static void banner_line(uint16_t *row, int py, int base,
                        const font_t *f, const char *s, uint16_t col)
{
    if (py + f->ascent < base || py >= base + 10)
        return;                            /* Zeile beruehrt diesen Block nicht */
    t_centre(row, py, base, f, s, col);
}

static void banners_row(uint16_t *row, int py)
{
    int k;

    if (spout.phase != PH_TITLE && spout.phase != PH_TITLE_INIT)
        return;

    for (k = 0; k < SPOUT_MAX_BANNER; k++) {
        const spout_banner_t *b = &spout.banner[k];
        char s[40];
        int sr, top;

        if (!b->used)
            continue;
        sr = (b->row - spout.disp_pos) & ROW_MASK;
        if (sr > RING_H / 2)
            sr -= RING_H;                  /* Block liegt oberhalb des Bildes */
        if (sr >= VIEW_H || sr < -56)
            continue;
        top = sr * ZOOM;

        switch (b->slot) {
        case 0:
            banner_line(row, py, top + 50, &font_ui_xl, "SPOUT", col_ink);
            banner_line(row, py, top + 80, &font_ui_m, "CAVEFLYER", col_ink);
            break;
        case 3:
            banner_line(row, py, top + 22, &font_ui_m, "B - THRUST", col_ink);
            banner_line(row, py, top + 52, &font_ui_m, "A - PAUSE", col_ink);
            break;
        case 2:
            s[0] = 0; cat(s, "BEST ");
            cat_num(s, (unsigned)(spout.hiscore[0] % 1000000), 1, ' ');
            banner_line(row, py, top + 22, &font_ui_m, s, col_ink);
            s[0] = 0; cat(s, "HEIGHT ");
            cat_num(s, (unsigned)(spout.hiscore[1] % 100000), 1, ' ');
            banner_line(row, py, top + 52, &font_ui_m, s, col_ink);
            break;
        default:
            banner_line(row, py, top + 18, &font_ui_s, "KUNI 2002", col_ink);
            banner_line(row, py, top + 38, &font_ui_s, "N.WHITE 2010 MIT", col_ink);
            banner_line(row, py, top + 58, &font_ui_s, "PBC+ PORT", col_ink);
            break;
        }
    }
}

/* ---- Meldungsfeld (PAUSE / GAME OVER) ----------------------------------- */
static void message_row(uint16_t *row, int py)
{
    int h = spout.msg2[0] ? 64 : 44;
    int y0 = PF_H / 2 - h / 2;
    int w, x0, x1;

    if (!spout.msg[0] || py < y0 || py >= y0 + h)
        return;

    w = font_text_w(&font_ui_l, spout.msg) + 28;
    if (spout.msg2[0]) {
        int w2 = font_text_w(&font_ui_s, spout.msg2) + 28;
        if (w2 > w)
            w = w2;
    }
    x0 = (SCR_W - w) / 2;
    x1 = x0 + w;
    if (x0 < 0) { x0 = 0; x1 = SCR_W; }
    {
        int x, edge = (py == y0 || py == y0 + h - 1);
        for (x = x0; x < x1; x++)
            row[x] = edge ? col_accent : col_panel;
        if (!edge) {
            row[x0] = col_accent;
            row[x1 - 1] = col_accent;
        }
    }
    t_centre(row, py, y0 + 32, &font_ui_l, spout.msg, col_fg);
    if (spout.msg2[0])
        t_centre(row, py, y0 + 54, &font_ui_s, spout.msg2, col_dim);
}

/* ---- Debugzeile ---------------------------------------------------------- */
static void debug_row(uint16_t *row, int py)
{
    char s[40];
    if (!spout_render.show_debug || py >= 16)
        return;
    { int x; for (x = 0; x < 150 && x < SCR_W; x++) row[x] = col_panel; }
    s[0] = 0;
    cat(s, "MS "); cat_num(s, (unsigned)spout_render.ms_frame, 2, '0');
    cat(s, "  CPU "); cat_num(s, (unsigned)spout_render.ms_cpu, 2, '0');
    cat(s, "  N "); cat_num(s, (unsigned)spout.n_grain, 3, '0');
    t_at(row, py, 6, 13, &font_ui_s, s, col_accent);
}

/* ---- Anzeigeleiste ------------------------------------------------------- */
#define HUD_B1 (PF_H + 17)                 /* Grundlinie obere Zeile        */
#define HUD_B2 (PF_H + 35)                 /* Grundlinie untere Zeile       */
#define HUD_L   20                         /* linker sicherer Rand          */
#define HUD_R  220                         /* rechter sicherer Rand         */
#define HUD_GAP  8

static void hud_row(uint16_t *row, int py)
{
    char s[40];
    int x;

    for (x = 0; x < SCR_W; x++)
        row[x] = col_panel;
    if (py == PF_H) {
        for (x = 0; x < SCR_W; x++)
            row[x] = col_accent;
        return;
    }

    if (spout.phase == PH_TITLE || spout.phase == PH_TITLE_INIT) {
        if ((spout_render.blink & 32) == 0)
            t_centre(row, py, HUD_B1, &font_ui_m, "A OR B = START", col_fg);
        if (spout_render.mute)
            t_at(row, py, HUD_L, HUD_B1, &font_ui_s, "MUTE", col_warn);
        s[0] = 0;
        cat(s, "BEST ");
        cat_num(s, (unsigned)(spout.hiscore[0] % 1000000), 1, ' ');
        cat(s, "   HEIGHT ");
        cat_num(s, (unsigned)(spout.hiscore[1] % 100000), 1, ' ');
        t_centre(row, py, HUD_B2, &font_ui_s, s, col_dim);
        return;
    }

    {
        int secs = (spout.timeleft + FRAMERATE - 1) / FRAMERATE;
        uint16_t tc = col_fg;
        int lw;

        if (secs < 0) secs = 0;
        if (secs <= 5) tc = ((spout_render.blink & 8) && spout.gameover == 0) ? col_alarm : col_warn;
        else if (secs <= 10) tc = col_warn;

        lw = font_text_w(&font_ui_s, "TIME");
        t_at(row, py, HUD_L, HUD_B1, &font_ui_s, "TIME", col_dim);
        s[0] = 0; num(s, (unsigned)secs, 1, ' ');
        t_at(row, py, HUD_L + lw + HUD_GAP, HUD_B1, &font_ui_m, s, tc);

        s[0] = 0; num(s, (unsigned)(spout.height > 0 ? spout.height % 100000 : 0), 1, ' ');
        t_right(row, py, HUD_R, HUD_B1, &font_ui_m, s, col_fg);
        t_right(row, py, HUD_R - font_text_w(&font_ui_m, s) - HUD_GAP, HUD_B1,
                &font_ui_s, "HEIGHT", col_dim);

        lw = font_text_w(&font_ui_s, "SCORE");
        t_at(row, py, HUD_L, HUD_B2, &font_ui_s, "SCORE", col_dim);
        s[0] = 0; num(s, (unsigned)(spout.dispscore % 1000000), 1, ' ');
        t_at(row, py, HUD_L + lw + HUD_GAP, HUD_B2, &font_ui_m, s, col_accent);
    }
}

/* ---- oeffentlich --------------------------------------------------------- */
void spout_render_rows(uint16_t *dst, int y0, int rows)
{
    int r;
    for (r = 0; r < rows; r++) {
        int py = y0 + r;
        uint16_t *row = dst + (size_t)r * SCR_W;

        if (py < 0 || py >= SCR_H) {
            int x;
            for (x = 0; x < SCR_W; x++) row[x] = col_bg;
            continue;
        }
        if (py < PF_H) {
            playfield_row(row, py);
            message_row(row, py);
            debug_row(row, py);
        } else {
            hud_row(row, py);
        }
    }
}
