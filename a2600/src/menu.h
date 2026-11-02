/* menu.h -- messages and lists in the launcher's look (core 0 only, while
 * core 1 is not drawing: a26_video_pause()). */
#ifndef PBC_MENU_H
#define PBC_MENU_H

#include <stdbool.h>
#include <stdint.h>

/* Full-screen message: title plus up to three lines. */
void menu_message(const char *title, const char *l1, const char *l2, const char *l3);

/* "Loading game" with a progress bar; done/total in bytes. */
void menu_progress_begin(const char *name);
void menu_progress(uint32_t done, uint32_t total);

/* Wait until every button is released. */
void menu_wait_release(void);

/* A list: returns the chosen index (B or Center), or -1 (A). `mark` is
 * shown with `note` (-1: none). `idle`, if any, runs all the time the list
 * is open: link play keeps the partner company with it. */
int menu_pick(const char *title, const char *const *rows, int n, int sel,
              int mark, const char *note, void (*idle)(void));

#endif /* PBC_MENU_H */
