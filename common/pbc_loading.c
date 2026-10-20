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
static void render(int y0, int y1, void (*draw)(void))
{
    uint16_t strip[GFX_W * STRIP];
    for (int y = y0; y < y1; y += STRIP) {
        int h = y1 - y < STRIP ? y1 - y : STRIP;
        gfx_fb = (uint16_t *)((uintptr_t)strip - (uintptr_t)y * GFX_W * sizeof *strip);
        gfx_clip(0, y, GFX_W, y + h);
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
