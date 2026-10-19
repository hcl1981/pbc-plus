/* d64_dir.c -- see d64_dir.h */
#include <string.h>
#include "ff.h"
#include "d64_dir.h"

/* Sectors per track on a 35-track 1541 disk. */
static int sectors(int track)
{
    return track <= 17 ? 21 : track <= 24 ? 19 : track <= 30 ? 18 : 17;
}

static long sector_offset(int track, int sector)
{
    long n = 0;
    for (int t = 1; t < track; t++)
        n += sectors(t);
    return (n + sector) * 256L;
}

int d64_list_prg(const char *path, char names[][D64_NAME_MAX + 1], int max)
{
    /* static: a FIL carries a 4 KB sector buffer, the stack is only 4 KB */
    static FIL f;
    static uint8_t blk[256];
    UINT got;
    int n = 0, guard = 0;

    if (f_open(&f, path, FA_READ) != FR_OK)
        return 0;

    int track = 18, sector = 1;     /* the directory always starts at 18/1 */
    while (track && n < max && guard++ < 19) {
        if (track > 35 || sector >= sectors(track) ||
            f_lseek(&f, sector_offset(track, sector)) != FR_OK ||
            f_read(&f, blk, 256, &got) != FR_OK || got != 256)
            break;
        for (int e = 0; e < 8 && n < max; e++) {
            const uint8_t *d = blk + e * 32;
            if ((d[2] & 0x07) != 2 || !(d[2] & 0x80))    /* closed PRG only */
                continue;
            int len = 0;
            while (len < D64_NAME_MAX && d[5 + len] != 0xA0)
                len++;
            if (!len)
                continue;
            memcpy(names[n], d + 5, (size_t)len);
            names[n][len] = 0;
            n++;
        }
        track = blk[0];
        sector = blk[1];
    }
    f_close(&f);
    return n;
}
