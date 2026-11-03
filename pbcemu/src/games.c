/* games.c -- see games.h */
#include <stdbool.h>
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

/* .sna and .dsk are both the Spectrum's or MSX's and the CPC's: the CPC's
 * files start with their own signature ("MV - SNA", "MV - CPC",
 * "EXTENDED"), the others do not. Read once per file. */
static bool ext_is(const char *name, const char *ext)
{
    const char *dot = strrchr(name, '.');
    return dot && strcasecmp(dot, ext) == 0;
}

static bool cpc_file(const char *name)
{
    static FIL f;                       /* a FIL carries a 4 KB buffer: not on the stack */
    char path[GAME_NAME_MAX + 2], head[8];
    UINT got = 0;
    snprintf(path, sizeof path, "/%s", name);
    if (f_open(&f, path, FA_READ) != FR_OK)
        return false;
    bool ok = f_read(&f, head, sizeof head, &got) == FR_OK && got == sizeof head;
    f_close(&f);
    return ok && (memcmp(head, "MV - SNA", 8) == 0 || memcmp(head, "MV - CPC", 8) == 0 ||
                  memcmp(head, "EXTENDED", 8) == 0);
}

/* the file is this device's game; *cpc caches the look into it (-1: not yet) */
static bool belongs(const device_t *d, const char *name, int *cpc)
{
    if (!device_has_ext(d, name))
        return false;
    if (d->app != APP_CPC && d->app != APP_ZX && d->app != APP_MSX)
        return true;
    if (!ext_is(name, ".sna") && !ext_is(name, ".dsk"))
        return true;
    if (*cpc < 0)
        *cpc = cpc_file(name);
    return (d->app == APP_CPC) == (*cpc != 0);
}

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
        int cpc = -1;
        for (int d = 0; d < device_count; d++)
            if (belongs(&devices[d], fi.fname, &cpc))
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
        int cpc = -1;
        if (belongs(&devices[device], fi.fname, &cpc))
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
