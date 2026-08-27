/* store.c -- Bestwert im Flash.
 *
 * Ablage im vorletzten Sektor: 0x10FFE000..0x10FFEFFF.  Der letzte Sektor
 * bleibt frei, weil die SDK-UF2 bei 0x10FFFF00 einen Zusatzblock ablegt
 * (CLAUDE.md Abschnitt 8).  Geschrieben wird nur bei einem neuen Bestwert,
 * also selten -- Flash haelt das aus.
 */
#include <string.h>
#include <stddef.h>
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "store.h"

#define STORE_OFFSET 0xFFE000u             /* 16 MB - 8 KB                  */
#define STORE_ADDR   (XIP_BASE + STORE_OFFSET)
#define STORE_MAGIC  0x53504F55u           /* "SPOU"                        */

typedef struct {
    uint32_t magic;
    uint32_t version;
    int32_t  score;
    int32_t  height;
    uint32_t crc;
} store_t;

static uint32_t crc_of(const store_t *s)
{
    const uint8_t *p = (const uint8_t *)s;
    uint32_t c = 0x1234u;
    size_t i;
    for (i = 0; i < offsetof(store_t, crc); i++)
        c = (c << 5) + c + p[i];
    return c;
}

void pbc_store_load(int hiscore[2])
{
    const store_t *s = (const store_t *)STORE_ADDR;
    hiscore[0] = 0;
    hiscore[1] = 0;
    if (s->magic != STORE_MAGIC || s->version != 1u)
        return;
    if (crc_of(s) != s->crc)
        return;
    if (s->score < 0 || s->height < 0)
        return;
    hiscore[0] = s->score;
    hiscore[1] = s->height;
}

bool pbc_store_save(const int hiscore[2])
{
    static uint8_t page[FLASH_PAGE_SIZE];
    store_t s;
    uint32_t irq;

    memset(&s, 0, sizeof s);
    s.magic = STORE_MAGIC;
    s.version = 1u;
    s.score = hiscore[0];
    s.height = hiscore[1];
    s.crc = crc_of(&s);

    memset(page, 0xff, sizeof page);
    memcpy(page, &s, sizeof s);

    irq = save_and_disable_interrupts();
    flash_range_erase(STORE_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(STORE_OFFSET, page, FLASH_PAGE_SIZE);
    restore_interrupts(irq);

    return memcmp((const void *)STORE_ADDR, &s, sizeof s) == 0;
}
