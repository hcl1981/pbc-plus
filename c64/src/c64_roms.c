/* c64_roms.c -- see c64_roms.h */
#include <stdlib.h>
#include <string.h>
#include "ff.h"
#include "c64_roms.h"

#define BASIC_SIZE  8192
#define KERNAL_SIZE 8192
#define CHAR_SIZE   4096

static uint8_t basic[BASIC_SIZE], kernal[KERNAL_SIZE], chargen[CHAR_SIZE];
static bool loaded, have_char, own;

/* Frodo's DOS-level drive 8 needs traps ($F2 xx) at the IEC routines of the
 * KERNAL, plus a skipped RAM test. Offsets into the ROM; "orig" are the bytes
 * of KERNAL 901227-03 there (also in -02), checked before patching. */
static const struct { uint16_t off; uint8_t orig[2], patch[2]; } kernal_patches[] = {
    { 0x0d40, { 0x78, 0x20 }, { 0xf2, 0x00 } },   /* IECOut */
    { 0x0d23, { 0x78, 0x20 }, { 0xf2, 0x01 } },   /* IECOutATN */
    { 0x0d36, { 0x78, 0x20 }, { 0xf2, 0x02 } },   /* IECOutSec */
    { 0x0e13, { 0x78, 0xa9 }, { 0xf2, 0x03 } },   /* IECIn */
    { 0x0def, { 0x78, 0x20 }, { 0xf2, 0x04 } },   /* IECSetATN */
    { 0x0dbe, { 0xad, 0x00 }, { 0xf2, 0x05 } },   /* IECRelATN */
    { 0x0dcc, { 0x78, 0x20 }, { 0xf2, 0x06 } },   /* IECTurnaround */
    { 0x0e03, { 0x20, 0xbe }, { 0xf2, 0x07 } },   /* IECRelease */
    { 0x1d84, { 0xd0, 0xe8 }, { 0xa0, 0x00 } },   /* fast reset */
};

/* Reads the first of the given names that exists with exactly `size` bytes. */
static bool load(const char *const *names, uint8_t *buf, UINT size)
{
    static FIL f;           /* a FIL carries a 4 KB sector buffer */
    for (; *names; names++) {
        if (f_open(&f, *names, FA_READ) != FR_OK)
            continue;
        UINT got = 0;
        bool ok = f_size(&f) == size && f_read(&f, buf, size, &got) == FR_OK && got == size;
        f_close(&f);
        if (ok)
            return true;
    }
    return false;
}

static const char *const basic_names[]   = { "/basic.rom", "/basic.bin", "/basic", NULL };
static const char *const kernal_names[]  = { "/kernal.rom", "/kernal.bin", "/kernal", NULL };
static const char *const chargen_names[] = { "/chargen.rom", "/chargen.bin", "/chargen", NULL };

bool c64_roms_load(void)
{
    if (!load(kernal_names, kernal, KERNAL_SIZE) || !load(basic_names, basic, BASIC_SIZE))
        return false;
    for (unsigned i = 0; i < sizeof kernal_patches / sizeof kernal_patches[0]; i++)
        if (memcmp(kernal + kernal_patches[i].off, kernal_patches[i].orig, 2) != 0)
            return false;   /* not a stock KERNAL: the traps would land elsewhere */
    for (unsigned i = 0; i < sizeof kernal_patches / sizeof kernal_patches[0]; i++)
        memcpy(kernal + kernal_patches[i].off, kernal_patches[i].patch, 2);
    have_char = load(chargen_names, chargen, CHAR_SIZE);
    loaded = own = true;
    return true;
}

void c64_roms_fill_builtin(const uint8_t *basic_rom, const uint8_t *kernal_rom,
                           const uint8_t *char_rom)
{
    if (!loaded) {
        memcpy(basic, basic_rom, BASIC_SIZE);
        memcpy(kernal, kernal_rom, KERNAL_SIZE);   /* traps already in place */
        loaded = true;
    }
    if (!have_char) {
        memcpy(chargen, char_rom, CHAR_SIZE);
        have_char = true;
    }
}

/* ---- the 1541's own ROM ---- */

#define DRIVE_SIZE 16384
static uint8_t *drive;

/* Frodo's patches (C64::patch_roms in core/rp2350/C64_rp2350.cpp): no ROM
 * checksum test, and a trap in the DOS idle loop, so the 1541 CPU sleeps
 * while the drive has nothing to do. */
static const struct { uint16_t off; uint8_t n, b[2]; } drive_patches[] = {
    { 0x2ae4, 2, { 0xea, 0xea } },
    { 0x2ae8, 2, { 0xea, 0xea } },
    { 0x2c9b, 2, { 0xf2, 0x00 } },
};

static const char *const drive_names[] = { "/1541.rom", "/1541.bin", "/dos1541.rom", "/1541", NULL };

bool c64_roms_load_drive(void)
{
    if (!drive)
        drive = malloc(DRIVE_SIZE);
    if (!drive || !load(drive_names, drive, DRIVE_SIZE)) {
        free(drive);
        drive = NULL;
        return false;
    }
    for (unsigned i = 0; i < sizeof drive_patches / sizeof drive_patches[0]; i++)
        memcpy(drive + drive_patches[i].off, drive_patches[i].b, drive_patches[i].n);
    return true;
}

bool c64_roms_drive_present(void)
{
    static FILINFO fi;
    for (const char *const *n = drive_names; *n; n++)
        if (f_stat(*n, &fi) == FR_OK && fi.fsize == DRIVE_SIZE)
            return true;
    return false;
}

const uint8_t *c64_roms_drive(void) { return drive; }

bool c64_roms_untrap_own(void)
{
    if (!own)
        return false;
    for (unsigned i = 0; i < sizeof kernal_patches / sizeof kernal_patches[0]; i++)
        if (kernal_patches[i].patch[0] == 0xf2)
            memcpy(kernal + kernal_patches[i].off, kernal_patches[i].orig, 2);
    return true;
}

const uint8_t *c64_roms_basic(void)   { return basic; }
const uint8_t *c64_roms_kernal(void)  { return kernal; }
const uint8_t *c64_roms_chargen(void) { return have_char ? chargen : NULL; }
