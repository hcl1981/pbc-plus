/* cart_store.c -- see cart_store.h */
#include <stddef.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/regs/addressmap.h"
#include "ff.h"
#include "cart_store.h"

#define NAME_MAX   96                /* = ROM_NAME_MAX */
#define INFO_MAGIC 0x444D4250u       /* "PBMD", as rom_store.c of the others */

/* The record of rom_store.c in the other emulators, unchanged. */
typedef struct {
    uint32_t magic;
    uint32_t size;               /* bytes in the ROM region */
    uint32_t file_size;          /* size of the file on the stick */
    uint32_t file_hash;          /* FNV-1a over the first 4 KB of the file */
    char     name[NAME_MAX];
    uint32_t check;              /* FNV-1a over everything above */
} rom_info_t;

_Static_assert(sizeof(rom_info_t) <= FLASH_PAGE_SIZE, "info record must fit one flash page");

/* core/Cartridge.cpp */
const char *c64_cart_parse(const char *path);

static const rom_info_t *flash_info(void)
{
    return (const rom_info_t *)(XIP_BASE + ROM_INFO_OFFSET);
}

static uint32_t fnv1a(uint32_t h, const void *p, size_t n)
{
    const uint8_t *b = (const uint8_t *)p;
    while (n--) {
        h ^= *b++;
        h *= 16777619u;
    }
    return h;
}

static bool info_valid(const rom_info_t *i)
{
    return i->magic == INFO_MAGIC &&
           i->size > 0 && i->size <= ROM_FLASH_SIZE &&
           i->check == fnv1a(2166136261u, i, offsetof(rom_info_t, check));
}

const uint8_t *cart_store_data(void)
{
    return (const uint8_t *)(XIP_BASE + ROM_FLASH_OFFSET);
}

bool cart_store_is_cart_name(const char *name)
{
    const char *dot = strrchr(name, '.');
    return dot && strcasecmp(dot, ".crt") == 0;
}

static bool file_fingerprint(const char *name, uint32_t *size, uint32_t *hash)
{
    char path[NAME_MAX + 2];
    UINT got = 0;

    /* heap: a FIL carries a 4 KB sector buffer, the stack is only 4 KB,
     * and this runs once before the game */
    FIL *f = malloc(sizeof *f);
    uint8_t *buf = malloc(4096);
    bool ok = f && buf;
    snprintf(path, sizeof path, "/%s", name);
    if (ok)
        ok = f_open(f, path, FA_READ) == FR_OK;
    if (ok) {
        *size = (uint32_t)f_size(f);
        ok = f_read(f, buf, 4096, &got) == FR_OK;
        f_close(f);
        *hash = fnv1a(2166136261u, buf, got);
    }
    free(buf);
    free(f);
    return ok;
}

bool cart_store_matches(const char *name)
{
    const rom_info_t *i = flash_info();
    uint32_t size, hash;
    if (!info_valid(i) || strcmp(i->name, name) != 0)
        return false;
    return file_fingerprint(name, &size, &hash) && size == i->file_size && hash == i->file_hash;
}

/* flash_range_* must not run while code executes from XIP. Only core 0 is
 * running at this point, so masking interrupts is enough. */
static void flash_erase(uint32_t off, uint32_t len)
{
    uint32_t irq = save_and_disable_interrupts();
    flash_range_erase(off, len);
    restore_interrupts(irq);
}

static void flash_program(uint32_t off, const uint8_t *src, uint32_t len)
{
    uint32_t irq = save_and_disable_interrupts();
    flash_range_program(off, src, len);
    restore_interrupts(irq);
}

static bool writing;
static uint32_t used;                   /* bytes of the region the banks take */
static cart_progress_fn progress_fn;

bool cart_store_writing(void) { return writing; }

void cart_store_erase(uint32_t size)
{
    used = (size + FLASH_SECTOR_SIZE - 1) & ~(FLASH_SECTOR_SIZE - 1);
    if (used > ROM_FLASH_SIZE)
        used = ROM_FLASH_SIZE;
    /* in steps, with the bar moving: a megabyte takes a few seconds */
    for (uint32_t off = 0; off < used; off += 64 * 1024) {
        uint32_t n = used - off < 64 * 1024 ? used - off : 64 * 1024;
        flash_erase(ROM_FLASH_OFFSET + off, n);
    }
}

void cart_store_write(uint32_t offset, const uint8_t *data, uint32_t len)
{
    static uint8_t page[FLASH_PAGE_SIZE] __aligned(4);
    if (offset + len > used)
        return;                         /* outside the erased space */
    /* whole pages; the chips start on page boundaries */
    uint32_t whole = len & ~(FLASH_PAGE_SIZE - 1);
    if (whole) {
        flash_program(ROM_FLASH_OFFSET + offset, data, whole);
        offset += whole;
        data += whole;
        len -= whole;
    }
    while (len) {
        uint32_t n = len < FLASH_PAGE_SIZE ? len : FLASH_PAGE_SIZE;
        memset(page, 0xFF, sizeof page);
        memcpy(page, data, n);
        flash_program(ROM_FLASH_OFFSET + offset, page, FLASH_PAGE_SIZE);
        offset += n;
        data += n;
        len -= n;
    }
}

void cart_store_progress(uint32_t done, uint32_t total)
{
    if (progress_fn)
        progress_fn(done, total);
}

const char *cart_store_program(const char *name, cart_progress_fn progress)
{
    char path[NAME_MAX + 2];
    uint32_t file_size, file_hash;

    if (strlen(name) >= NAME_MAX)
        return "File name too long";
    if (!file_fingerprint(name, &file_size, &file_hash))
        return "Cannot read file";

    /* Invalidate the info record first: a half-written region must never
     * look like a valid game after a power cut. */
    flash_erase(ROM_INFO_OFFSET, FLASH_SECTOR_SIZE);

    snprintf(path, sizeof path, "/%s", name);
    writing = true;
    used = 0;
    progress_fn = progress;
    const char *err = c64_cart_parse(path);
    writing = false;
    progress_fn = NULL;
    if (err)
        return err;
    if (used == 0)
        return "Empty cartridge";

    /* Commit. */
    static rom_info_t info __aligned(4);
    static uint8_t page[FLASH_PAGE_SIZE] __aligned(4);
    memset(&info, 0, sizeof info);
    info.magic = INFO_MAGIC;
    info.size = used;
    info.file_size = file_size;
    info.file_hash = file_hash;
    strncpy(info.name, name, NAME_MAX - 1);
    info.check = fnv1a(2166136261u, &info, offsetof(rom_info_t, check));
    memset(page, 0xFF, sizeof page);
    memcpy(page, &info, sizeof info);
    flash_program(ROM_INFO_OFFSET, page, FLASH_PAGE_SIZE);
    return NULL;
}
