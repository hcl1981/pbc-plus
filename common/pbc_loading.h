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

/* "Loading game": the game being copied into the flash, with a bar. */
void pbc_loading_begin(const char *game);
void pbc_loading_progress(uint32_t done, uint32_t total);

/* A card with a title and up to three lines (errors, crashes). */
void pbc_loading_message(const char *title, const char *l1, const char *l2, const char *l3);

#endif
