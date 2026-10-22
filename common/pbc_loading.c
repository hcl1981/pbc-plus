/* pbc_loading.c -- see pbc_loading.h */
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "gfx.h"
#include "display.h"
#include "pbc_launch.h"
#include "pbc_devices.h"
#include "pbc_loading.h"

/* as pbcemu/src/ui.c */
#define BG_TOP     0x161d31
#define BG_BOTTOM  0x06080e
#define CARD       0x1d2539
#define TEXT       0xf2f4f8
#define DIM        0x8d96ab
#define FAINT      0x485168

#define FADE_STEPS 25                   /* x 10 ms */
#define STRIP      2                    /* rows drawn at a time, on the stack */
#define BAR_X      30
#define BAR_Y      142
#define BAR_W      (GFX_W - 2 * BAR_X)
#define BAR_H      8

extern uint16_t *gfx_fb;                /* gfx.c, built with GFX_STRIP */

typedef struct { const char *name; uint32_t accent; const pbc_picture_t *icon; } look_t;
#define LOOK(name, maker, exts, app, accent, pic, icon, controls, modes) { name, accent, &icon },
static const look_t looks[] = { PBC_DEVICES(LOOK) };

static const look_t *look(void)
{
    const pbc_launch_t *l = pbc_launch_get();
    int n = (int)(sizeof looks / sizeof looks[0]);
    return &looks[l && l->device < n ? l->device : n - 1];
}

/* Runs draw() once per strip of rows y0..y1 and sends each strip. The
 * strip lives on the stack: no RAM is kept from the game for these screens. */
static int strip_y0, strip_y1;          /* the rows being drawn, for a narrower clip */

static void render(int y0, int y1, void (*draw)(void))
{
    uint16_t strip[GFX_W * STRIP];
    for (int y = y0; y < y1; y += STRIP) {
        int h = y1 - y < STRIP ? y1 - y : STRIP;
        gfx_fb = (uint16_t *)((uintptr_t)strip - (uintptr_t)y * GFX_W * sizeof *strip);
        gfx_clip(0, y, GFX_W, y + h);
        strip_y0 = y;
        strip_y1 = y + h;
        draw();
        pbc_display_window(0, y, GFX_W, h);
        pbc_display_begin();
        pbc_display_send(strip, GFX_W * h);
        pbc_display_wait();
        pbc_display_end();
    }
    gfx_noclip();
}

/* ---- backlight ---------------------------------------------------------------- */

static repeating_timer_t fade_timer;
static int fade_step = -1;              /* -1: not started */

static bool fade_tick(repeating_timer_t *t)
{
    (void)t;
    fade_step++;
    pbc_display_backlight((uint8_t)(PBC_BACKLIGHT * fade_step / FADE_STEPS));
    return fade_step < FADE_STEPS;
}

void pbc_fade_in(bool wait)
{
    if (fade_step >= 0)
        return;
    fade_step = 0;
    if (wait) {
        while (fade_tick(NULL))
            sleep_ms(10);
    } else if (!add_repeating_timer_ms(10, fade_tick, NULL, &fade_timer)) {
        pbc_display_backlight(PBC_BACKLIGHT);
    }
}

/* ---- loading ----------------------------------------------------------------- */

/* The texts are on the stack of the call that renders them, as the strip. */
static const char *game_title;          /* NULL: only the bar is redrawn */
static int percent;

static void loading_screen(void)
{
    const look_t *d = look();
    gfx_background(BG_TOP, BG_BOTTOM);
    for (int y = 0; y < 66; y++)
        gfx_rect(0, y, GFX_W, 1, d->accent, 70 - y);
    gfx_picture(d->icon, 10, 10, 255);
    if (game_title)
        gfx_text(&font_head, 90, 13, game_title, TEXT, 255);
    gfx_text(&font_small, 90, 37, d->name, DIM, 255);
    gfx_rect(0, 64, GFX_W, 1, 0xffffff, 22);

    gfx_text_centered(&font_head, GFX_W / 2, 104, "Loading game", TEXT, 255);
    gfx_rrect(BAR_X, BAR_Y, BAR_W, BAR_H, BAR_H / 2, 0xffffff, 24);
    if (percent > 0)
        gfx_rrect(BAR_X, BAR_Y, BAR_W * percent / 100, BAR_H, BAR_H / 2, d->accent, 255);
    char p[8];
    snprintf(p, sizeof p, "%d %%", percent);
    gfx_text_centered(&font_small, GFX_W / 2, BAR_Y + 16, p, DIM, 255);

    gfx_text_centered(&font_small, GFX_W / 2, 212, "Only needed the first time", FAINT, 255);
    gfx_text_centered(&font_small, GFX_W / 2, 228, "a game is started.", FAINT, 255);
}

void pbc_loading_begin(const char *game)
{
    char name[PBC_LAUNCH_NAME_MAX], tags[PBC_LAUNCH_NAME_MAX], fit[PBC_LAUNCH_NAME_MAX + 4];
    pbc_game_title(game, name, tags, sizeof name);
    gfx_text_fit(&font_head, name, GFX_W - 104, fit);
    percent = 0;
    game_title = fit;
    render(0, GFX_H, loading_screen);
    game_title = NULL;
    pbc_fade_in(true);
}

void pbc_loading_progress(uint32_t done, uint32_t total)
{
    int p = total ? (int)((uint64_t)done * 100 / total) : 0;
    if (p == percent)
        return;
    percent = p;
    render(BAR_Y - 2, BAR_Y + 16 + font_small.height, loading_screen);
}

/* ---- messages ------------------------------------------------------------------ */

#define MSG_CHARS 44                    /* more does not fit the card anyway */

static const char *msg[4];

/* as ui_message() in pbcemu/src/ui.c */
static void message_screen(void)
{
    gfx_background(BG_TOP, BG_BOTTOM);
    int h = 150, y0 = (GFX_H - h) / 2;
    gfx_rrect(14, y0, GFX_W - 28, h, 16, CARD, 255);
    gfx_text_centered(&font_head, GFX_W / 2, y0 + 18, msg[0], TEXT, 255);
    for (int i = 1; i < 3; i++)
        gfx_text_centered(&font_small, GFX_W / 2, y0 + 36 + i * 18, msg[i], DIM, 255);
    gfx_text_centered(&font_small, GFX_W / 2, y0 + h - 26, msg[3], FAINT, 255);
}

void pbc_loading_message(const char *title, const char *l1, const char *l2, const char *l3)
{
    const char *in[4] = { title, l1, l2, l3 };
    char fit[4][MSG_CHARS + 4];
    for (int i = 0; i < 4; i++) {
        char s[MSG_CHARS];
        snprintf(s, sizeof s, "%s", in[i] ? in[i] : "");
        gfx_text_fit(i ? &font_small : &font_head, s, GFX_W - 48, fit[i]);
        msg[i] = fit[i];
    }
    render(0, GFX_H, message_screen);
    pbc_fade_in(true);
}

/* ---- list ------------------------------------------------------------------------- */

#define LIST_VIS   7                    /* rows on the card at most */

static const char *const *list_rows;
static int list_n, list_sel, list_mark, list_top;
static const char *list_title, *list_note, *list_footer;
static uint64_t list_since;             /* when `sel` got there: a long name scrolls */
static int list_x;                      /* how far it has scrolled */
static int list_sel_y, list_sel_h;      /* where the ticks redraw it */

/* as the options card and the game list in pbcemu/src/ui.c */
static void list_screen(void)
{
    const look_t *d = look();
    int vis = list_n < LIST_VIS ? list_n : LIST_VIS;
    int rh = vis > 6 ? 26 : 30;
    int c = (rh - 4) / 2;
    int h = 56 + vis * rh + 30, y0 = (GFX_H - h) / 2;
    gfx_background(BG_TOP, BG_BOTTOM);
    gfx_rrect(14, y0, GFX_W - 28, h, 16, CARD, 255);
    if (list_title)                     /* NULL in the ticks: not in their rows */
        gfx_text(&font_head, 30, y0 + 14, list_title, TEXT, 255);
    int note_w = list_note ? gfx_text_w(&font_small, list_note) + 8 : 0;
    for (int k = 0; k < vis; k++) {
        int i = list_top + k, y = y0 + 48 + k * rh;
        bool sel = i == list_sel;
        int avail = GFX_W - 72 - (i == list_mark ? note_w : 0);
        if (sel) {
            gfx_rrect(22, y, GFX_W - 48, rh - 4, 9, d->accent, 55);
            list_sel_y = y;
            list_sel_h = rh - 4;
        }
        if (sel && gfx_text_w(&font_body, list_rows[i]) > avail) {
            gfx_clip(34, strip_y0, 34 + avail, strip_y1);
            gfx_text(&font_body, 34 - list_x, y + c - 9, list_rows[i], TEXT, 255);
            gfx_clip(0, strip_y0, GFX_W, strip_y1);
        } else {
            char fit[PBC_LAUNCH_NAME_MAX + 4];
            gfx_text_fit(&font_body, list_rows[i], avail, fit);
            gfx_text(&font_body, 34, y + c - 9, fit, sel ? TEXT : DIM, 255);
        }
        if (i == list_mark && list_note)
            gfx_text(&font_small, GFX_W - 38 - note_w + 8, y + c - 6, list_note, FAINT, 255);
    }
    if (list_n > vis) {                 /* scroll bar */
        int track = vis * rh - 4, ty0 = y0 + 48;
        int th = track * vis / list_n;
        if (th < 12) th = 12;
        int ty = ty0 + (track - th) * list_top / (list_n - vis);
        gfx_rrect(GFX_W - 23, ty0, 3, track, 1, 0xffffff, 18);
        gfx_rrect(GFX_W - 23, ty, 3, th, 1, d->accent, 255);
    }
    gfx_text_centered(&font_small, GFX_W / 2, y0 + h - 24, list_footer, DIM, 255);
}

void pbc_loading_list(const char *title, const char *const *rows, int n, int sel,
                      int mark, const char *note, const char *footer)
{
    char fit[PBC_LAUNCH_NAME_MAX + 4];
    gfx_text_fit(&font_head, title, GFX_W - 60, fit);
    if (sel != list_sel || rows != list_rows)
        list_since = time_us_64();
    list_title = fit;
    list_rows = rows;
    list_n = n;
    list_sel = sel;
    list_mark = mark;
    list_note = note;
    list_footer = footer;
    list_x = 0;
    /* the window follows the selection, as little as needed */
    int vis = n < LIST_VIS ? n : LIST_VIS;
    if (list_top > n - vis) list_top = n - vis;
    if (sel < list_top) list_top = sel;
    if (sel >= list_top + vis) list_top = sel - vis + 1;
    if (list_top < 0) list_top = 0;
    render(0, GFX_H, list_screen);
    list_title = NULL;
}

void pbc_loading_list_tick(void)
{
    if (!list_rows || list_sel < 0)
        return;
    int note_w = list_note && list_sel == list_mark ? gfx_text_w(&font_small, list_note) + 8 : 0;
    int x = gfx_marquee(gfx_text_w(&font_body, list_rows[list_sel]), GFX_W - 72 - note_w,
                        (uint32_t)((time_us_64() - list_since) / 1000));
    if (x == list_x)
        return;
    list_x = x;
    render(list_sel_y, list_sel_y + list_sel_h, list_screen);
}
