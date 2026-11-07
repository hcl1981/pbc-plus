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
#include "favs.h"
#include "battery.h"
#include "display.h"
#include "sound.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "hardware/spi.h"
#include "board.h"
#include "pbc_lobby.h"
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

#define LINK_GREEN 0x3ccf7a
#define FAV_GOLD   0xe2b13c
#define LOW_YELLOW 0xf0c040
#define LOW_RED    0xe5484d

_Static_assert(LOBBY_NAME_MAX == GAME_NAME_MAX, "offers carry a file name");

int  ui_link_player;
bool ui_link_master;

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

/* A small picture from rows of '#', drawn as horizontal runs. */
static void tiny(const char *const *rows, int n, int x, int y, uint32_t rgb, int alpha)
{
    for (int r = 0; r < n; r++)
        for (int i = 0; rows[r][i];) {
            if (rows[r][i] != '#') {
                i++;
                continue;
            }
            int j = i;
            while (rows[r][j] == '#')
                j++;
            gfx_rect(x + i, y + r, j - i, 1, rgb, alpha);
            i = j;
        }
}

static const char *const star_rows[] = {
    "....#....", "...###...", "#########", ".#######.", "..#####..", ".###.###.", "##.....##",
};

static void star(int x, int y, uint32_t rgb, int alpha)
{
    tiny(star_rows, 7, x, y, rgb, alpha);
}

/* Top right: the batteries' charge in four bars, or a bolt and "USB" when
 * the PicoBoy runs from the cable. xr is the right edge. */
static void battery_icon(int xr, int y)
{
    static const char *const bolt[] = {
        "....##.", "...##..", "..##...", ".######", "....##.", "...##..", "..##...", ".##....", "#......",
    };
    battery_poll();
    if (battery_on_usb()) {
        int w = gfx_text_w(&font_small, "USB");
        gfx_text(&font_small, xr - w, y, "USB", DIM, 255);
        tiny(bolt, 9, xr - w - 10, y + 2, LOW_YELLOW, 255);
        return;
    }
    int bars = battery_bars();
    uint32_t c = bars >= 3 ? LINK_GREEN : bars == 2 ? LOW_YELLOW : LOW_RED;
    int x = xr - 24;
    gfx_rrect(x, y + 1, 22, 12, 3, bars ? DIM : LOW_RED, 255);
    gfx_rrect(x + 1, y + 2, 20, 10, 2, BG_TOP, 255);
    gfx_rect(xr - 2, y + 4, 2, 6, bars ? DIM : LOW_RED, 255);
    for (int i = 0; i < bars; i++)
        gfx_rect(x + 3 + i * 4, y + 4, 3, 6, c, 255);
}

void ui_wait_release(void)
{
    while (input_raw())
        sleep_ms(10);
    sleep_ms(30);
    input_menu_edges();             /* forget what was pressed */
}

/* ---- power saving ---------------------------------------------------------------- */

/* 30 s without a button: the display dims, after 2 more minutes it goes
 * dark, each in a second. A button brings the light back in 150 ms; it
 * does nothing else. */
#define DIM_AFTER_US  30000000u
#define OFF_AFTER_US 150000000u
#define FADE_DOWN_US   1000000u         /* for the whole range */
#define FADE_UP_US      150000u
#define CHASE_US       2500000u         /* dark: red, yellow, green every 2.5 s */
#define CHASE_STEP_US   250000u         /* from one LED to the next: they overlap */
#define CHASE_IN_US     150000u
#define CHASE_OUT_US    350000u
#define CHASE_PEAK         110          /* of 255, squared on the PWM */

static uint64_t last_press, bl_time;
static int idle;                    /* 0 awake, 1 dimmed, 2 dark */
static bool fading;
static int bl;                      /* the backlight during a fade, 1/256 steps */

static void backlight_tick(uint64_t now)
{
    int target = (idle == 2 ? 0 : idle == 1 ? prefs_backlight() / 3 : prefs_backlight()) << 8;
    uint64_t dt = now - bl_time;
    bl_time = now;
    if (!fading) {                  /* follows the brightness setting */
        bl = target;
        return;
    }
    int step = (int)(dt * (255u << 8) / (target > bl ? FADE_UP_US : FADE_DOWN_US));
    if (step < 1)
        step = 1;
    if (bl < target)
        bl = bl + step > target ? target : bl + step;
    else
        bl = bl - step < target ? target : bl - step;
    pbc_display_backlight((uint8_t)(bl >> 8));
    fading = bl != target;
}

/* Dark: nothing to draw. The clock goes down to 60 MHz (USB keeps its own
 * 48 MHz), and every 2.5 s the status LEDs run red, yellow, green, each
 * fading in and out and into the next, until a button or a link partner's game. The waking
 * button does nothing else. */
static void dark_wait(void)
{
    static const uint pins[3] = { PBC_PIN_LED_RED, PBC_PIN_LED_YELLOW, PBC_PIN_LED_GREEN };
    gfx_finish();                       /* the SPI clock changes with it */
    sound_off();                        /* GP14's PWM slice is the speaker's */
    set_sys_clock_khz(60000, true);
    pwm_config c = pwm_get_default_config();
    pwm_config_set_wrap(&c, 255 * 255);
    for (int k = 0; k < 3; k++) {
        pwm_init(pwm_gpio_to_slice_num(pins[k]), &c, true);
        pwm_set_gpio_level(pins[k], 0);
        gpio_set_function(pins[k], GPIO_FUNC_PWM);
    }
    uint64_t t0 = time_us_64();
    while (!input_raw() && !lobby_incoming(NULL)) {
        int t = (int)((time_us_64() - t0) % CHASE_US);
        for (int k = 0; k < 3; k++) {
            int dt = t - k * (int)CHASE_STEP_US, v = 0;
            if (dt >= 0 && dt < (int)CHASE_IN_US)
                v = dt * CHASE_PEAK / (int)CHASE_IN_US;
            else if (dt >= (int)CHASE_IN_US && dt < (int)(CHASE_IN_US + CHASE_OUT_US))
                v = CHASE_PEAK - (dt - (int)CHASE_IN_US) * CHASE_PEAK / (int)CHASE_OUT_US;
            pwm_set_gpio_level(pins[k], (uint16_t)(v * v));
        }
        sleep_ms(10);
    }
    for (int k = 0; k < 3; k++) {
        gpio_init(pins[k]);
        gpio_set_dir(pins[k], GPIO_OUT);
        gpio_put(pins[k], 0);
    }
    set_sys_clock_khz(150000, true);
    /* set_sys_clock_khz put clk_peri on the USB PLL (48 MHz); back on clk_sys
     * as at boot, with the SPI divider worked out again -- before, the
     * display ran at a third of its speed after waking up */
    uint32_t sys_hz = clock_get_hz(clk_sys);
    clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS, sys_hz, sys_hz);
    spi_set_baudrate(PBC_SPI, PBC_SPI_HZ);
    sound_on();
    while (input_raw())
        sleep_ms(10);
    input_menu_edges();                 /* forget the press */
    last_press = time_us_64();
    idle = 0;
    fading = true;
}

/* input_menu_edges() for the menus, with the power saving */
static uint32_t edges(void)
{
    uint32_t e = input_menu_edges();
    uint64_t now = time_us_64();
    if (!last_press)
        last_press = now;
    bool woken = false;
    if (e || input_raw()) {
        last_press = now;
        woken = idle != 0;
        if (woken) {
            idle = 0;
            fading = true;
        }
    } else {
        int want = now - last_press > OFF_AFTER_US ? 2 : now - last_press > DIM_AFTER_US ? 1 : 0;
        if (want != idle) {
            idle = want;
            fading = true;
        }
    }
    backlight_tick(now);
    if (idle == 2 && !fading) {
        dark_wait();
        return 0;
    }
    return woken ? 0 : e;
}

/* ---- device carousel ------------------------------------------------------------ */

static int counts[16];
static int fav_count;               /* favourites still on the drive */

/* The carousel: page 0 the favourites, then the devices. */
#define PAGES (device_count + 1)
#define FIRST_DEVICE 1

static uint32_t page_accent(int p)
{
    return p == 0 ? FAV_GOLD : devices[p - FIRST_DEVICE].accent;
}

static void games_text(int n, char *out, size_t size)
{
    snprintf(out, size, n == 1 ? "1 game" : "%d games", n);
}

static void count_text(int d, char *out, size_t n)
{
    if (devices[d].app == APP_NONE)
        snprintf(out, n, "Copy & eject");
    else if (counts[d] == 0)
        snprintf(out, n, "No games yet");
    else
        games_text(counts[d], out, n);
}

static void fav_text(int count, char *out, size_t n)
{
    if (count == 0)
        snprintf(out, n, "No favorites yet");
    else
        games_text(count, out, n);
}

/* A device picture over a soft round floor shadow (assets.c takes the
 * pictures' own contact shadows out and says where it goes). With `floats`
 * the device rises and sinks by 3 pixels every 3.2 s, in steps of a quarter
 * pixel (the next row drawn in part), and the shadow pales and narrows. */
#define FLOAT_MS 3200u
#define FLOAT_PX 3

/* returns how many whole pixels the device is up */
static int device_picture(const pbc_picture_t *pic, int x, int y, int alpha, bool floats)
{
    int e = 0;                              /* 0..256: how far up */
    if (floats) {
        int ms = (int)((uint32_t)(time_us_64() / 1000) % FLOAT_MS);
        int tri = (ms < (int)FLOAT_MS / 2 ? ms : (int)FLOAT_MS - ms) * 512 / (int)FLOAT_MS;
        e = tri * tri * (768 - 2 * tri) >> 16;       /* smoothstep */
    }
    const pbc_floor_t *fl = pbc_floor_of(pic);
    if (fl)
        gfx_glow(x + fl->cx, y + fl->cy, fl->rx * (2048 - e) / 2048 + 6, 8, 0x000000,
                 alpha * 150 / 255 * (512 - e) / 512);
    int pos = y * 256 - FLOAT_PX * e, yi = pos >> 8, f = (pos & 255) & ~63;
    gfx_picture(pic, x, yi, alpha);
    if (f)
        gfx_picture(pic, x, yi + 1, alpha * f / 256);
    return y - yi;
}

static void device_card(int p, int xoff, int alpha)
{
    int cx = GFX_W / 2 + xoff;
    char c[24];
    if (p == 0) {
        device_picture(&pic_fav, cx - pic_fav.w / 2, 38, alpha, true);
        gfx_text_centered(&font_title, cx, 170, "Favorites", TEXT, alpha);
        fav_text(fav_count, c, sizeof c);
        int w = gfx_text_w(&font_small, c) + 26;
        gfx_rrect(cx - w / 2, 218, w, 20, 10, FAV_GOLD, alpha * 90 / 255);
        gfx_text_centered(&font_small, cx, 222, c, TEXT, alpha);
        return;
    }
    int d = p - FIRST_DEVICE;
    const device_t *dev = &devices[d];
    int up = device_picture(dev->pic, cx - dev->pic->w / 2, 38, alpha, true);
    if (device_link(dev)) {
        /* playable by two over the link cable: a round golden badge on
         * the picture, like a seal */
        int bx = cx + dev->pic->w / 2 - 22, by = 38 + 19 - up;
        gfx_circle(bx, by + 1, 18, 0x000000, alpha * 70 / 255);         /* shadow */
        gfx_circle(bx, by, 18, 0x9c6f16, alpha);                          /* rim */
        gfx_circle(bx, by, 16, 0xe2b13c, alpha);                          /* gold */
        gfx_circle(bx - 3, by - 4, 9, 0xffe7a3, alpha * 70 / 255);       /* shine */
        gfx_text_centered(&font_small, bx, by - 6, "LINK", 0x3b2705, alpha);
    }
    gfx_text_centered(&font_title, cx, 170, dev->name, TEXT, alpha);
    gfx_text_centered(&font_small, cx, 200, dev->maker, DIM, alpha);

    count_text(d, c, sizeof c);
    int w = gfx_text_w(&font_small, c) + 26;
    gfx_rrect(cx - w / 2, 218, w, 20, 10, dev->accent, alpha * 90 / 255);
    gfx_text_centered(&font_small, cx, 222, c, TEXT, alpha);
}

static void device_screen(int sel, int next, int t, int dir)
{
    background();
    uint32_t accent = gfx_mix(page_accent(sel), page_accent(next), t * 255 / 256);
    gfx_glow(GFX_W / 2, 104, 130, 78, accent, 80);

    /* title: PBC in white, emu in the accent */
    int x = 14 + gfx_text(&font_head, 14, 9, "PBC", TEXT, 255);
    gfx_text(&font_head, x, 9, "emu", accent, 255);
    char page[28];
    snprintf(page, sizeof page, "%d / %d", (t < 128 ? sel : next) + 1, PAGES);
    gfx_text_centered(&font_small, GFX_W / 2, 14, page, DIM, 255);
    battery_icon(GFX_W - 14, 12);
    if (lobby_partner()) {
        const char *s = "Link connected";
        gfx_text(&font_small, GFX_W - 14 - gfx_text_w(&font_small, s), 28, s, LINK_GREEN, 255);
    }

    int e = ease(t);
    device_card(sel, -dir * e * GFX_W / 256, 255 - e * 200 / 256);
    if (t > 0)
        device_card(next, dir * (256 - e) * GFX_W / 256, 55 + e * 200 / 256);

    gfx_chevron(12, 104, 7, -1, TEXT, 110);
    gfx_chevron(GFX_W - 12, 104, 7, 1, TEXT, 110);

    /* page dots, the current one stretched */
    int cur = t < 128 ? sel : next;
    int dots_w = (PAGES - 1) * 12 + 18;
    x = (GFX_W - dots_w) / 2;
    for (int i = 0; i < PAGES; i++) {
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
enum { ROW_SOUND, ROW_VOLUME, ROW_DISPLAY, ROW_STATS, ROW_PORT, ROW_PROGRAM, ROW_KEY, ROW_DRIVE, ROW_FAV, ROW_BRIGHT,
       ROW_LAYOUT, ROW_JOYKIND, ROW_SWAP, ROW_PALETTE, ROW_DIFF_L, ROW_DIFF_R, ROW_BW, ROW_SCAN,
       ROW_CABLE, ROW_MACHINE };

/* A game's own options byte (c64/settings.h, settings_key_raw), handed to
 * its emulator in the launch record (pbc_launch.h, `key`): what it means
 * depends on the system. */
enum { RAW_NONE, RAW_CPC, RAW_ZX, RAW_MD, RAW_SWAP, RAW_GB, RAW_A26 };

typedef struct {
    int device;                     /* -1: no device (no display row) */
    const char *game;               /* C64 game: joystick port and program too */
    int  port;
    char (*files)[D64_NAME_MAX + 1];
    int  nfiles, file;
    int  key;                       /* on Center + B: C64_KEYS index, or the CPC's */
    int  raw_sys;                   /* RAW_*: what the game's own byte means */
    int  raw;                       /* ... and the byte */
    bool nes;                       /* RAW_SWAP: NES (A/B) or Sega (1/2) */
    bool drive;                     /* processor-level 1541 */
    bool drive_rom;                 /* ... possible: 1541.rom is on the drive */
    bool fav;                       /* the game is a favourite */
    bool zx48;                      /* ZX tape game: starts as a 48K (kept in `drive`) */
    int  rows[14], nrows;
} opt_t;

#define C64_FILES_MAX 64
#define OPT_ROW_H     32            /* 30 when the rows scroll */

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

static void level_bars(int x, int y, int n, int val, bool on, uint32_t accent)
{
    for (int i = 0; i < n; i++) {
        int h = 5 + i * 12 / n;
        gfx_rrect(x + i * 7, y + 17 - h, 5, h, 2, (i < val && on) ? accent : 0x3a4258, 255);
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

/* The CPC's keys for Center + B, in the order of cpc_keys_menu
 * (cpc/src/cpc_input.c): nine by name, F0-F9, 0-9, A-Z. */
#define CPC_KEYS 55

/* The Mega Drive buttons on PicoBoy B, A and Center, as megadrive/src/input.c */
#define MD_LAYOUTS 6
static const char md_layouts[MD_LAYOUTS][4] = { "ABC", "BAC", "ACB", "CAB", "BCA", "CBA" };

/* The Spectrum's keys for Center + B, in the order of zx_keys_menu
 * (zx/src/zx_input.c): six by name, 0-9, A-Z. */
#define ZX_KEYS 42
static const char *zx_key_name(int i)
{
    static const char *const named[] = { "ENTER", "SPACE", "BREAK", "DELETE", "CAPS SH", "SYM SH" };
    static char s[2];
    if (i < 6)
        return named[i];
    s[0] = (char)(i < 16 ? '0' + i - 6 : 'A' + i - 16);
    return s;
}

/* old Game Boy games: as gbc/src/main.c; "Auto" is the Super Game Boy's
 * colours for a game made for it, green for the others */
#define GB_PALETTES 8
static const char *const gb_palettes[GB_PALETTES] = { "Auto", "Grey", "Brown", "Red", "Blue", "Dark blue",
                                                      "Pastel", "Inverted" };
static const char *const cpc_joys[3] = { "Joystick", "Cursor keys", "QAOP" };
/* the Spectrum's joysticks, as zx_joy_names (zx/src/zx_input.c) */
static const char *const zx_joys[4] = { "Kempston", "Sinclair", "Cursor", "QAOP" };

/* where the game's byte keeps "buttons swapped" */
static int swap_bit(const opt_t *o)
{
    return o->raw_sys == RAW_GB ? 8 : 1;
}

/* the key on Center + B of a CPC or Spectrum game, from its byte */
static int raw_key(const opt_t *o)
{
    return (o->raw & 0x3f) % (o->raw_sys == RAW_CPC ? CPC_KEYS : ZX_KEYS);
}
static const char *cpc_key_name(int i)
{
    static const char *const named[] = { "RETURN", "SPACE", "ESC", "COPY", "DEL", "SHIFT",
                                         "CTRL", "TAB", "CAPS LOCK" };
    static char s[4];
    if (i < 9)
        return named[i];
    if (i < 19)
        snprintf(s, sizeof s, "F%d", i - 9);
    else if (i < 29)
        snprintf(s, sizeof s, "%d", i - 19);
    else
        snprintf(s, sizeof s, "%c", 'A' + i - 29);
    return s;
}

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
    /* Up to seven rows fit; more (a C64 game has ten) scroll, six at a
     * time, rather than getting squeezed. */
    int rh = OPT_ROW_H, vis = o->nrows;
    if (56 + vis * rh + 30 > GFX_H - 8) {
        rh = OPT_ROW_H - 2;
        vis = (GFX_H - 8 - 86) / rh;
    }
    static int top;                     /* first row shown */
    if (row < top) top = row;
    if (row >= top + vis) top = row - vis + 1;
    if (top > o->nrows - vis) top = o->nrows - vis;
    int c = (rh - 4) / 2;               /* middle of a row's highlight */
    int h = 56 + vis * rh + 30, y0 = (GFX_H - h) / 2;
    gfx_rrect(14, y0, GFX_W - 28, h, 16, CARD, 255);
    char t[GAME_NAME_MAX + 4];
    gfx_text_fit(&font_head, title, GFX_W - 60, t);
    gfx_text(&font_head, 30, y0 + 14, t, TEXT, 255);

    static const char *const names[] = { "Sound", "Volume", "Display", "Show FPS", "Joystick", "Program",
                                         "", "True drive", "Favorite", "Brightness", "", "Joystick", "",
                                         "Palette", "Left difficulty", "Right difficulty", "TV type",
                                         "Scanlines", "Link cable", "Machine" };
    static const char *const key_names[] = { C64_KEYS(C64_KEY_NAME) };
    if (vis < o->nrows) {               /* where in the rows: a thin bar on the right */
        int track = vis * rh - 4, th = track * vis / o->nrows;
        gfx_rrect(GFX_W - 21, y0 + 48, 3, track, 1, 0xffffff, 18);
        gfx_rrect(GFX_W - 21, y0 + 48 + (track - th) * top / (o->nrows - vis), 3, th, 1, accent, 255);
    }
    for (int i = top; i < top + vis; i++) {
        int y = y0 + 48 + (i - top) * rh, kind = o->rows[i];
        if (i == row)
            gfx_rrect(22, y, GFX_W - 44, rh - 4, 9, accent, 55);
        y += c - 14;                    /* the items below are placed for a 32 pixel row */
        bool off = kind == ROW_DRIVE && !o->drive_rom;   /* nothing to switch */
        gfx_text(&font_body, 34, y + 5, names[kind], off ? FAINT : i == row ? TEXT : DIM, 255);
        int xr = GFX_W - 34;
        char v[32];
        switch (kind) {
        case ROW_SOUND: switch_toggle(xr - 38, y + 4, g_prefs.sound, accent); break;
        case ROW_VOLUME: level_bars(xr - VOLUME_MAX * 7 + 2, y + 5, VOLUME_MAX, g_prefs.volume, g_prefs.sound, accent); break;
        case ROW_BRIGHT: level_bars(xr - BRIGHT_MAX * 7 + 2, y + 5, BRIGHT_MAX, g_prefs.brightness, true, accent); break;
        case ROW_DISPLAY:
            value_arrows(xr - 8, y + 4, devices[o->device].modes[prefs_video(o->device)]);
            break;
        case ROW_STATS: switch_toggle(xr - 38, y + 4, g_prefs.stats, accent); break;
        case ROW_SCAN: switch_toggle(xr - 38, y + 4, prefs_scanlines(o->device), accent); break;
        case ROW_CABLE: switch_toggle(xr - 38, y + 4, (o->raw & 0x10) != 0, accent); break;
        case ROW_FAV: switch_toggle(xr - 38, y + 4, o->fav, FAV_GOLD); break;
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
            value_arrows(xr - 8, y + 4, o->raw_sys == RAW_CPC ? cpc_key_name(raw_key(o)) :
                                        o->raw_sys == RAW_ZX ? zx_key_name(raw_key(o)) : key_names[o->key]);
            break;
        case ROW_LAYOUT: {
            key_chips(34, y + 7, "B A Center");     /* ... get these Mega Drive buttons */
            const char *l = md_layouts[o->raw % MD_LAYOUTS];
            snprintf(v, sizeof v, "%c  %c  %c", l[0], l[1], l[2]);
            value_arrows(xr - 8, y + 4, v);
            break;
        }
        case ROW_SWAP:
            key_chips(34, y + 7, "B A");            /* ... get these buttons */
            value_arrows(xr - 8, y + 4, (o->raw & swap_bit(o)) ? (o->nes ? "B  A" : "2  1") : (o->nes ? "A  B" : "1  2"));
            break;
        case ROW_JOYKIND:
            value_arrows(xr - 8, y + 4, o->raw_sys == RAW_ZX ? zx_joys[(o->raw >> 6) & 3] : cpc_joys[(o->raw >> 6) % 3]);
            break;
        case ROW_MACHINE: value_arrows(xr - 8, y + 4, o->zx48 ? "48K" : "128K"); break;
        case ROW_PALETTE: value_arrows(xr - 8, y + 4, gb_palettes[o->raw & 7]); break;
        case ROW_DIFF_L: value_arrows(xr - 8, y + 4, (o->raw & 1) ? "A" : "B"); break;
        case ROW_DIFF_R: value_arrows(xr - 8, y + 4, (o->raw & 2) ? "A" : "B"); break;
        case ROW_BW: value_arrows(xr - 8, y + 4, (o->raw & 4) ? "B/W" : "Color"); break;
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
    bool game_opts = game && device >= 0 && devices[device].app != APP_NONE;
    if (game_opts) {
        o.rows[o.nrows++] = ROW_FAV;
        o.fav = favs_has(device, game);
    }
    /* a C64 game's own rows first, the ones for all games below them */
    bool c64 = game_opts && device >= 0 && devices[device].app == APP_C64;
    if (c64) {
        o.rows[o.nrows++] = ROW_PORT;
        o.rows[o.nrows++] = ROW_PROGRAM;
        o.rows[o.nrows++] = ROW_KEY;
        o.rows[o.nrows++] = ROW_DRIVE;
    }
    if (game_opts) {
        /* the game's own byte: which rows show it */
        int app = devices[device].app;
        const char *ext = strrchr(game, '.');
        bool gb_old = ext && strcasecmp(ext, ".gb") == 0;                 /* not a Color game */
        bool a78 = ext && strcasecmp(ext, ".a78") == 0;
        bool vec = ext && strcasecmp(ext, ".vec") == 0;
        o.raw_sys = app == APP_CPC ? RAW_CPC : app == APP_ZX ? RAW_ZX : app == APP_MD ? RAW_MD :
                    app == APP_NES || app == APP_SMS ? RAW_SWAP : app == APP_GBC ? RAW_GB :
                    app == APP_A26 && !vec ? RAW_A26 : RAW_NONE;
        o.nes = app == APP_NES || app == APP_GBC;           /* buttons called A and B */
        if (o.raw_sys != RAW_NONE) {
            settings_load();
            o.raw = settings_key_raw(game);
        }
        switch (o.raw_sys) {
        case RAW_CPC: o.rows[o.nrows++] = ROW_KEY; o.rows[o.nrows++] = ROW_JOYKIND; break;
        case RAW_ZX: {
            o.rows[o.nrows++] = ROW_KEY;
            o.rows[o.nrows++] = ROW_JOYKIND;
            /* tapes: the machine; a snapshot brings its own */
            if (ext && (strcasecmp(ext, ".tap") == 0 || strcasecmp(ext, ".tzx") == 0)) {
                o.rows[o.nrows++] = ROW_MACHINE;
                o.zx48 = settings_true_drive(game);
            }
            break;
        }
        case RAW_MD:  o.rows[o.nrows++] = ROW_LAYOUT; break;
        case RAW_SWAP: o.rows[o.nrows++] = ROW_SWAP; break;
        case RAW_GB:
            if (gb_old)
                o.rows[o.nrows++] = ROW_PALETTE;
            o.rows[o.nrows++] = ROW_SWAP;
            o.rows[o.nrows++] = ROW_CABLE;      /* two PicoBoys as two Game Boys */
            break;
        case RAW_A26:
            o.rows[o.nrows++] = ROW_DIFF_L;
            o.rows[o.nrows++] = ROW_DIFF_R;
            if (!a78)
                o.rows[o.nrows++] = ROW_BW;     /* on the 7800 that switch is PAUSE */
            break;
        }
    }
    o.rows[o.nrows++] = ROW_SOUND;
    o.rows[o.nrows++] = ROW_VOLUME;
    o.rows[o.nrows++] = ROW_BRIGHT;
    if (device >= 0 && mode_count(device) > 1)
        o.rows[o.nrows++] = ROW_DISPLAY;
    if (device >= 0 && devices[device].app != APP_NONE)
        o.rows[o.nrows++] = ROW_SCAN;
    if (device >= 0 && devices[device].app != APP_NONE)
        o.rows[o.nrows++] = ROW_STATS;
    if (c64) {
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
        uint32_t e = edges();
        int d = (e & IN_RIGHT) ? 1 : (e & IN_LEFT) ? -1 : 0;
        if (e & IN_UP)   row = (row + o.nrows - 1) % o.nrows;
        if (e & IN_DOWN) row = (row + 1) % o.nrows;
        if (d) {
            switch (o.rows[row]) {
            case ROW_SOUND:
                g_prefs.sound = !g_prefs.sound;
                sound_update();
                sound_click();
                break;
            case ROW_VOLUME:
                g_prefs.volume += d;
                if (g_prefs.volume < VOLUME_MIN) g_prefs.volume = VOLUME_MIN;
                if (g_prefs.volume > VOLUME_MAX) g_prefs.volume = VOLUME_MAX;
                g_prefs.sound = true;
                sound_update();
                sound_click();                  /* hear the new volume */
                break;
            case ROW_DISPLAY: {
                int n = mode_count(device);
                prefs_set_video(device, (prefs_video(device) + n + d) % n);
                break;
            }
            case ROW_STATS: g_prefs.stats = !g_prefs.stats; break;
            case ROW_SCAN: prefs_set_scanlines(device, !prefs_scanlines(device)); break;
            case ROW_CABLE: o.raw ^= 0x10; break;
            case ROW_FAV: o.fav = !o.fav; break;
            case ROW_BRIGHT:
                g_prefs.brightness += d;
                if (g_prefs.brightness < BRIGHT_MIN) g_prefs.brightness = BRIGHT_MIN;
                if (g_prefs.brightness > BRIGHT_MAX) g_prefs.brightness = BRIGHT_MAX;
                pbc_display_backlight(prefs_backlight());     /* see it at once */
                break;
            case ROW_PORT: o.port = o.port == 1 ? 2 : 1; break;
            case ROW_PROGRAM: o.file = (o.file + o.nfiles + d) % o.nfiles; break;
            case ROW_KEY:
                if (o.raw_sys == RAW_CPC)
                    o.raw = (o.raw & 0xc0) | (raw_key(&o) + CPC_KEYS + d) % CPC_KEYS;
                else if (o.raw_sys == RAW_ZX)
                    o.raw = (o.raw & 0xc0) | (raw_key(&o) + ZX_KEYS + d) % ZX_KEYS;
                else
                    o.key = (o.key + C64_KEY_COUNT + d) % C64_KEY_COUNT;
                break;
            case ROW_LAYOUT: o.raw = (o.raw % MD_LAYOUTS + MD_LAYOUTS + d) % MD_LAYOUTS; break;
            case ROW_JOYKIND:
                if (o.raw_sys == RAW_ZX)
                    o.raw = (o.raw & 0x3f) | ((((o.raw >> 6) & 3) + 4 + d) & 3) << 6;
                else
                    o.raw = (o.raw & 0x3f) | ((((o.raw >> 6) % 3) + 3 + d) % 3) << 6;
                break;
            case ROW_MACHINE: o.zx48 = !o.zx48; break;
            case ROW_SWAP:    o.raw ^= swap_bit(&o); break;
            case ROW_PALETTE: o.raw = (o.raw & ~7) | (((o.raw & 7) + GB_PALETTES + d) % GB_PALETTES); break;
            case ROW_DIFF_L:  o.raw ^= 1; break;
            case ROW_DIFF_R:  o.raw ^= 2; break;
            case ROW_BW:      o.raw ^= 4; break;
            case ROW_DRIVE: o.drive = o.drive_rom && !o.drive; break;
            }
        }
        if (e & (IN_A | IN_B | IN_CENTER))
            break;
        if (lobby_incoming(NULL))           /* the partner started a game */
            break;
    }
    prefs_save();
    if (game_opts && o.fav != favs_has(device, game)) {
        if (favs_set(device, game, o.fav))
            favs_save();
        else
            ui_message("Favorites", "The list is full:", "at most 40 games.", NULL);
    }
    if (c64) {
        settings_set_port(game, o.port);
        settings_set_file(game, o.file ? o.files[o.file] : "");
        settings_set_key(game, o.key);
        if (o.drive_rom)                /* without the ROM the choice stays as it was */
            settings_set_true_drive(game, o.drive);
        settings_save();
        free(o.files);
    }
    bool zx_tape = false;
    for (int i = 0; i < o.nrows; i++)
        zx_tape |= o.rows[i] == ROW_MACHINE;
    if (zx_tape && o.zx48 != settings_true_drive(game)) {
        settings_set_true_drive(game, o.zx48);  /* the ZX's machine in the C64's drive byte */
        settings_save();
    }
    if (o.raw_sys != RAW_NONE && o.raw != settings_key_raw(game)) {
        settings_set_key_raw(game, o.raw);  /* only then: an entry for every game would fill the table */
        settings_save();
    }
    ui_wait_release();
}

/* ---- link play ------------------------------------------------------------------------ */

static void background_under(void *arg)
{
    (void)arg;
    background();
}

static void link_card(void (*under)(void *), void *arg, const char *title, const char *line,
                      const char *const *rows, int nrows, int sel, const char *foot, uint32_t accent)
{
    under(arg);
    gfx_rect(0, 0, GFX_W, GFX_H, 0x000000, 150);
    int h = 94 + nrows * OPT_ROW_H, y0 = (GFX_H - h) / 2;
    gfx_rrect(14, y0, GFX_W - 28, h, 16, CARD, 255);
    char t[GAME_NAME_MAX + 4];
    gfx_text_fit(&font_head, title, GFX_W - 60, t);
    gfx_text(&font_head, 30, y0 + 14, t, TEXT, 255);
    gfx_text(&font_small, 30, y0 + 42, line, DIM, 255);
    for (int i = 0; i < nrows; i++) {
        int y = y0 + 62 + i * OPT_ROW_H;
        if (i == sel)
            gfx_rrect(22, y, GFX_W - 44, OPT_ROW_H - 4, 9, accent, 90);
        gfx_text(&font_body, 34, y + 5, rows[i], i == sel ? TEXT : DIM, 255);
    }
    gfx_text_centered(&font_small, GFX_W / 2, y0 + h - 24, foot, DIM, 255);
    gfx_present();
}

/* A partner is there: this device picks its controller and offers the game.
 * True: both start it. False: back to the list. */
static bool link_offer(void (*under)(void *), void *arg, int device, const char *game, uint32_t accent)
{
    static const char *const rows[] = { "Player 1", "Player 2" };
    static int sel;                     /* as last time */
    char title[GAME_NAME_MAX], tags[GAME_NAME_MAX];
    games_title(game, title, tags, sizeof title);

    ui_wait_release();
    for (;;) {
        link_card(under, arg, title, "Choose your controller", rows, 2, sel, "B  start      A  back", accent);
        uint32_t e = edges();
        if (e & (IN_UP | IN_DOWN | IN_LEFT | IN_RIGHT))
            sel = !sel;
        if (e & IN_B)
            break;
        if ((e & IN_A) || lobby_incoming(NULL) || !lobby_partner())
            return false;
    }

    static lobby_offer_t o;
    o.device = device;
    o.player = sel ? 1 : 2;
    snprintf(o.game, sizeof o.game, "%s", game);
    lobby_offer(&o);
    for (;;) {
        link_card(under, arg, title, "Waiting for partner ...", NULL, 0, 0, "A  back", accent);
        switch (lobby_offer_state()) {
        case LOBBY_ACCEPTED:
            ui_link_player = sel + 1;
            ui_link_master = true;
            return true;
        case LOBBY_NO_GAME:
            ui_message("Link play", "Your partner does not", "have this game.", NULL);
            return false;
        case LOBBY_GONE:
            ui_message("Link play", "Your partner has left.", NULL, NULL);
            return false;
        case LOBBY_LOST:                /* both at once: the partner's game it is */
            return false;
        }
        if (edges() & IN_A) {
            lobby_offer_cancel();
            return false;
        }
    }
}

static bool file_on_drive(const char *name)
{
    static FILINFO fi;
    char path[GAME_NAME_MAX + 2];
    snprintf(path, sizeof path, "/%s", name);
    return f_stat(path, &fi) == FR_OK;
}

/* The partner started a game: this device starts it too.
 * True with device and game set. */
static bool link_join(int *device, char game[GAME_NAME_MAX])
{
    static lobby_offer_t o;
    if (!lobby_incoming(&o))
        return false;
    bool ok = o.device >= 0 && o.device < device_count && devices[o.device].app != APP_NONE &&
              device_link(&devices[o.device]) &&
              (o.player == 1 || o.player == 2) && file_on_drive(o.game);
    lobby_answer(ok);
    if (!ok)
        return false;

    char title[GAME_NAME_MAX], tags[GAME_NAME_MAX], line[32];
    games_title(o.game, title, tags, sizeof title);
    snprintf(line, sizeof line, "You are player %d", o.player);
    int went;
    while ((went = lobby_went()) == 0) {
        link_card(background_under, NULL, title, line, NULL, 0, 0, "Starting ...", devices[o.device].accent);
    }
    if (went < 0) {
        ui_message("Link play", "Your partner has left.", NULL, NULL);
        return false;
    }
    *device = o.device;
    strcpy(game, o.game);
    ui_link_player = o.player;
    ui_link_master = false;
    return true;
}

/* ---- game list ------------------------------------------------------------------------ */

#define LIST_FAV    (-1)            /* list_t.device: the favourites */

typedef struct {
    int device;                     /* a device, or LIST_FAV */
    int *devs;                      /* favourites: the device of each game */
    char (*names)[GAME_NAME_MAX];
    int n, cur;
    int scroll;                     /* in 1/16 pixel, follows the cursor smoothly */
    uint64_t cur_since;             /* when the cursor got there: long names scroll */
    int fade;                       /* 0..256 when entering */
} list_t;

static void list_screen(void *arg)
{
    list_t *l = arg;
    bool fav = l->device < 0;           /* games of every system */
    const device_t *dev = fav ? NULL : &devices[l->device];
    uint32_t accent = fav ? FAV_GOLD : dev->accent;
    background();

    /* header: the accent fading into the background */
    for (int y = 0; y < 66; y++)
        gfx_rect(0, y, GFX_W, 1, accent, 70 - y);
    gfx_picture(fav ? &icon_fav : dev->icon, 10, 10, l->fade);
    gfx_text(&font_head, 90, 13, fav ? "Favorites" : dev->name, TEXT, 255);
    char c[24];
    if (fav)
        fav_text(l->n, c, sizeof c);
    else
        count_text(l->device, c, sizeof c);
    gfx_text(&font_small, 90, 37, c, DIM, 255);
    const device_t *link_dev = fav ? (l->n ? &devices[l->devs[l->cur]] : NULL) : dev;
    if (lobby_partner() && link_dev && device_link(link_dev)) {
        const char *s = "Link connected";
        gfx_text(&font_small, GFX_W - 12 - gfx_text_w(&font_small, s), 37, s, LINK_GREEN, 255);
    }
    gfx_rect(0, 64, GFX_W, 1, 0xffffff, 22);

    if (l->n == 0 && fav) {
        gfx_text_centered(&font_body, GFX_W / 2, 128, "No favorites yet", TEXT, 255);
        gfx_text_centered(&font_small, GFX_W / 2, 152, "In a game list, press Center", DIM, 255);
        gfx_text_centered(&font_small, GFX_W / 2, 166, "and switch on \"Favorite\".", DIM, 255);
    } else if (l->n == 0) {
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
            uint32_t row_accent = fav ? devices[l->devs[i]].accent : accent;
            if (sel) {
                gfx_rrect(8, y + 1, GFX_W - 20, ROW_H - 2, 9, row_accent, 70);
                gfx_rrect(8, y + 6, 4, ROW_H - 12, 2, row_accent, 255);
            }
            char title[GAME_NAME_MAX], tags[GAME_NAME_MAX], fit[GAME_NAME_MAX + 4];
            games_title(l->names[i], title, tags, sizeof title);
            int max_w = GFX_W - 44;
            /* on the right: a favourite's star, or in the favourites its system */
            if (fav) {
                const char *sys = devices[l->devs[i]].name;
                int w = gfx_text_w(&font_small, sys);
                gfx_text(&font_small, GFX_W - 18 - w, y + 9, sys, sel ? DIM : FAINT, 255);
                max_w -= w + 8;
            } else if (favs_has(l->device, l->names[i])) {
                star(GFX_W - 27, y + 11, FAV_GOLD, 255);
                max_w -= 16;
            }
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
            gfx_rrect(GFX_W - 7, ty, 3, th, 1, accent, 255);
        }
    }

    gfx_rect(0, GFX_H - 28, GFX_W, 1, 0xffffff, 22);
    static const char *const keys[] = { "B", "A", "*" };
    static const char *const labels[] = { "Play", "Back", "Options" };
    footer(keys, labels, 3);
}

/* Returns true with the chosen game in `game`, false for "back". A game the
 * link partner started may be on another device: `device` follows it. */
static bool game_list(int *device_io, char game[GAME_NAME_MAX], int which)
{
    bool fav = which < 0;                   /* LIST_FAV: games of every system */
    int device = fav ? which : *device_io;
    list_t l = { .device = device };
    l.n = fav ? games_favorites(&l.names, &l.devs) : games_list(device, &l.names);
    for (int i = 0; game[0] && i < l.n; i++)
        if (strcmp(l.names[i], game) == 0 && (!fav || l.devs[i] == *device_io))
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

        if (link_join(device_io, game))
            return true;

        uint32_t e = edges();
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
        int dv = fav ? l.devs[l.cur] : device;
        if (e & IN_CENTER) {
            options(list_screen, &l, dv, l.names[l.cur], devices[dv].accent);
            if (which == LIST_FAV) {    /* it may have left the favourites */
                l.n = games_favorites(&l.names, &l.devs);
                if (l.cur >= l.n)
                    l.cur = l.n ? l.n - 1 : 0;
            }
            continue;
        }
        if (e & IN_B) {
            strcpy(game, l.names[l.cur]);
            *device_io = dv;
            if (!lobby_partner() || !device_link(&devices[dv]) ||
                link_offer(list_screen, &l, dv, game, devices[dv].accent))
                return true;
            ui_wait_release();
        }
    }
}

/* ---- start animation ------------------------------------------------------------------------ */

bool ui_intro_pending;

#define INTRO_BG 0x080b14

/* "PBC" from the left, "emu" from the right, a glint over the word; then
 * the carousel shows through. 1.5 s, any button skips it. */
static void intro(void)
{
    const int lw = logo_pbc.w + logo_emu.w, x0 = (GFX_W - lw) / 2, y0 = 96;      /* a little above the middle */
    uint64_t t0 = 0;
    for (;;) {
        int ms = t0 ? (int)((time_us_64() - t0) / 1000) : 0;
        if (ms >= 1500 || input_raw())
            break;
        /* the word rises and fades from 1.0 s, the carousel shows from 1.15 s */
        int fade = ms < 1000 ? 255 : ms < 1250 ? 255 - (ms - 1000) * 255 / 250 : 0;
        int cover = ms < 1150 ? 255 : 255 - (ms - 1150) * 255 / 350;
        int y = y0 - (ms < 1000 ? 0 : (ms - 1000) / 12);
        if (cover < 255)
            device_screen(0, 0, 0, 1);
        gfx_rect(0, 0, GFX_W, GFX_H, INTRO_BG, cover);
        int glow = ms < 200 ? 0 : ms < 700 ? (ms - 200) * 70 / 500 : 70;
        gfx_glow(GFX_W / 2, y + 32, 120, 54, FAV_GOLD, glow * fade / 255);

        int e = ease(ms >= 500 ? 256 : ms * 256 / 500);
        int a = e * fade / 256, slide = (256 - e) * 70 / 256;
        gfx_picture(&logo_pbc, x0 - slide, y, a);
        gfx_picture(&logo_emu, x0 + logo_pbc.w + slide, y, a);
        static bool rung;
        if (ms > 450 && !rung) {
            rung = true;
            sound_bell();
        }
        if (ms > 450 && ms < 1000) {        /* the glint, soft at its edges */
            int bx = x0 - 30 + (lw + 60) * (ms - 450) / 550;
            static const int half[] = { 22, 13, 6 }, alpha[] = { 50, 70, 110 };
            for (int i = 0; i < 3; i++) {
                gfx_clip(bx - half[i], 0, bx + half[i], GFX_H);
                gfx_picture(&logo_glint, x0, y, alpha[i] * fade / 255);
            }
            gfx_noclip();
        }
        gfx_present();                      /* the first one fades the light in */
        if (!t0)
            t0 = time_us_64();
    }
}

/* ---- main loop ---------------------------------------------------------------------------- */

typedef struct { int sel; } carousel_t;

static void device_still(void *arg)
{
    int sel = ((carousel_t *)arg)->sel;
    device_screen(sel, sel, 0, 1);
}

static void count_favs(void)
{
    char (*names)[GAME_NAME_MAX];
    int *devs;
    fav_count = games_favorites(&names, &devs);
}

ui_result_t ui_run(int *device, char game[GAME_NAME_MAX])
{
    games_count(counts);
    count_favs();
    int sel = 0, next = 0, dir = 1, t = 0;   /* always the favourites first */
    if (ui_intro_pending) {
        ui_intro_pending = false;
        intro();
    }

    ui_link_player = 0;
    ui_wait_release();
    for (;;) {
        device_screen(sel, next, t, dir);
        gfx_present();
        if (link_join(device, game))
            return UI_PLAY;
        if (next != sel) {
            t += 48;
            if (t >= 256) {
                sel = next;
                t = 0;
            }
        }
        uint32_t e = edges();
        if (e & (IN_LEFT | IN_RIGHT)) {
            if (next != sel)                /* still sliding: finish it at once */
                sel = next;
            dir = (e & IN_RIGHT) ? 1 : -1;
            next = (sel + dir + PAGES) % PAGES;
            t = 0;
        }
        if (next != sel)
            continue;
        int d = sel - FIRST_DEVICE;         /* -1: the favourites */
        if (e & IN_CENTER) {
            carousel_t c = { sel };
            options(device_still, &c, d < 0 || devices[d].app == APP_NONE ? -1 : d, NULL, page_accent(sel));
        }
        if (e & IN_B) {
            if (d >= 0 && devices[d].app == APP_NONE)
                return UI_USB;
            if (d >= 0)
                *device = d;
            if (game_list(device, game, d < 0 ? LIST_FAV : 0))
                return UI_PLAY;
            game[0] = 0;
            count_favs();
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
    if (!device_is_coleco(dev))         /* its six keypad rows reach down to here */
        gfx_text_centered(&font_small, GFX_W / 2, GFX_H - 38, "Back to this list: RESET", FAINT, 255);
    if (alpha > 0) {
        int w = 120;
        int bar_y = device_is_coleco(dev) ? GFX_H - 14 : GFX_H - 20;   /* below the keypad rows */
        gfx_rrect((GFX_W - w) / 2, bar_y, w * alpha / 255, 4, 2, dev->accent, 255);
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
    char port[24], note[64] = "";
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
    if (dev->app != APP_C64)
        settings_load();
    int raw = dev->app != APP_C64 ? settings_key_raw(game) : 0;
    if ((dev->app == APP_CPC || dev->app == APP_ZX) && !ui_link_player) {
        for (int i = 0; i < n; i++)
            if (strcmp(k[i], "Center + B") == 0)
                b[i] = dev->app == APP_CPC ? cpc_key_name((raw & 0x3f) % CPC_KEYS) : zx_key_name((raw & 0x3f) % ZX_KEYS);
        b[0] = dev->app == APP_CPC ? cpc_joys[(raw >> 6) % 3] : zx_joys[(raw >> 6) & 3];   /* the joystick kind */
    }
    if (((dev->app == APP_NES || dev->app == APP_SMS) && (raw & 1)) || (dev->app == APP_GBC && (raw & 8))) {
        int ia = -1, ib = -1;                       /* swapped: the names of A and B change places */
        for (int i = 0; i < n; i++) {
            if (strcmp(k[i], "A") == 0) ia = i;
            if (strcmp(k[i], "B") == 0) ib = i;
        }
        if (ia >= 0 && ib >= 0) {
            const char *t = b[ia];
            b[ia] = b[ib];
            b[ib] = t;
        }
    }
    if (dev->app == APP_MD) {           /* the game's button layout */
        static const char *const letter[] = { "A", "B", "C" };
        static const char *const pbc[] = { "B", "A", "Center" };
        const char *l = md_layouts[raw % MD_LAYOUTS];
        for (int i = 0; i < n; i++)
            for (int j = 0; j < 3; j++)
                if (strcmp(k[i], pbc[j]) == 0)
                    b[i] = letter[l[j] - 'A'];
    }

    if (ui_link_player) {
        size_t len = strlen(note);
        snprintf(note + len, sizeof note - len, "%sLink: player %d", len ? ", " : "", ui_link_player);
    }

    /* about three seconds, or until B is pressed */
    ui_wait_release();
    for (int f = 0; f < 96; f++) {
        int t = f * 255 / 95;
        starting_screen(device, fit, b, k, n, note[0] ? note : NULL, t);
        gfx_present();
        if (edges() & IN_B)
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
    device_picture(dev->pic, (GFX_W - dev->pic->w) / 2, 36, 255, false);
    gfx_text_centered(&font_title, GFX_W / 2, 170, "USB Drive", TEXT, 255);
    gfx_text_centered(&font_small, GFX_W / 2, 202, "Copy games onto PBC-ROMS,", DIM, 255);
    gfx_text_centered(&font_small, GFX_W / 2, 217, "then eject the drive.", DIM, 255);
    int w = gfx_text_w(&font_small, states[state]) + 26;
    gfx_rrect((GFX_W - w) / 2, 240, w, 20, 10, state == 2 ? 0x3ccf7a : dev->accent, 110);
    gfx_text_centered(&font_small, GFX_W / 2, 244, states[state], TEXT, 255);
    gfx_present();
}
