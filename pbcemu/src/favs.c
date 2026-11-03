/* favs.c -- see favs.h */
#include <string.h>
#include <strings.h>
#include <stddef.h>
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "favs.h"
#include "devices.h"

#define FAVS_MAGIC 0x31564146u      /* "FAV1" */

typedef struct {
    uint32_t system;                /* hash of the device's name */
    char name[GAME_NAME_MAX];
} fav_t;

typedef struct {
    uint32_t magic, count;
    fav_t fav[FAVS_MAX];
    uint32_t check;
} stored_t;

_Static_assert(sizeof(stored_t) <= FLASH_SECTOR_SIZE, "one sector");

static stored_t s;

static uint32_t hash(const void *p, size_t n)
{
    const uint8_t *b = p;
    uint32_t h = 2166136261u;
    while (n--)
        h = (h ^ *b++) * 16777619u;
    return h;
}

static uint32_t system_of(int device)
{
    return hash(devices[device].name, strlen(devices[device].name));
}

void favs_load(void)
{
    const stored_t *f = (const stored_t *)(XIP_BASE + FAVS_OFFSET);
    memset(&s, 0, sizeof s);
    s.magic = FAVS_MAGIC;
    if (f->magic != FAVS_MAGIC || f->count > FAVS_MAX ||
        f->check != hash(f, offsetof(stored_t, check)))
        return;
    memcpy(&s, f, sizeof s);
}

void favs_save(void)
{
    s.check = hash(&s, offsetof(stored_t, check));
    if (memcmp((const void *)(XIP_BASE + FAVS_OFFSET), &s, sizeof s) == 0)
        return;
    uint32_t irq = save_and_disable_interrupts();
    flash_range_erase(FAVS_OFFSET, FLASH_SECTOR_SIZE);
    /* whole pages; s is static, so the bytes past it are its neighbours'
     * -- copy into a page buffer for the last one */
    static uint8_t page[FLASH_PAGE_SIZE] __aligned(4);
    size_t full = sizeof s / FLASH_PAGE_SIZE * FLASH_PAGE_SIZE;
    flash_range_program(FAVS_OFFSET, (const uint8_t *)&s, full);
    if (sizeof s > full) {
        memset(page, 0xFF, sizeof page);
        memcpy(page, (const uint8_t *)&s + full, sizeof s - full);
        flash_range_program(FAVS_OFFSET + full, page, FLASH_PAGE_SIZE);
    }
    restore_interrupts(irq);
}

int favs_count(void) { return (int)s.count; }

int favs_device(int i)
{
    for (int d = 0; d < device_count; d++)
        if (system_of(d) == s.fav[i].system)
            return d;
    return -1;
}

const char *favs_name(int i) { return s.fav[i].name; }

static int find(int device, const char *name)
{
    uint32_t sys = system_of(device);
    for (int i = 0; i < (int)s.count; i++)
        if (s.fav[i].system == sys && strcmp(s.fav[i].name, name) == 0)
            return i;
    return -1;
}

bool favs_has(int device, const char *name)
{
    return find(device, name) >= 0;
}

bool favs_set(int device, const char *name, bool on)
{
    int i = find(device, name);
    if (!on) {
        if (i >= 0) {
            memmove(&s.fav[i], &s.fav[i + 1], (s.count - (unsigned)i - 1) * sizeof s.fav[0]);
            s.count--;
            memset(&s.fav[s.count], 0, sizeof s.fav[0]);
        }
        return true;
    }
    if (i >= 0)
        return true;
    if (s.count >= FAVS_MAX)
        return false;
    int at = 0;                     /* sorted by name */
    while (at < (int)s.count && strcasecmp(s.fav[at].name, name) < 0)
        at++;
    memmove(&s.fav[at + 1], &s.fav[at], (s.count - (unsigned)at) * sizeof s.fav[0]);
    memset(&s.fav[at], 0, sizeof s.fav[0]);
    s.fav[at].system = system_of(device);
    strncpy(s.fav[at].name, name, GAME_NAME_MAX - 1);
    s.count++;
    return true;
}
