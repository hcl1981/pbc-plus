/*
 * flash_fs.c
 *
 * Low-level flash block device for the FatFs partition (see flash_fs.h).
 * Whole-sector (4096-byte) reads and erase+program writes.
 *
 * Multicore: in USB stick mode only core 0 runs; while a game runs, disk
 * writes park core 1 for each sector (see flash_fs_write).
 */

#include "flash_fs.h"

#include <string.h>
#include <hardware/flash.h>
#include <hardware/sync.h>
#include <pico/multicore.h>
#include <hardware/regs/addressmap.h>   /* XIP_BASE */

int flash_fs_read(uint32_t lba, void *dst, uint32_t sector_count) {
    if (lba + sector_count > PBGB_FS_SECTOR_COUNT) return -1;
    const uint8_t *src = (const uint8_t *)(XIP_BASE + PBGB_FS_FLASH_OFFSET
                                           + (size_t)lba * PBGB_FS_SECTOR_SIZE);
    memcpy(dst, src, (size_t)sector_count * PBGB_FS_SECTOR_SIZE);
    return 0;
}

int flash_fs_write(uint32_t lba, const void *src, uint32_t sector_count) {
    if (lba + sector_count > PBGB_FS_SECTOR_COUNT) return -1;
    const uint8_t *s = (const uint8_t *)src;

    /* While a game runs, core 1 executes from flash as well. Erasing makes
     * the flash unreadable for a few ms, so core 1 is parked in a RAM loop
     * (multicore lockout, armed in c64_video.c) for each sector. In USB stick
     * mode core 1 is not running and masking interrupts is enough. */
    bool park = multicore_lockout_victim_is_initialized(1);

    for (uint32_t i = 0; i < sector_count; ++i) {
        uint32_t off = PBGB_FS_FLASH_OFFSET + (lba + i) * PBGB_FS_SECTOR_SIZE;
        if (park)
            multicore_lockout_start_blocking();
        uint32_t intr = save_and_disable_interrupts();
        flash_range_erase(off, PBGB_FS_SECTOR_SIZE);
        flash_range_program(off, s + (size_t)i * PBGB_FS_SECTOR_SIZE, PBGB_FS_SECTOR_SIZE);
        restore_interrupts(intr);
        if (park)
            multicore_lockout_end_blocking();
    }
    return 0;
}
