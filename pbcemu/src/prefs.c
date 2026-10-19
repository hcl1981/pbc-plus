/* prefs.c -- see prefs.h */
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include <stddef.h>
#include "prefs.h"
#include "devices.h"

#define PREFS_MAGIC 0x32465250u     /* "PRF2": with display modes */

typedef struct {
    uint32_t magic;
    uint8_t  sound, volume, stats, pad;
    uint8_t  video[PREFS_DEVICES];
    uint32_t check;
} stored_t;

prefs_t g_prefs = { true, VOLUME_DEFAULT, false, { 0 } };

static uint32_t checksum(const stored_t *s)
{
    const uint8_t *p = (const uint8_t *)s;
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < offsetof(stored_t, check); i++)
        h = (h ^ p[i]) * 16777619u;
    return h;
}

void prefs_load(void)
{
    const stored_t *s = (const stored_t *)(XIP_BASE + PREFS_OFFSET);
    if (s->magic != PREFS_MAGIC || s->check != checksum(s))
        return;
    g_prefs.sound = s->sound != 0;
    if (s->volume >= VOLUME_MIN && s->volume <= VOLUME_MAX)
        g_prefs.volume = s->volume;
    g_prefs.stats = s->stats != 0;
    memcpy(g_prefs.video, s->video, sizeof g_prefs.video);
}

int prefs_video(int device)
{
    if (device < 0 || device >= PREFS_DEVICES || device >= device_count)
        return 0;
    int n = 0;
    while (devices[device].modes && devices[device].modes[n])
        n++;
    return g_prefs.video[device] < n ? g_prefs.video[device] : 0;
}

void prefs_set_video(int device, int mode)
{
    if (device >= 0 && device < PREFS_DEVICES)
        g_prefs.video[device] = (uint8_t)mode;
}

void prefs_save(void)
{
    static uint8_t page[FLASH_PAGE_SIZE] __aligned(4);
    stored_t s;
    memset(&s, 0, sizeof s);
    s.magic = PREFS_MAGIC;
    s.sound = g_prefs.sound;
    s.volume = (uint8_t)g_prefs.volume;
    s.stats = g_prefs.stats;
    memcpy(s.video, g_prefs.video, sizeof s.video);
    s.check = checksum(&s);
    if (memcmp((const void *)(XIP_BASE + PREFS_OFFSET), &s, sizeof s) == 0)
        return;
    memset(page, 0xFF, sizeof page);
    memcpy(page, &s, sizeof s);
    uint32_t irq = save_and_disable_interrupts();
    flash_range_erase(PREFS_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(PREFS_OFFSET, page, FLASH_PAGE_SIZE);
    restore_interrupts(irq);
}
