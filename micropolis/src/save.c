#include "save.h"
#include "config.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include <string.h>

// Save region lives at the very top of flash.
#define SAVE_OFFSET (PICO_FLASH_SIZE_BYTES - SAVE_REGION_BYTES)
#define SAVE_MAGIC  0x4D435031u   // "MCP1"

// First 8 bytes of the region: magic + payload length.
typedef struct { uint32_t magic; uint32_t len; } save_hdr_t;

void save_init(void) { /* nothing to do; flash is memory-mapped */ }

static const uint8_t *region_ptr(void) {
    return (const uint8_t *)(XIP_BASE + SAVE_OFFSET);
}

bool save_present(void) {
    const save_hdr_t *h = (const save_hdr_t *)region_ptr();
    return h->magic == SAVE_MAGIC && h->len <= (SAVE_REGION_BYTES - sizeof(save_hdr_t));
}

bool save_read(void *buf, size_t len) {
    if (!save_present()) return false;
    const save_hdr_t *h = (const save_hdr_t *)region_ptr();
    if (len > h->len) len = h->len;
    memcpy(buf, region_ptr() + sizeof(save_hdr_t), len);
    return true;
}

bool save_write(const void *buf, size_t len) {
    if (len + sizeof(save_hdr_t) > SAVE_REGION_BYTES) return false;

    // Stage header + payload in a RAM page-aligned buffer.
    static uint8_t page[SAVE_REGION_BYTES];
    save_hdr_t *h = (save_hdr_t *)page;
    h->magic = SAVE_MAGIC;
    h->len   = (uint32_t)len;
    memcpy(page + sizeof(save_hdr_t), buf, len);

    size_t total = sizeof(save_hdr_t) + len;
    // Round up to flash page (256) for program, and sector (4096) for erase.
    size_t prog  = (total + (FLASH_PAGE_SIZE - 1))   & ~(FLASH_PAGE_SIZE - 1);
    size_t erase = (total + (FLASH_SECTOR_SIZE - 1)) & ~(FLASH_SECTOR_SIZE - 1);

    // Park core1 in its RAM-resident lockout handler so it is NOT executing
    // from flash (XIP) while the flash is being erased/programmed. Requires
    // core1 to have called multicore_lockout_victim_init() (see core_sync.c).
    multicore_lockout_start_blocking();
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(SAVE_OFFSET, erase);
    flash_range_program(SAVE_OFFSET, page, prog);
    restore_interrupts(ints);
    multicore_lockout_end_blocking();
    return true;
}
