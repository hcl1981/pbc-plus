/* settings.c -- see settings.h */
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/regs/addressmap.h"
#include "settings.h"
#include "pbc_launch.h"

#define SETTINGS_MAGIC 0x45435050u   /* "PPCE" */

pbc_settings_t g_settings = { true, VOLUME_DEFAULT };

typedef struct {
    uint32_t magic;
    uint32_t sound;
    uint32_t check;
    uint32_t volume;                /* added later: a record without it has 0xFFFFFFFF here */
} stored_t;

static uint32_t checksum(const stored_t *s)
{
    return (s->magic ^ s->sound) * 2654435761u;
}

void settings_load(void)
{
#if PBC_MULTI
    /* Combined firmware: sound and volume are set in the launcher. */
    const pbc_launch_t *l = pbc_launch_get();
    if (l) {
        g_settings.sound = l->sound != 0;
        if (l->volume >= VOLUME_MIN && l->volume <= VOLUME_MAX)
            g_settings.volume = l->volume;
    }
    return;
#endif
    const stored_t *s = (const stored_t *)(XIP_BASE + SETTINGS_OFFSET);
    if (s->magic == SETTINGS_MAGIC && s->check == checksum(s))
    {
        g_settings.sound = s->sound != 0;
        if (s->volume >= VOLUME_MIN && s->volume <= VOLUME_MAX)
            g_settings.volume = (int)s->volume;
    }
}

void settings_save(void)
{
    static uint8_t page[FLASH_PAGE_SIZE] __aligned(4);
    stored_t s = { SETTINGS_MAGIC, g_settings.sound, 0, (uint32_t)g_settings.volume };
    s.check = checksum(&s);
    if (memcmp((const void *)(XIP_BASE + SETTINGS_OFFSET), &s, sizeof s) == 0)
        return;                             /* unchanged: spare the flash */

    memset(page, 0xFF, sizeof page);
    memcpy(page, &s, sizeof s);
    uint32_t irq = save_and_disable_interrupts();
    flash_range_erase(SETTINGS_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(SETTINGS_OFFSET, page, FLASH_PAGE_SIZE);
    restore_interrupts(irq);
}
