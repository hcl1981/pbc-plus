/* games.c -- see games.h */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "ff.h"
#include "games.h"
#include "devices.h"
#include "pbc_launch.h"

#define GAMES_MAX 400

static char (*names)[GAME_NAME_MAX];

void games_count(int *counts)
{
    static DIR dir;
    static FILINFO fi;
    for (int d = 0; d < device_count; d++)
        counts[d] = 0;
    if (f_opendir(&dir, "/") != FR_OK)
        return;
    while (f_readdir(&dir, &fi) == FR_OK && fi.fname[0]) {
        if (fi.fattrib & (AM_DIR | AM_HID | AM_SYS) || strlen(fi.fname) >= GAME_NAME_MAX)
            continue;
        for (int d = 0; d < device_count; d++)
            if (device_has_ext(&devices[d], fi.fname))
                counts[d]++;
    }
    f_closedir(&dir);
}

static int by_name(const void *a, const void *b)
{
    return strcasecmp((const char *)a, (const char *)b);
}

int games_list(int device, char (**out)[GAME_NAME_MAX])
{
    static DIR dir;
    static FILINFO fi;
    int n = 0;
    if (!names)
        names = malloc(GAMES_MAX * sizeof *names);
    *out = names;
    if (!names || f_opendir(&dir, "/") != FR_OK)
        return 0;
    while (n < GAMES_MAX && f_readdir(&dir, &fi) == FR_OK && fi.fname[0]) {
        if (fi.fattrib & (AM_DIR | AM_HID | AM_SYS) || strlen(fi.fname) >= GAME_NAME_MAX)
            continue;
        if (device_has_ext(&devices[device], fi.fname))
            strcpy(names[n++], fi.fname);
    }
    f_closedir(&dir);
    qsort(names, (size_t)n, sizeof *names, by_name);
    return n;
}

void games_title(const char *file, char *title, char *tags, int size)
{
    pbc_game_title(file, title, tags, size);
}
