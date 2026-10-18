/* menu.c -- see menu.h */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "ff.h"
#include "board.h"
#include "color565.h"
#include "display.h"
#include "input.h"
#include "rom_store.h"
#include "settings.h"
#include "menu.h"

#define MAX_ROMS     200
#define ROW_H        12
#define LIST_Y       34
#define LIST_ROWS    18          /* 34 + 18*12 = 250, footer below */
#define NAME_CHARS   27          /* (240 - 2*12) / 8 */

#define COL_BG       PBC_BLACK
#define COL_TITLE    PBC_YELLOW
#define COL_TEXT     PBC_WHITE
#define COL_DIM      PBC_GREY
#define COL_SEL_BG   PBC_BLUE
#define COL_BAR      PBC_DKGREY

/* Heap, not .bss: only needed while the menu is up, and the emulator's
 * tile cache wants every byte it can get afterwards. */
static char (*rom_names)[ROM_NAME_MAX];
static int  rom_count;

void menu_wait_release(void)
{
    while (input_raw())
        sleep_ms(10);
    sleep_ms(30);
    input_menu_edges();   /* swallow the release edge state */
}

static void header(const char *title)
{
    pbc_display_fill(0, 0, PBC_LCD_W, PBC_LCD_H, COL_BG);
    pbc_display_fill(0, 0, PBC_LCD_W, 28, COL_BAR);
    pbc_display_text_centered(10, title, COL_TITLE, COL_BAR, 1);
}

void menu_message(const char *title, const char *l1, const char *l2, const char *l3)
{
    header(title);
    if (l1) pbc_display_text_centered(110, l1, COL_TEXT, COL_BG, 1);
    if (l2) pbc_display_text_centered(126, l2, COL_TEXT, COL_BG, 1);
    if (l3) pbc_display_text_centered(142, l3, COL_DIM, COL_BG, 1);
}

static void shorten(const char *src, char *dst, int max_chars)
{
    int n = (int)strlen(src);
    if (n <= max_chars) {
        strcpy(dst, src);
        return;
    }
    memcpy(dst, src, (size_t)max_chars - 2);
    dst[max_chars - 2] = '.';
    dst[max_chars - 1] = '.';
    dst[max_chars] = 0;
}

void menu_progress_begin(const char *name)
{
    char shortname[NAME_CHARS + 1];
    shorten(name, shortname, NAME_CHARS);
    header("SPIEL WIRD GELADEN");
    pbc_display_text_centered(100, shortname, COL_TEXT, COL_BG, 1);
    pbc_display_text_centered(170, "Nur beim ersten Start", COL_DIM, COL_BG, 1);
    pbc_display_text_centered(182, "eines Spiels noetig.", COL_DIM, COL_BG, 1);
    pbc_display_fill(20, 130, 200, 12, COL_BAR);
}

void menu_progress(uint32_t done, uint32_t total)
{
    int w = total ? (int)((uint64_t)done * 196 / total) : 0;
    pbc_display_fill(22, 132, w, 8, COL_TITLE);
}

static int cmp_names(const void *a, const void *b)
{
    return strcasecmp((const char *)a, (const char *)b);
}

static void scan(void)
{
    DIR dir;
    static FILINFO fi;   /* long file name buffer, keep it off the stack */
    rom_count = 0;
    rom_names = malloc(MAX_ROMS * ROM_NAME_MAX);
    if (!rom_names || f_opendir(&dir, "/") != FR_OK)
        return;
    while (rom_count < MAX_ROMS && f_readdir(&dir, &fi) == FR_OK && fi.fname[0]) {
        if (fi.fattrib & (AM_DIR | AM_HID | AM_SYS))
            continue;
        if (fi.fname[0] == '.')              /* macOS ._ files */
            continue;
        if (!rom_store_is_rom_name(fi.fname) || strlen(fi.fname) >= ROM_NAME_MAX)
            continue;
        strcpy(rom_names[rom_count++], fi.fname);
    }
    f_closedir(&dir);
    qsort(rom_names, (size_t)rom_count, ROM_NAME_MAX, cmp_names);
}

static void draw_row(int idx, int top, int sel, const char *current)
{
    int row = idx - top;
    int y = LIST_Y + row * ROW_H;
    bool is_sel = idx == sel;
    uint16_t bg = is_sel ? COL_SEL_BG : COL_BG;
    char buf[NAME_CHARS + 2];

    pbc_display_fill(0, y, PBC_LCD_W, ROW_H, bg);
    if (idx >= rom_count)
        return;
    bool in_flash = current && strcmp(current, rom_names[idx]) == 0;
    shorten(rom_names[idx], buf, NAME_CHARS);
    pbc_display_text(12, y + 2, buf, in_flash ? COL_TITLE : COL_TEXT, bg, 1);
}

static void draw_list(int top, int sel, const char *current)
{
    for (int i = 0; i < LIST_ROWS; i++)
        draw_row(top + i, top, sel, current);
    char pos[24];
    snprintf(pos, sizeof pos, "%d/%d", sel + 1, rom_count);
    pbc_display_fill(0, 254, PBC_LCD_W, 8, COL_BG);
    pbc_display_text_centered(254, pos, COL_DIM, COL_BG, 1);
}

/* Options: sound on/off and volume. The only place either can be changed,
 * so a game cannot switch them by accident. Saved on the way out. */
static void options(void)
{
    const int rows = 3;
    int sel = 0;

    header("OPTIONEN");
    pbc_display_text_centered(200, "Hoch/Runter: waehlen", COL_DIM, COL_BG, 1);
    pbc_display_text_centered(212, "Links/Rechts: aendern", COL_DIM, COL_BG, 1);
    pbc_display_text_centered(224, "B: zurueck", COL_DIM, COL_BG, 1);
    menu_wait_release();

    for (bool redraw = true;;) {
        if (redraw) {
            char line[3][32];
            snprintf(line[0], 32, "Ton          %s", g_settings.sound ? "an " : "aus");
            snprintf(line[1], 32, "Lautstaerke  %-2d", g_settings.volume);
            snprintf(line[2], 32, "Zurueck");
            for (int i = 0; i < rows; i++) {
                uint16_t bg = i == sel ? COL_SEL_BG : COL_BG;
                uint16_t fg = (i == 1 && !g_settings.sound) ? COL_DIM : COL_TEXT;
                pbc_display_fill(0, 70 + i * 22, PBC_LCD_W, 16, bg);
                pbc_display_text(32, 74 + i * 22, line[i], fg, bg, 1);
            }
            redraw = false;
        }
        uint32_t e = input_menu_edges();
        if (!e) {
            sleep_ms(15);
            continue;
        }
        redraw = true;
        if (e & IN_UP)   sel = (sel + rows - 1) % rows;
        if (e & IN_DOWN) sel = (sel + 1) % rows;
        if ((e & IN_B) || ((e & (IN_A | IN_CENTER)) && sel == 2))
            break;
        int dir = (e & IN_LEFT) ? -1 : (e & (IN_RIGHT | IN_A | IN_CENTER)) ? 1 : 0;
        if (dir && sel == 0)
            g_settings.sound = !g_settings.sound;
        if (dir && sel == 1) {
            int v = g_settings.volume + dir;
            if (v < VOLUME_MIN) v = VOLUME_MIN;
            if (v > VOLUME_MAX) v = VOLUME_MAX;
            g_settings.volume = v;
        }
    }
    settings_save();
    menu_wait_release();
}

static void list_frame(void)
{
    header("PBC NES");
    pbc_display_text_centered(266, "A: Spielen   B: Optionen", COL_DIM, COL_BG, 1);
}

static bool pick(char *out, int out_size);

bool menu_pick_rom(char *out, int out_size)
{
    bool ok = pick(out, out_size);
    free(rom_names);
    rom_names = NULL;
    return ok;
}

static bool pick(char *out, int out_size)
{
    scan();
    if (rom_count == 0) {
        menu_message("PBC NES",
                     "Keine ROMs gefunden.",
                     "Taste druecken fuer den",
                     "USB-Stick-Modus");
        menu_wait_release();
        while (!input_menu_edges())
            sleep_ms(20);
        return false;
    }

    const char *current = rom_store_current();
    int sel = 0;
    for (int i = 0; current && i < rom_count; i++)
        if (strcmp(rom_names[i], current) == 0)
            sel = i;
    int top = sel - LIST_ROWS / 2;

    list_frame();
    menu_wait_release();

    bool full = true;
    for (;;) {
        if (top > rom_count - LIST_ROWS) top = rom_count - LIST_ROWS;
        if (top < 0) top = 0;
        if (full) {
            draw_list(top, sel, current);
            full = false;
        }

        uint32_t e = input_menu_edges();
        if (!e) {
            sleep_ms(15);
            continue;
        }
        int old_sel = sel, old_top = top;
        if (e & IN_UP)    sel = sel > 0 ? sel - 1 : rom_count - 1;
        if (e & IN_DOWN)  sel = sel < rom_count - 1 ? sel + 1 : 0;
        if (e & IN_LEFT)  { sel -= LIST_ROWS; if (sel < 0) sel = 0; }
        if (e & IN_RIGHT) { sel += LIST_ROWS; if (sel >= rom_count) sel = rom_count - 1; }
        if (e & IN_B) {
            options();
            list_frame();
            full = true;
            continue;
        }
        if (e & (IN_A | IN_CENTER)) {
            strncpy(out, rom_names[sel], (size_t)out_size - 1);
            out[out_size - 1] = 0;
            return true;
        }

        if (sel < top) top = sel;
        if (sel >= top + LIST_ROWS) top = sel - LIST_ROWS + 1;
        if (top != old_top) {
            full = true;
        } else if (sel != old_sel) {
            draw_row(old_sel, top, sel, current);
            draw_row(sel, top, sel, current);
            char pos[24];
            snprintf(pos, sizeof pos, "%d/%d", sel + 1, rom_count);
            pbc_display_fill(0, 254, PBC_LCD_W, 8, COL_BG);
            pbc_display_text_centered(254, pos, COL_DIM, COL_BG, 1);
        }
    }
}
