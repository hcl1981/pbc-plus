/* pbc_loading.h -- the emulators' screens before the game, in the look of
 * the launcher (combined firmware only).
 *
 * Drawn with the launcher's gfx.c and fonts, a few rows at a time: an
 * emulator has no RAM for a frame buffer. The device (name, colour, icon)
 * comes from the launch record.
 */
#ifndef PBC_LOADING_H
#define PBC_LOADING_H

#include <stdint.h>
#include <stdbool.h>

/* The launcher fades the backlight out before it hands over; the emulator
 * starts dark and fades in once there is something on the panel: the
 * first screen below, or the game (call before its loop; returns at once
 * and fades in the background). Only the first call does anything. */
#define PBC_BACKLIGHT 200
void pbc_fade_in(bool wait);

/* "Loading game": the game being copied into the flash, with a bar. */
void pbc_loading_begin(const char *game);
void pbc_loading_progress(uint32_t done, uint32_t total);

/* A card with a title and up to three lines (errors, crashes). */
void pbc_loading_message(const char *title, const char *l1, const char *l2, const char *l3);

/* A list on a card, e.g. the C64's disks: `sel` highlighted, `mark` with
 * `note` on the right (-1: none), `footer` under it; a window of it when
 * long. Whole screen. While it is shown, call pbc_loading_list_tick()
 * every few ms: a selected name too long for its row scrolls through. */
void pbc_loading_list(const char *title, const char *const *rows, int n, int sel,
                      int mark, const char *note, const char *footer);
void pbc_loading_list_tick(void);

#endif
