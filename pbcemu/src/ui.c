/* ui.c -- see ui.h
 *
 * Everything is redrawn into the frame buffer every frame (gfx.c), so
 * sliding, fading and smooth scrolling come for free. One frame takes about
 * 30 ms to reach the panel; that is the pace of all animations here.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "pico/stdlib.h"
#include "gfx.h"
#include "input.h"
#include "devices.h"
#include "games.h"
#include "prefs.h"
#include "ui.h"
#include "c64/settings.h"
#include "c64/d64_dir.h"
#include "ff.h"

/* ---- look ------------------------------------------------------------------ */

#define BG_TOP     0x161d31
#define BG_BOTTOM  0x06080e
#define CARD       0x1d2539
#define TEXT       0xf2f4f8
#define DIM        0x8d96ab
#define FAINT      0x485168

#define ROW_H      30
#define LIST_Y     70
#define LIST_ROWS  6

static void background(void)
{
    gfx_background(BG_TOP, BG_BOTTOM);
}

/* A button symbol (filled disc with its letter) followed by a label.
 * Returns the x after it. */
static int hint(int x, int y, const char *key, const char *label)
{
    gfx_circle(x + 7, y + 7, 7, 0xd5dae6, 255);
    if (key[0] == '*')                  /* the centre button: a dot */
        gfx_circle(x + 7, y + 7, 3, 0x1a2030, 255);
    else
        gfx_text_centered(&font_small, x + 7, y + 1, key, 0x1a2030, 255);
    x += 18;
    x += gfx_text(&font_small, x, y + 1, label, DIM, 255);
    return x + 14;
}

static void footer(const char *const *keys, const char *const *labels, int n)
{
    int w = 0;
    for (int i = 0; i < n; i++)
        w += 18 + gfx_text_w(&font_small, labels[i]) + 14;
    w -= 14;
    int x = (GFX_W - w) / 2;
    for (int i = 0; i < n; i++)
        x = hint(x, GFX_H - 20, keys[i], labels[i]);
}

/* ease-out: t 0..256 -> 0..256 */
static int ease(int t)
{
    int u = 256 - t;
    return 256 - (u * u * u >> 16);
}

void ui_wait_release(void)
{
    while (input_raw())
        sleep_ms(10);
    sleep_ms(30);
    input_menu_edges();             /* forget what was pressed */
}

/* ---- device carousel ------------------------------------------------------------ */

static int counts[16];

static void count_text(int d, char *out, size_t n)
{
    if (devices[d].app == APP_NONE)
        snprintf(out, n, "Copy & eject");
    else if (counts[d] == 0)
        snprintf(out, n, "No games yet");
    else
        snprintf(out, n, counts[d] == 1 ? "1 game" : "%d games", counts[d]);
}

static void device_card(int d, int xoff, int alpha)
{
    const device_t *dev = &devices[d];
    int cx = GFX_W / 2 + xoff;
    gfx_picture(dev->pic, cx - dev->pic->w / 2, 38, alpha);
    gfx_text_centered(&font_title, cx, 170, dev->name, TEXT, alpha);
    gfx_text_centered(&font_small, cx, 200, dev->maker, DIM, alpha);

    char c[24];
    count_text(d, c, sizeof c);
    int w = gfx_text_w(&font_small, c) + 26;
    gfx_rrect(cx - w / 2, 218, w, 20, 10, dev->accent, alpha * 90 / 255);
    gfx_text_centered(&font_small, cx, 222, c, TEXT, alpha);
}

static void device_screen(int sel, int next, int t, int dir)
{
    background();
    uint32_t accent = gfx_mix(devices[sel].accent, devices[next].accent, t * 255 / 256);
    gfx_glow(GFX_W / 2, 104, 130, 78, accent, 80);

    /* title: PBC in white, emu in the accent */
    int x = 14 + gfx_text(&font_head, 14, 9, "PBC", TEXT, 255);
    gfx_text(&font_head, x, 9, "emu", accent, 255);
    char page[16];
    snprintf(page, sizeof page, "%d / %d", (t < 128 ? sel : next) + 1, device_count);
    gfx_text(&font_small, GFX_W - 14 - gfx_text_w(&font_small, page), 14, page, DIM, 255);

    int e = ease(t);
    device_card(sel, -dir * e * GFX_W / 256, 255 - e * 200 / 256);
    if (t > 0)
        device_card(next, dir * (256 - e) * GFX_W / 256, 55 + e * 200 / 256);

    gfx_chevron(12, 104, 7, -1, TEXT, 110);
    gfx_chevron(GFX_W - 12, 104, 7, 1, TEXT, 110);

    /* page dots, the current one stretched */
    int cur = t < 128 ? sel : next;
    int dots_w = (device_count - 1) * 12 + 18;
    x = (GFX_W - dots_w) / 2;
    for (int i = 0; i < device_count; i++) {
        if (i == cur) {
            gfx_rrect(x, 248, 18, 6, 3, accent, 255);
            x += 24;
        } else {
            gfx_circle(x + 3, 251, 3, FAINT, 255);
            x += 12;
        }
    }
    static const char *const keys[] = { "B", "*" };
    static const char *const labels[] = { "Open", "Settings" };
    footer(keys, labels, 2);
}

/* ---- options ----------------------------------------------------------------------- */

/* Rows of the options card; which ones appear depends on where it opens. */
enum { ROW_SOUND, ROW_VOLUME, ROW_DISPLAY, ROW_STATS, ROW_PORT, ROW_PROGRAM, ROW_KEY, ROW_DRIVE };

typedef struct {
    int device;                     /* -1: no device (no display row) */
    const char *game;               /* C64 game: joystick port and program too */
    int  port;
    char (*files)[D64_NAME_MAX + 1];
    int  nfiles, file;
    int  key;                       /* on Center + B, C64_KEYS index */
    bool drive;                     /* processor-level 1541 */
    bool drive_rom;                 /* ... possible: 1541.rom is on the drive */
    int  rows[8], nrows;
} opt_t;

#define C64_FILES_MAX 64
#define OPT_ROW_H     32            /* fewer pixels when more rows than fit */

/* PETSCII file name -> something the font can show. */
static void petscii_to_ascii(const char *in, char *out)
{
    for (; *in; in++, out++) {
        unsigned char c = (unsigned char)*in;
        if (c >= 0xC1 && c <= 0xDA) c = (unsigned char)(c - 0x80);
        *out = (c >= 0x20 && c <= 0x5F) ? (char)c : '?';
    }
    *out = 0;
}

static void switch_toggle(int x, int y, bool on, uint32_t accent)
{
    gfx_rrect(x, y, 38, 20, 10, on ? accent : 0x3a4258, 255);
    gfx_circle(on ? x + 28 : x + 10, y + 10, 8, 0xffffff, 255);
}

static void volume_bars(int x, int y, int vol, bool on, uint32_t accent)
{
    for (int i = 0; i < VOLUME_MAX; i++) {
        int h = 5 + i;
        gfx_rrect(x + i * 7, y + 17 - h, 5, h, 2, (i < vol && on) ? accent : 0x3a4258, 255);
    }
}

static void value_arrows(int x_right, int y, const char *s)
{
    int w = gfx_text_w(&font_body, s);
    gfx_chevron(x_right - w - 16, y + 9, 4, -1, DIM, 255);
    gfx_text(&font_body, x_right - w - 6, y + 1, s, TEXT, 255);
    gfx_chevron(x_right + 4, y + 9, 4, 1, DIM, 255);
}

static void key_chips(int x, int y, const char *keys);
static bool c64_drive_rom(void);

static int mode_count(int device)
{
    int n = 0;
    while (devices[device].modes && devices[device].modes[n])
        n++;
    return n;
}

static void options_screen(void (*under)(void *), void *arg, const char *title,
                           opt_t *o, int row, uint32_t accent)
{
    under(arg);
    gfx_rect(0, 0, GFX_W, GFX_H, 0x000000, 150);
    int rh = OPT_ROW_H;
    if (56 + o->nrows * rh + 30 > GFX_H - 8)
        rh = (GFX_H - 8 - 86) / o->nrows;
    int c = (rh - 4) / 2;               /* middle of a row's highlight */
    int h = 56 + o->nrows * rh + 30, y0 = (GFX_H - h) / 2;
    gfx_rrect(14, y0, GFX_W - 28, h, 16, CARD, 255);
    char t[GAME_NAME_MAX + 4];
    gfx_text_fit(&font_head, title, GFX_W - 60, t);
    gfx_text(&font_head, 30, y0 + 14, t, TEXT, 255);

    static const char *const names[] = { "Sound", "Volume", "Display", "Show FPS", "Joystick", "Program",
                                         "", "True drive" };
    static const char *const key_names[] = { C64_KEYS(C64_KEY_NAME) };
    for (int i = 0; i < o->nrows; i++) {
        int y = y0 + 48 + i * rh, kind = o->rows[i];
        if (i == row)
            gfx_rrect(22, y, GFX_W - 44, rh - 4, 9, accent, 55);
        y += c - 14;                    /* the items below are placed for a 32 pixel row */
        bool off = kind == ROW_DRIVE && !o->drive_rom;   /* nothing to switch */
        gfx_text(&font_body, 34, y + 5, names[kind], off ? FAINT : i == row ? TEXT : DIM, 255);
        int xr = GFX_W - 34;
        char v[32];
        switch (kind) {
        case ROW_SOUND: switch_toggle(xr - 38, y + 4, g_prefs.sound, accent); break;
        case ROW_VOLUME: volume_bars(xr - VOLUME_MAX * 7 + 2, y + 5, g_prefs.volume, g_prefs.sound, accent); break;
        case ROW_DISPLAY:
            value_arrows(xr - 8, y + 4, devices[o->device].modes[prefs_video(o->device)]);
            break;
        case ROW_STATS: switch_toggle(xr - 38, y + 4, g_prefs.stats, accent); break;
        case ROW_PORT:
            snprintf(v, sizeof v, "Port %d", o->port);
            value_arrows(xr - 8, y + 4, v);
            break;
        case ROW_PROGRAM: {
            char a[D64_NAME_MAX + 1];
            if (!o->files || o->file == 0)
                strcpy(a, "First");         /* LOAD"*" */
            else
                petscii_to_ascii(o->files[o->file], a);
            gfx_text_fit(&font_body, a, 96, v);
            value_arrows(xr - 8, y + 4, v);
            break;
        }
        case ROW_KEY:
            key_chips(34, y + 7, "Center + B");     /* as on the start screen */
            value_arrows(xr - 8, y + 4, key_names[o->key]);
            break;
        case ROW_DRIVE:
            if (o->drive_rom) {
                switch_toggle(xr - 38, y + 4, o->drive, accent);
            } else {
                const char *t = "needs 1541.rom";
                gfx_text(&font_small, xr - gfx_text_w(&font_small, t), y + 8, t, FAINT, 255);
            }
            break;
        }
    }
    gfx_text_centered(&font_small, GFX_W / 2, y0 + h - 24, "\xae \xaf  change      B  done", DIM, 255);
    gfx_present();
}

/* Sound, volume, the device's display mode and the performance overlay
 * (for a C64 game also its port and program). Saved when closed: the only
 * place where any of it changes, never during a game. */
static void options(void (*under)(void *), void *arg, int device, const char *game, uint32_t accent)
{
    opt_t o = { .device = device };
    o.rows[o.nrows++] = ROW_SOUND;
    o.rows[o.nrows++] = ROW_VOLUME;
    if (device >= 0 && mode_count(device) > 1)
        o.rows[o.nrows++] = ROW_DISPLAY;
    if (device >= 0 && devices[device].app != APP_NONE)
        o.rows[o.nrows++] = ROW_STATS;
    bool c64 = game && devices[device].app == APP_C64;
    if (c64) {
        o.rows[o.nrows++] = ROW_PORT;
        o.rows[o.nrows++] = ROW_PROGRAM;
        o.rows[o.nrows++] = ROW_KEY;
        o.rows[o.nrows++] = ROW_DRIVE;
        settings_load();
        o.game = game;
        o.port = settings_port(game);
        o.key = settings_key(game);
        o.drive_rom = c64_drive_rom();
        o.drive = settings_true_drive(game) && o.drive_rom;
        o.files = malloc(C64_FILES_MAX * sizeof *o.files);
        o.nfiles = 1;
        if (o.files) {
            strcpy(o.files[0], "*");
            size_t n = strlen(game);
            if (n > 4 && strcasecmp(game + n - 4, ".d64") == 0) {
                char path[GAME_NAME_MAX + 2], chosen[SETTINGS_FILE_MAX + 1];
                snprintf(path, sizeof path, "/%.*s", GAME_NAME_MAX - 1, game);
                o.nfiles += d64_list_prg(path, o.files + 1, C64_FILES_MAX - 1);
                settings_file(game, chosen);
                for (int i = 1; chosen[0] && i < o.nfiles; i++)
                    if (strcmp(o.files[i], chosen) == 0)
                        o.file = i;
            }
        }
    }
    char title[GAME_NAME_MAX], tags[GAME_NAME_MAX];
    if (game)
        games_title(game, title, tags, sizeof title);
    else if (device >= 0)
        snprintf(title, sizeof title, "%s", devices[device].name);
    else
        strcpy(title, "Settings");

    int row = 0;
    ui_wait_release();
    for (;;) {
        options_screen(under, arg, title, &o, row, accent);
        uint32_t e = input_menu_edges();
        int d = (e & IN_RIGHT) ? 1 : (e & IN_LEFT) ? -1 : 0;
        if (e & IN_UP)   row = (row + o.nrows - 1) % o.nrows;
        if (e & IN_DOWN) row = (row + 1) % o.nrows;
        if (d) {
            switch (o.rows[row]) {
            case ROW_SOUND: g_prefs.sound = !g_prefs.sound; break;
            case ROW_VOLUME:
                g_prefs.volume += d;
                if (g_prefs.volume < VOLUME_MIN) g_prefs.volume = VOLUME_MIN;
                if (g_prefs.volume > VOLUME_MAX) g_prefs.volume = VOLUME_MAX;
                g_prefs.sound = true;
                break;
            case ROW_DISPLAY: {
                int n = mode_count(device);
                prefs_set_video(device, (prefs_video(device) + n + d) % n);
                break;
            }
            case ROW_STATS: g_prefs.stats = !g_prefs.stats; break;
            case ROW_PORT: o.port = o.port == 1 ? 2 : 1; break;
            case ROW_PROGRAM: o.file = (o.file + o.nfiles + d) % o.nfiles; break;
            case ROW_KEY: o.key = (o.key + C64_KEY_COUNT + d) % C64_KEY_COUNT; break;
            case ROW_DRIVE: o.drive = o.drive_rom && !o.drive; break;
            }
        }
        if (e & (IN_A | IN_B | IN_CENTER))
            break;
    }
    prefs_save();
    if (c64) {
        settings_set_port(game, o.port);
        settings_set_file(game, o.file ? o.files[o.file] : "");
        settings_set_key(game, o.key);
        if (o.drive_rom)                /* without the ROM the choice stays as it was */
            settings_set_true_drive(game, o.drive);
        settings_save();
        free(o.files);
    }
    ui_wait_release();
}

/* ---- game list ------------------------------------------------------------------------ */

typedef struct {
    int device;
    char (*names)[GAME_NAME_MAX];
    int n, cur;
    int scroll;                     /* in 1/16 pixel, follows the cursor smoothly */
    uint64_t cur_since;             /* when the cursor got there: long names scroll */
    int fade;                       /* 0..256 when entering */
} list_t;

static void list_screen(void *arg)
{
    list_t *l = arg;
    const device_t *dev = &devices[l->device];
    background();

    /* header: the accent fading into the background */
    for (int y = 0; y < 66; y++)
        gfx_rect(0, y, GFX_W, 1, dev->accent, 70 - y);
    gfx_picture(dev->icon, 10, 10, l->fade);
    gfx_text(&font_head, 90, 13, dev->name, TEXT, 255);
    char c[24];
    count_text(l->device, c, sizeof c);
    gfx_text(&font_small, 90, 37, c, DIM, 255);
    gfx_rect(0, 64, GFX_W, 1, 0xffffff, 22);

    if (l->n == 0) {
        gfx_text_centered(&font_body, GFX_W / 2, 128, "No games for this system", TEXT, 255);
        gfx_text_centered(&font_small, GFX_W / 2, 152, "Copy them onto the drive with", DIM, 255);
        gfx_text_centered(&font_small, GFX_W / 2, 166, "\"Add Games\" (last page).", DIM, 255);
    } else {
        int top = l->scroll / 16;
        gfx_clip(0, LIST_Y - 2, GFX_W, LIST_Y + LIST_ROWS * ROW_H + 2);
        int first = top / ROW_H;
        for (int i = first; i < l->n && i <= first + LIST_ROWS; i++) {
            int y = LIST_Y + i * ROW_H - top;
            bool sel = i == l->cur;
            if (sel) {
                gfx_rrect(8, y + 1, GFX_W - 20, ROW_H - 2, 9, dev->accent, 70);
                gfx_rrect(8, y + 6, 4, ROW_H - 12, 2, dev->accent, 255);
            }
            char title[GAME_NAME_MAX], tags[GAME_NAME_MAX], fit[GAME_NAME_MAX + 4];
            games_title(l->names[i], title, tags, sizeof title);
            int max_w = GFX_W - 44;
            int full = gfx_text_w(&font_body, title) + (tags[0] ? 6 + gfx_text_w(&font_small, tags) : 0);
            if (sel && full > max_w) {
                /* too long: the whole name runs through the row */
                int x = 20 - gfx_marquee(full, max_w, (uint32_t)((time_us_64() - l->cur_since) / 1000));
                gfx_clip(20, LIST_Y - 2, 20 + max_w, LIST_Y + LIST_ROWS * ROW_H + 2);
                x += gfx_text(&font_body, x, y + 6, title, TEXT, 255);
                if (tags[0])
                    gfx_text(&font_small, x + 6, y + 9, tags, DIM, 255);
                gfx_clip(0, LIST_Y - 2, GFX_W, LIST_Y + LIST_ROWS * ROW_H + 2);
                continue;
            }
            gfx_text_fit(&font_body, title, max_w, fit);
            int w = gfx_text(&font_body, 20, y + 6, fit, sel ? TEXT : 0xc3c9d6, 255);
            if (tags[0] && w + 12 < max_w) {
                gfx_text_fit(&font_small, tags, max_w - w - 6, fit);
                gfx_text(&font_small, 20 + w + 6, y + 9, fit, sel ? DIM : FAINT, 255);
            }
        }
        gfx_noclip();
        if (l->n > LIST_ROWS) {
            int track = LIST_ROWS * ROW_H - 8;
            int th = track * LIST_ROWS / l->n;
            if (th < 16) th = 16;
            int ty = LIST_Y + 4 + (track - th) * top / ((l->n - LIST_ROWS) * ROW_H);
            gfx_rrect(GFX_W - 7, LIST_Y + 4, 3, track, 1, 0xffffff, 18);
            gfx_rrect(GFX_W - 7, ty, 3, th, 1, dev->accent, 255);
        }
    }

    gfx_rect(0, GFX_H - 28, GFX_W, 1, 0xffffff, 22);
    static const char *const keys[] = { "B", "A", "*" };
    static const char *const labels[] = { "Play", "Back", "Options" };
    footer(keys, labels, 3);
}

/* Returns true with the chosen game in `game`, false for "back". */
static bool game_list(int device, char game[GAME_NAME_MAX])
{
    list_t l = { .device = device };
    l.n = games_list(device, &l.names);
    for (int i = 0; game[0] && i < l.n; i++)
        if (strcmp(l.names[i], game) == 0)
            l.cur = i;
    int target_top = 0;
    if (l.cur >= LIST_ROWS)
        target_top = (l.cur - LIST_ROWS + 1) * ROW_H;
    l.scroll = target_top * 16;
    l.cur_since = time_us_64();

    ui_wait_release();
    for (;;) {
        l.fade = l.fade + 48 > 256 ? 256 : l.fade + 48;
        int want = l.scroll / 16;
        if (l.cur * ROW_H < want) want = l.cur * ROW_H;
        if ((l.cur + 1) * ROW_H > want + LIST_ROWS * ROW_H) want = (l.cur + 1 - LIST_ROWS) * ROW_H;
        l.scroll += (want * 16 - l.scroll) / 2;
        if (abs(want * 16 - l.scroll) < 16) l.scroll = want * 16;

        list_screen(&l);
        gfx_present();

        uint32_t e = input_menu_edges();
        if (e & IN_A)
            return false;
        if (l.n == 0)
            continue;
        if (e & IN_UP)    l.cur = (l.cur + l.n - 1) % l.n;
        if (e & IN_DOWN)  l.cur = (l.cur + 1) % l.n;
        if (e & IN_LEFT)  l.cur = l.cur >= LIST_ROWS ? l.cur - LIST_ROWS : 0;
        if (e & IN_RIGHT) l.cur = l.cur + LIST_ROWS < l.n ? l.cur + LIST_ROWS : l.n - 1;
        if (e & (IN_UP | IN_DOWN | IN_LEFT | IN_RIGHT))
            l.cur_since = time_us_64();
        if (e & IN_CENTER)
            options(list_screen, &l, device, l.names[l.cur], devices[device].accent);
        if (e & IN_B) {
            strcpy(game, l.names[l.cur]);
            return true;
        }
    }
}

/* ---- main loop ---------------------------------------------------------------------------- */

typedef struct { int sel; } carousel_t;

static void device_still(void *arg)
{
    int sel = ((carousel_t *)arg)->sel;
    device_screen(sel, sel, 0, 1);
}

ui_result_t ui_run(int *device, char game[GAME_NAME_MAX])
{
    games_count(counts);
    int sel = (*device >= 0 && *device < device_count) ? *device : 1;
    int next = sel, dir = 1, t = 0;

    ui_wait_release();
    for (;;) {
        device_screen(sel, next, t, dir);
        gfx_present();
        if (next != sel) {
            t += 48;
            if (t >= 256) {
                sel = next;
                t = 0;
            }
        }
        uint32_t e = input_menu_edges();
        if (e & (IN_LEFT | IN_RIGHT)) {
            if (next != sel)                /* still sliding: finish it at once */
                sel = next;
            dir = (e & IN_RIGHT) ? 1 : -1;
            next = (sel + dir + device_count) % device_count;
            t = 0;
        }
        if (next != sel)
            continue;
        if (e & IN_CENTER) {
            carousel_t d = { sel };
            options(device_still, &d, devices[sel].app == APP_NONE ? -1 : sel, NULL, devices[sel].accent);
        }
        if (e & IN_B) {
            *device = sel;
            if (devices[sel].app == APP_NONE)
                return UI_USB;
            if (game_list(sel, game))
                return UI_PLAY;
            game[0] = 0;
            ui_wait_release();
        }
    }
}

/* ---- other screens ---------------------------------------------------------------------------- */

void ui_message(const char *title, const char *l1, const char *l2, const char *l3)
{
    background();
    int h = 150, y0 = (GFX_H - h) / 2;
    gfx_rrect(14, y0, GFX_W - 28, h, 16, CARD, 255);
    gfx_text_centered(&font_head, GFX_W / 2, y0 + 18, title, TEXT, 255);
    const char *lines[] = { l1, l2, l3 };
    for (int i = 0; i < 3; i++)
        if (lines[i])
            gfx_text_centered(&font_small, GFX_W / 2, y0 + 54 + i * 18, lines[i], DIM, 255);
    gfx_text_centered(&font_small, GFX_W / 2, y0 + h - 26, "Press a button", FAINT, 255);
    gfx_present();
    ui_wait_release();
    while (!input_raw())
        sleep_ms(20);
    ui_wait_release();
}

/* True if the player's own Commodore ROMs are on the drive (the C64
 * emulator checks them properly; this is only for the controls card). */
static bool c64_own_roms(void)
{
    static FILINFO fi;
    return f_stat("/kernal.rom", &fi) == FR_OK && fi.fsize == 8192 &&
           f_stat("/basic.rom", &fi) == FR_OK && fi.fsize == 8192;
}

/* The 1541's ROM for "True drive", by the names c64/src/c64_roms.c tries. */
static bool c64_drive_rom(void)
{
    static const char *const names[] = { "/1541.rom", "/1541.bin", "/dos1541.rom", "/1541" };
    static FILINFO fi;
    for (unsigned i = 0; i < sizeof names / sizeof names[0]; i++)
        if (f_stat(names[i], &fi) == FR_OK && fi.fsize == 16384)
            return true;
    return false;
}

/* "Center + B" -> the same button symbols as in the footer; other words
 * (Joystick ...) stay text. */
static void key_chips(int x, int y, const char *keys)
{
    char word[24];
    while (*keys) {
        int n = 0;
        while (keys[n] && keys[n] != ' ' && n < (int)sizeof word - 1) {
            word[n] = keys[n];
            n++;
        }
        word[n] = 0;
        keys += n;
        while (*keys == ' ')
            keys++;
        if (!strcmp(word, "A") || !strcmp(word, "B") || !strcmp(word, "Center")) {
            gfx_circle(x + 7, y + 7, 7, 0xd5dae6, 255);
            if (word[0] == 'C')
                gfx_circle(x + 7, y + 7, 3, 0x1a2030, 255);
            else
                gfx_text_centered(&font_small, x + 7, y + 1, word, 0x1a2030, 255);
            x += 20;
        } else {
            x += gfx_text(&font_body, x, y - 2, word, 0xd5dae6, 255) + 6;
        }
    }
}

static void starting_screen(int device, const char *title, const char *const *rows_b,
                            const char *const *rows_k, int rows, const char *note, int alpha)
{
    const device_t *dev = &devices[device];
    background();
    for (int y = 0; y < 66; y++)
        gfx_rect(0, y, GFX_W, 1, dev->accent, 70 - y);
    gfx_picture(dev->icon, 10, 10, 255);
    gfx_text(&font_head, 90, 13, title, TEXT, 255);
    gfx_text(&font_small, 90, 37, dev->name, DIM, 255);
    gfx_rect(0, 64, GFX_W, 1, 0xffffff, 22);

    gfx_text(&font_small, 22, 80, "CONTROLS", FAINT, 255);
    int step = note && rows > 4 ? 24 : 28;    /* the note must stay above the footer */
    for (int i = 0; i < rows; i++) {
        int y = 100 + i * step;
        int w = gfx_text_w(&font_small, rows_b[i]) + 16;
        if (w < 34) w = 34;
        gfx_rrect(22, y, w, 20, 10, dev->accent, 90);
        gfx_text_centered(&font_small, 22 + w / 2, y + 4, rows_b[i], TEXT, 255);
        key_chips(96, y + 3, rows_k[i]);
    }
    if (note)
        gfx_text(&font_small, 22, 100 + rows * step + 4, note, DIM, 255);
    gfx_text_centered(&font_small, GFX_W / 2, GFX_H - 38, "Back to this list: RESET", FAINT, 255);
    if (alpha > 0) {
        int w = 120;
        gfx_rrect((GFX_W - w) / 2, GFX_H - 20, w * alpha / 255, 4, 2, dev->accent, 255);
    }
}

void ui_starting(int device, const char *game)
{
    const device_t *dev = &devices[device];
    char title[GAME_NAME_MAX], tags[GAME_NAME_MAX], fit[GAME_NAME_MAX + 4];
    games_title(game, title, tags, sizeof title);
    gfx_text_fit(&font_head, title, GFX_W - 104, fit);

    const char *b[8], *k[8];
    int n = 0;
    char port[24], note[40] = "";
    for (const control_t *c = dev->controls; c->button && n < 8; c++, n++) {
        b[n] = c->button;
        k[n] = c->key;
    }
    if (dev->app == APP_C64) {
        static const char *const key_names[] = { C64_KEYS(C64_KEY_NAME) };
        snprintf(port, sizeof port, "Joystick, port %d", settings_port(game));
        k[0] = port;
        b[3] = key_names[settings_key(game)];      /* the key on Center + B */
        snprintf(note, sizeof note, "ROMs: %s%s", c64_own_roms() ? "Commodore" : "OpenROMs",
                 settings_true_drive(game) && c64_drive_rom() ? ", true drive" : "");
    }

    /* about two seconds, or until B is pressed */
    ui_wait_release();
    for (int t = 0; t <= 255; t += 4) {
        starting_screen(device, fit, b, k, n, note[0] ? note : NULL, t);
        gfx_present();
        if (input_menu_edges() & IN_B)
            break;
    }
    gfx_finish();
}

void ui_usb(int state)
{
    static const char *const states[] = { "Connect the USB cable", "Connected", "Drive mounted on the PC" };
    const device_t *dev = &devices[device_count - 1];
    background();
    gfx_glow(GFX_W / 2, 100, 130, 78, dev->accent, 80);
    gfx_picture(dev->pic, (GFX_W - dev->pic->w) / 2, 36, 255);
    gfx_text_centered(&font_title, GFX_W / 2, 170, "USB Drive", TEXT, 255);
    gfx_text_centered(&font_small, GFX_W / 2, 202, "Copy games onto PBC-ROMS,", DIM, 255);
    gfx_text_centered(&font_small, GFX_W / 2, 217, "then eject the drive.", DIM, 255);
    int w = gfx_text_w(&font_small, states[state]) + 26;
    gfx_rrect((GFX_W - w) / 2, 240, w, 20, 10, state == 2 ? 0x3ccf7a : dev->accent, 110);
    gfx_text_centered(&font_small, GFX_W / 2, 244, states[state], TEXT, 255);
    gfx_present();
}
