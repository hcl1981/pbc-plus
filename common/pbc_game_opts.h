/* pbc_game_opts.h -- a game's options, changed in the game.
 *
 * The launcher keeps per-game options in its table at 0xFF2000 (layout:
 * pbcemu/src/c64/c64_settings.c): for the CPC and the Spectrum the byte
 * `key` (see pbc_launch.h). Their in-game menu writes it back here, so the
 * next start and the launcher's options show the same. Writing stops core 1
 * (flash_safe_execute: core 1 must have called multicore_lockout_victim_init,
 * as the video cores do) and takes a sector erase, ~50 ms: only from a menu.
 * Not in link play, where the games start with fixed options. */
#ifndef PBC_GAME_OPTS_H
#define PBC_GAME_OPTS_H

#include <stdbool.h>
#include <stdint.h>

/* false if the table could not be written (no memory, flash busy) */
bool pbc_game_opts_set_key(const char *game, uint8_t key);

#endif
