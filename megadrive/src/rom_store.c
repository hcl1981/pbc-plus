/* rom_store.c -- see rom_store.h */
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/regs/addressmap.h"
#include "ff.h"
#include "rom_store.h"

#define INFO_MAGIC 0x444D4250u   /* "PBMD" */

typedef struct {
    uint32_t magic;
    uint32_t size;               /* bytes in the ROM region (after SMD decode) */
    uint32_t file_size;          /* size of the file on the stick */
    uint32_t file_hash;          /* FNV-1a over the first 4 KB of the file */
    char     name[ROM_NAME_MAX];
    uint32_t check;              /* FNV-1a over everything above */
} rom_info_t;

_Static_assert(sizeof(rom_info_t) <= FLASH_PAGE_SIZE, "info record must fit one flash page");

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

const char *rom_store_current(void)
{
    const rom_info_t *i = flash_info();
    return info_valid(i) ? i->name : NULL;
}

const uint8_t *rom_store_data(void)
{
    return (const uint8_t *)(XIP_BASE + ROM_FLASH_OFFSET);
}

uint32_t rom_store_size(void)
{
    const rom_info_t *i = flash_info();
    return info_valid(i) ? i->size : 0;
}

static bool ends_with_ci(const char *s, const char *ext)
{
    size_t n = strlen(s), m = strlen(ext);
    if (n < m)
        return false;
    s += n - m;
    for (size_t k = 0; k < m; k++) {
        char a = s[k], b = ext[k];
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (a != b)
            return false;
    }
    return true;
}

bool rom_store_is_rom_name(const char *name)
{
    return ends_with_ci(name, ".md") || ends_with_ci(name, ".bin") ||
           ends_with_ci(name, ".gen") || ends_with_ci(name, ".smd");
}

static bool file_fingerprint(const char *name, uint32_t *size, uint32_t *hash, uint8_t *buf)
{
    char path[ROM_NAME_MAX + 2];
    /* static: a FIL carries a 4 KB sector buffer, the stack is only 4 KB */
    static FIL f;
    UINT got = 0;

    snprintf(path, sizeof path, "/%s", name);
    if (f_open(&f, path, FA_READ) != FR_OK)
        return false;
    *size = (uint32_t)f_size(&f);
    if (f_read(&f, buf, 4096, &got) != FR_OK) {
        f_close(&f);
        return false;
    }
    f_close(&f);
    *hash = fnv1a(2166136261u, buf, got);
    return true;
}

bool rom_store_matches(const char *name)
{
    static uint8_t buf[4096];
    const rom_info_t *i = flash_info();
    uint32_t size, hash;

    if (!info_valid(i) || strcmp(i->name, name) != 0)
        return false;
    if (!file_fingerprint(name, &size, &hash, buf))
        return false;
    return size == i->file_size && hash == i->file_hash;
}

/* flash_range_* must not run while code executes from XIP. Only core 0 is
 * running at this point (core 1 is started with the emulator), so masking
 * interrupts is enough. */
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

const char *rom_store_program(const char *name, uint8_t *scratch, rom_progress_fn progress)
{
    char path[ROM_NAME_MAX + 2];
    /* static: a FIL carries a 4 KB sector buffer, the stack is only 4 KB */
    static FIL f;
    UINT got;
    bool smd = ends_with_ci(name, ".smd");
    uint8_t *in  = scratch;            /* 16 KB */
    uint8_t *out = scratch + 16384;    /* 16 KB */
    uint32_t file_size, data_size, file_hash = 2166136261u;

    if (strlen(name) >= ROM_NAME_MAX)
        return "Dateiname zu lang";

    snprintf(path, sizeof path, "/%s", name);
    if (f_open(&f, path, FA_READ) != FR_OK)
        return "Datei nicht lesbar";
    file_size = (uint32_t)f_size(&f);

    /* SMD dumps carry a 512-byte header, then 16 KB blocks with the odd
     * bytes in the first half and the even bytes in the second. */
    data_size = smd ? file_size - 512 : file_size;
    if (file_size < 0x200 || (smd && (file_size < 512 + 16384 || data_size % 16384)))
        { f_close(&f); return "Keine gueltige ROM"; }
    if (data_size > ROM_FLASH_SIZE)
        { f_close(&f); return "ROM groesser als 5 MB"; }

    /* Invalidate the info record first: a half-written region must never
     * look like a valid game after a power cut. */
    flash_erase(ROM_INFO_OFFSET, FLASH_SECTOR_SIZE);

    if (smd) {
        if (f_read(&f, in, 512, &got) != FR_OK || got != 512)
            { f_close(&f); return "Lesefehler"; }
        file_hash = fnv1a(file_hash, in, 512);
    }

    uint32_t done = 0;
    while (done < data_size) {
        uint32_t chunk = data_size - done;
        if (chunk > 16384) chunk = 16384;

        if (f_read(&f, in, chunk, &got) != FR_OK || got != chunk)
            { f_close(&f); return "Lesefehler"; }
        if (done + (smd ? 512 : 0) < 4096) {
            uint32_t head = (smd ? 512 : 0) + done;
            uint32_t take = 4096 - head < chunk ? 4096 - head : chunk;
            file_hash = fnv1a(file_hash, in, take);
        }

        if (smd) {
            /* decode and byte-swap in one go: swapped[2i] = even[i]... */
            for (uint32_t i = 0; i < 8192; i++) {
                out[2 * i]     = in[i];          /* odd byte  -> low  */
                out[2 * i + 1] = in[8192 + i];   /* even byte -> high */
            }
        } else {
            for (uint32_t i = 0; i + 1 < chunk; i += 2) {
                out[i]     = in[i + 1];
                out[i + 1] = in[i];
            }
            if (chunk & 1)
                out[chunk - 1] = in[chunk - 1];
        }

        /* Pad the tail of the last chunk to whole pages. */
        uint32_t prog = (chunk + FLASH_PAGE_SIZE - 1) & ~(FLASH_PAGE_SIZE - 1);
        if (prog > chunk)
            memset(out + chunk, 0xFF, prog - chunk);

        uint32_t erase = (chunk + FLASH_SECTOR_SIZE - 1) & ~(FLASH_SECTOR_SIZE - 1);
        flash_erase(ROM_FLASH_OFFSET + done, erase);
        flash_program(ROM_FLASH_OFFSET + done, out, prog);

        done += chunk;
        if (progress)
            progress(done, data_size);
    }
    f_close(&f);

    /* Verify: the region must read back exactly what was written. */
    if (f_open(&f, path, FA_READ) == FR_OK) {
        const uint8_t *rom = rom_store_data();
        bool ok = true;
        if (smd)
            f_lseek(&f, 512);
        for (uint32_t pos = 0; ok && pos < data_size; pos += 16384) {
            uint32_t chunk = data_size - pos < 16384 ? data_size - pos : 16384;
            if (f_read(&f, in, chunk, &got) != FR_OK || got != chunk) { ok = false; break; }
            if (smd) {
                for (uint32_t i = 0; i < 8192 && ok; i++)
                    ok = rom[pos + 2 * i] == in[i] && rom[pos + 2 * i + 1] == in[8192 + i];
            } else {
                for (uint32_t i = 0; i + 1 < chunk && ok; i += 2)
                    ok = rom[pos + i] == in[i + 1] && rom[pos + i + 1] == in[i];
            }
        }
        f_close(&f);
        if (!ok)
            return "Flash-Pruefung fehlgeschlagen";
    }

    /* Commit. */
    static rom_info_t info __aligned(4);
    static uint8_t page[FLASH_PAGE_SIZE] __aligned(4);
    memset(&info, 0, sizeof info);
    info.magic = INFO_MAGIC;
    info.size = data_size;
    info.file_size = file_size;
    info.file_hash = file_hash;
    strncpy(info.name, name, ROM_NAME_MAX - 1);
    info.check = fnv1a(2166136261u, &info, offsetof(rom_info_t, check));
    memset(page, 0xFF, sizeof page);
    memcpy(page, &info, sizeof info);
    flash_program(ROM_INFO_OFFSET, page, FLASH_PAGE_SIZE);
    return NULL;
}
