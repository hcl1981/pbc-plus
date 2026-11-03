/* sram.c -- see sram.h */
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "ff.h"
#include "diskio.h"
#include "pbc_link.h"
#include "rom_store.h"
#include "sram.h"

#define SRAM_MAX 0x2000             /* 8 KB: what nearly every game has, or less */

unsigned int m68k_rom_end = 0x800000;

/* The CPU's reads from 2 MB up (m68kcpu.h): ROM straight from the flash
 * below m68k_rom_end, the rest through the bus (I/O, VDP, battery RAM). */
#define HIGH __attribute__((section(".time_critical.sram")))
extern unsigned char *ROM_DATA;
unsigned int m68k_read_memory_8(unsigned int address);
unsigned int m68k_read_memory_16(unsigned int address);

HIGH unsigned int m68ki_read_high_8(unsigned int a)
{
    return a < m68k_rom_end ? ROM_DATA[a ^ 1] : m68k_read_memory_8(a);
}

HIGH unsigned int m68ki_read_high_16(unsigned int a)
{
    return a < m68k_rom_end ? *(const uint16_t *)(ROM_DATA + a) : m68k_read_memory_16(a);
}

HIGH unsigned int m68ki_read_high_32(unsigned int a)
{
    if (a >= m68k_rom_end)
        return m68k_read_memory_16(a) << 16 | m68k_read_memory_16(a + 2);
    uint32_t v = *(const uint32_t *)(ROM_DATA + a);
    return (v << 16) | (v >> 16);
}

static uint8_t __aligned(4) ram[SRAM_MAX];     /* the used bytes, in address order */
static bool     present;
static uint32_t base, hi;           /* first (even) and last address */
static uint32_t size;
static uint8_t  step;               /* 2: only the odd or the even bytes */
static uint8_t  parity;             /* ... which ones (1 = odd) */
static bool     mapped, saving;
static volatile bool dirty;
static uint64_t first_change, last_change;
static FIL     *file;               /* rom_store's, free during the game */
static char     path[ROM_NAME_MAX + 8];

static uint32_t header32(uint32_t a)
{
    const uint8_t *r = rom_store_data();            /* byte-swapped image */
    return (uint32_t)r[a ^ 1] << 24 | (uint32_t)r[(a + 1) ^ 1] << 16 |
           (uint32_t)r[(a + 2) ^ 1] << 8 | r[(a + 3) ^ 1];
}

static void remap(void)
{
    m68k_rom_end = mapped ? base : 0x800000;
}

bool sram_init(const char *game)
{
    const uint8_t *r = rom_store_data();
    if (r[0x1b0 ^ 1] != 'R' || r[0x1b1 ^ 1] != 'A')
        return false;
    uint32_t s = header32(0x1b4), e = header32(0x1b8);
    /* one or two bytes there: a serial EEPROM, which is something else */
    if (s < 0x200000 || e >= 0x400000 || e < s + 2)
        return false;
    int kind = r[0x1b2 ^ 1] & 0x18;                  /* $18 odd, $10 even bytes */
    if (kind == 0x18 || kind == 0x10) {
        step = 2;
        parity = kind == 0x18;
    } else if (!((s ^ e) & 1)) {                    /* no kind given: from the addresses */
        step = 2;
        parity = s & 1;
    } else {
        step = 1;
    }
    base = s & ~1u;
    size = (e - base) / step + 1;
    if (size > SRAM_MAX)
        size = SRAM_MAX;
    hi = base + size * step - 1;

    present = true;
    file = rom_store_file();
    memset(ram, 0xff, size);

    /* link play: both devices start alike, and nothing is kept */
    saving = !pbc_link_active();
    if (saving) {
        snprintf(path, sizeof path, "/%s", game);
        char *dot = strrchr(path, '.');
        if (dot)
            *dot = 0;
        strcat(path, ".sav");
        UINT got;
        if (f_open(file, path, FA_READ) == FR_OK) {
            f_read(file, ram, size, &got);
            f_close(file);
        }
    }
    mapped = rom_store_size() <= base;              /* else after $A130F1 = 1 */
    remap();
    return true;
}

void sram_control(unsigned int value)
{
    if (!present)
        return;
    mapped = value & 1;
    remap();
}

static inline int index_of(unsigned int addr)
{
    if (!mapped || addr < base || addr > hi)
        return -1;
    if (step == 1)
        return (int)(addr - base);
    if ((addr & 1) != parity)
        return -2;                                  /* the other byte of the word */
    return (int)((addr - base) >> 1);
}

int sram_read8(unsigned int addr)
{
    int i = index_of(addr);
    return i == -1 ? -1 : i < 0 ? 0xff : ram[i];
}

bool sram_write8(unsigned int addr, unsigned int value)
{
    int i = index_of(addr);
    if (i == -1)
        return false;
    if (i >= 0 && ram[i] != (uint8_t)value) {
        ram[i] = (uint8_t)value;
        uint64_t now = time_us_64();
        if (!dirty) {
            dirty = true;
            first_change = now;
        }
        last_change = now;
    }
    return true;
}

void sram_poll(void)
{
    if (!dirty || !saving)
        return;
    uint64_t now = time_us_64();
    if (now - last_change < 1000000 && now - first_change < 20000000)
        return;
    dirty = false;
    /* A full drive: the file stays as it was, the game goes on. The file
     * is made `size` long once; then its sectors are written directly,
     * which spares FatFs' writing code. */
    if (f_open(file, path, FA_WRITE | FA_OPEN_ALWAYS) != FR_OK) {
        saving = false;                             /* e.g. no directory entry left */
        return;
    }
    FATFS *fs = file->obj.fs;
    bool ok = f_size(file) >= size || (f_lseek(file, size) == FR_OK && f_size(file) >= size);
    if (!ok)
        saving = false;                             /* no room: no more tries */
    for (uint32_t pos = 0; ok && pos < size; pos += FF_MAX_SS) {
        /* one byte into the sector: the FIL then knows its cluster */
        ok = f_lseek(file, pos + 1) == FR_OK && file->clust >= 2;
        LBA_t sect = fs->database + (LBA_t)fs->csize * (file->clust - 2) + pos / FF_MAX_SS % fs->csize;
        ok = ok && disk_write(0, ram + pos, sect, 1) == RES_OK;
    }
    f_close(file);
    if (!ok) {                                      /* again in a second */
        dirty = true;
        first_change = last_change = now;
    }
}
