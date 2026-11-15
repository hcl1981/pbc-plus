/* pbc_save.h -- the name of a game's save file on the drive.
 *
 * "/Tetris.gb.sav" for "Tetris.gb": the whole file name, so "Tetris.nes"
 * beside it has its own. Up to PBCemu 1.11 it was "/Tetris.sav" for both,
 * and each overwrote the other's save. That older name (also what other
 * emulators call their saves) is still read as long as the new file does
 * not exist; saving always goes to the new one.
 *
 *   char path[ROM_NAME_MAX + 8], old[ROM_NAME_MAX + 8];
 *   pbc_save_names(game, path, sizeof path, old, sizeof old);
 *   if (f_open(&f, path, FA_READ) == FR_OK || f_open(&f, old, FA_READ) == FR_OK) ...
 */
#ifndef PBC_SAVE_H
#define PBC_SAVE_H

#include <stdio.h>
#include <string.h>

static inline void pbc_save_names(const char *game, char *path, size_t size, char *old, size_t old_size)
{
    snprintf(path, size, "/%s.sav", game);
    snprintf(old, old_size, "/%s", game);
    char *dot = strrchr(old, '.');
    if (dot && dot > old + 1)
        *dot = 0;
    strncat(old, ".sav", old_size - strlen(old) - 1);
}

#endif
