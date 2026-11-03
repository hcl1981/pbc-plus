/* menu.c -- see menu.h */
#include <string.h>
#include "pico/stdlib.h"
#include "board.h"
#include "color565.h"
#include "display.h"
#include "input.h"
#include "menu.h"
#include "pbc_loading.h"        /* the launcher's look */

#define PAGE 7                  /* rows of a list on the card */

void menu_wait_release(void)
{
    while (input_raw())
        sleep_ms(10);
    sleep_ms(30);
    input_menu_edges();         /* swallow the release edge state */
}

void menu_message(const char *title, const char *l1, const char *l2, const char *l3)
{
    /* not for a crash: that can come from a stack overflow, and the
     * launcher's look needs about 1 KB of stack */
    if (strcmp(title, "CRASH") != 0) {
        pbc_loading_message(title, l1, l2, l3);
        return;
    }
    pbc_display_backlight(PBC_BACKLIGHT);
    pbc_display_fill(0, 0, PBC_LCD_W, PBC_LCD_H, PBC_BLACK);
    pbc_display_text_centered(80, title, PBC_YELLOW, PBC_BLACK, 1);
    if (l1) pbc_display_text_centered(110, l1, PBC_WHITE, PBC_BLACK, 1);
    if (l2) pbc_display_text_centered(126, l2, PBC_WHITE, PBC_BLACK, 1);
    if (l3) pbc_display_text_centered(142, l3, PBC_GREY, PBC_BLACK, 1);
}

void menu_progress_begin(const char *name)
{
    pbc_loading_begin(name);
}

void menu_progress(uint32_t done, uint32_t total)
{
    pbc_loading_progress(done, total);
}

static void wait_release(void (*idle)(void))
{
    if (!idle) {
        menu_wait_release();
        return;
    }
    while (input_raw()) {
        idle();
        sleep_ms(15);
    }
}

int menu_pick(const char *title, const char *const *rows, int n, int sel,
              int mark, const char *note, void (*idle)(void))
{
    if (sel < 0 || sel >= n)
        sel = 0;
    wait_release(idle);
    pbc_loading_list(title, rows, n, sel, mark, note, "B  choose      A  back");
    for (;;) {
        if (idle)
            idle();
        uint32_t e = input_menu_edges();
        if (!e) {
            pbc_loading_list_tick();    /* a long name scrolls */
            sleep_ms(15);
            continue;
        }
        if (e & IN_A) {
            sel = -1;
            break;
        }
        if (e & (IN_B | IN_CENTER))
            break;
        int old = sel;
        if (e & IN_UP)    sel = (sel + n - 1) % n;
        if (e & IN_DOWN)  sel = (sel + 1) % n;
        if (e & IN_LEFT)  sel = sel >= PAGE ? sel - PAGE : 0;
        if (e & IN_RIGHT) sel = sel + PAGE < n ? sel + PAGE : n - 1;
        if (sel != old)
            pbc_loading_list(title, rows, n, sel, mark, note, "B  choose      A  back");
    }
    wait_release(idle);
    return sel;
}
