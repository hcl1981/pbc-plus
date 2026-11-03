/* favs.h -- favourite games of every system, kept in flash (0xFF9000).
 *
 * A favourite is a file name and its system, the system stored by the hash
 * of its name so that a new system in devices.c moves nothing. Sorted by
 * file name, case ignored. */
#ifndef PBC_FAVS_H
#define PBC_FAVS_H

#include <stdbool.h>
#include "games.h"

#define FAVS_OFFSET 0xFF9000u
#define FAVS_MAX    40

void favs_load(void);
void favs_save(void);               /* only writes when something changed */

int  favs_count(void);
int  favs_device(int i);            /* -1: a system this build does not have */
const char *favs_name(int i);
bool favs_has(int device, const char *name);
/* false: the list is full */
bool favs_set(int device, const char *name, bool on);

#endif
