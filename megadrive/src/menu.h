/* menu.h -- ROM list and on-screen messages (blocking, core 0 only). */
#ifndef PBC_MENU_H
#define PBC_MENU_H

#include <stdbool.h>
#include <stdint.h>

/* Scan the mounted FatFs root for ROMs and let the player pick one.
 * Writes the file name to `out`. Returns false if the player chose the first
 * entry, "Spiele per USB aufspielen", or if there are no ROMs at all (a hint
 * screen has been shown and the call has waited for a button); the caller
 * then switches to USB stick mode. */
bool menu_pick_rom(char *out, int out_size);

/* Full-screen message: title plus up to three lines. */
void menu_message(const char *title, const char *l1, const char *l2, const char *l3);

/* Progress bar under a "Lade ..." screen; done/total in bytes. */
void menu_progress(uint32_t done, uint32_t total);
void menu_progress_begin(const char *name);

/* Controls card shown for two seconds before a game starts, as PicoBoyGB
 * does: a framed box with one "console button -> PicoBoy key" line each. */
typedef struct { const char *button, *key; } menu_control_t;
void menu_controls(const menu_control_t *c, int n);

/* Wait until every button is released. */
void menu_wait_release(void);

#endif /* PBC_MENU_H */
