/* games.h -- the games on the USB drive, by device. */
#ifndef PBC_GAMES_H
#define PBC_GAMES_H

#define GAME_NAME_MAX 96            /* = ROM_NAME_MAX of the emulators */

/* Counts the games of every device (one pass over the drive root). The
 * volume must be mounted. */
void games_count(int *counts);

/* Lists the games of one device, sorted. Returns the number found; the
 * array stays valid until the next call. */
int  games_list(int device, char (**names)[GAME_NAME_MAX]);

/* The favourites still on the drive, in the same array as games_list();
 * (*devs)[i] is the device of game i. */
int  games_favorites(char (**names)[GAME_NAME_MAX], int **devs);

/* "Sonic (USA, Europe).md" -> title "Sonic", tags "(USA, Europe)" */
void games_title(const char *file, char *title, char *tags, int size);

#endif /* PBC_GAMES_H */
