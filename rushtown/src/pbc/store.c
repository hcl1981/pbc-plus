/* store.c -- money, chips and jobs in flash.
 *
 * The sector before the last one: 0x10FFE000. The last one stays free, the
 * SDK's UF2 puts a block of its own at 0x10FFFF00. Written on a finished
 * job, a chip and in the menu, so seldom.
 */
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pbc.h"
#include "../rt.h"

#define STORE_OFFSET 0xFFE000u

/* the saved games lie one after the other in the first page */
#define SLOT(n) ((const void *)(XIP_BASE + STORE_OFFSET + (uint32_t)(n) * sizeof(save_t)))

bool plat_load(save_t *s, int slot)
{
    memcpy(s, SLOT(slot), sizeof *s);
    return s->magic == SAVE_MAGIC;
}

bool plat_save(const save_t *s, int slot)
{
    static uint8_t page[FLASH_PAGE_SIZE];
    _Static_assert(MODES * sizeof(save_t) <= FLASH_PAGE_SIZE, "the saved games must fit one page");
    if (!memcmp(SLOT(slot), s, sizeof *s))
        return true;
    memcpy(page, SLOT(0), sizeof page);         /* the other game stays as it is */
    memcpy(page + (size_t)slot * sizeof *s, s, sizeof *s);
    pbc_display_wait();                     /* nothing else on the buses meanwhile */
    uint32_t irq = save_and_disable_interrupts();
    flash_range_erase(STORE_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(STORE_OFFSET, page, FLASH_PAGE_SIZE);
    restore_interrupts(irq);
    return !memcmp(SLOT(slot), s, sizeof *s);
}
