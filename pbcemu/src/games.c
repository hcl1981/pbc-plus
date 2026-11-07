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
#include "favs.h"

#define GAMES_MAX 400

static char (*names)[GAME_NAME_MAX];

/* Extensions two systems share: .sna and .dsk (Spectrum or MSX, and the
 * CPC), .rom and .bin (MSX or Mega Drive, and the ColecoVision). The first
 * bytes tell them apart: the CPC's files start with "MV - SNA", "MV - CPC"
 * or "EXTENDED", a ColecoVision cartridge with AA 55 or 55 AA. Read once
 * per file. */
static bool ext_is(const char *name, const char *ext)
{
    const char *dot = strrchr(name, '.');
    return dot && strcasecmp(dot, ext) == 0;
}

enum { K_UNKNOWN = -1, K_OTHER, K_CPC, K_COLECO };

static int file_kind(const char *name)
{
    static FIL f;                       /* a FIL carries a 4 KB buffer: not on the stack */
    char path[GAME_NAME_MAX + 2];
    uint8_t head[8];
    UINT got = 0;
    snprintf(path, sizeof path, "/%s", name);
    if (f_open(&f, path, FA_READ) != FR_OK)
        return K_OTHER;
    bool ok = f_read(&f, head, sizeof head, &got) == FR_OK && got == sizeof head;
    f_close(&f);
    if (!ok)
        return K_OTHER;
    if (memcmp(head, "MV - SNA", 8) == 0 || memcmp(head, "MV - CPC", 8) == 0 ||
        memcmp(head, "EXTENDED", 8) == 0)
        return K_CPC;
    if ((head[0] == 0xaa && head[1] == 0x55) || (head[0] == 0x55 && head[1] == 0xaa))
        return K_COLECO;
    return K_OTHER;
}

/* the file is this device's game; *kind caches the look into it */
static bool belongs(const device_t *d, const char *name, int *kind)
{
    if (!device_has_ext(d, name))
        return false;
    bool cpc_ext = ext_is(name, ".sna") || ext_is(name, ".dsk");
    bool cv_ext = ext_is(name, ".rom") || ext_is(name, ".bin");
    if (!cpc_ext && !cv_ext)
        return true;
    if (*kind == K_UNKNOWN)
        *kind = file_kind(name);
    if (cpc_ext)
        return (d->app == APP_CPC) == (*kind == K_CPC);
    return device_is_coleco(d) == (*kind == K_COLECO);
}

/* the Vectrex lists Mine Storm (built in) unless the drive has that file */
static bool builtin_listed(int device)
{
    static FILINFO fi;
    return strcmp(devices[device].exts, ".vec") == 0 && f_stat("/" PBC_VECTREX_BUILTIN, &fi) != FR_OK;
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
        int kind = K_UNKNOWN;
        for (int d = 0; d < device_count; d++)
            if (belongs(&devices[d], fi.fname, &kind))
                counts[d]++;
    }
    f_closedir(&dir);
    for (int d = 0; d < device_count; d++)
        if (builtin_listed(d))
            counts[d]++;
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
        int kind = K_UNKNOWN;
        if (belongs(&devices[device], fi.fname, &kind))
            strcpy(names[n++], fi.fname);
    }
    f_closedir(&dir);
    if (n < GAMES_MAX && builtin_listed(device))
        strcpy(names[n++], PBC_VECTREX_BUILTIN);
    qsort(names, (size_t)n, sizeof *names, by_name);
    return n;
}

int games_favorites(char (**out)[GAME_NAME_MAX], int **devs)
{
    static int dev[FAVS_MAX];
    static FILINFO fi;
    int n = 0;
    if (!names)
        names = malloc(GAMES_MAX * sizeof *names);
    *out = names;
    *devs = dev;
    if (!names)
        return 0;
    for (int i = 0; i < favs_count(); i++) {
        int d = favs_device(i), kind = K_UNKNOWN;
        char path[GAME_NAME_MAX + 2];
        snprintf(path, sizeof path, "/%s", favs_name(i));
        /* still on the drive (or built in) and still that system's game */
        bool builtin = d >= 0 && strcmp(favs_name(i), PBC_VECTREX_BUILTIN) == 0 && builtin_listed(d);
        if (d < 0 || (!builtin && f_stat(path, &fi) != FR_OK) || !belongs(&devices[d], favs_name(i), &kind))
            continue;
        dev[n] = d;
        strcpy(names[n++], favs_name(i));
    }
    return n;
}

void games_title(const char *file, char *title, char *tags, int size)
{
    pbc_game_title(file, title, tags, size);
}
