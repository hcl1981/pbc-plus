/*
 * flash_fs.c
 *
 * Low-level flash block device for the FatFs partition (see flash_fs.h).
 * Whole-sector (4096-byte) reads and erase+program writes.
 *
 * NOTE on multicore: all writes here happen while only core 0 is running
 * (USB stick mode and ROM programming both run before core 1 is launched),
 * so it is enough to disable interrupts around the flash operations - there
 * is no second core executing from XIP that could stall.
 */

#include "flash_fs.h"

#include <string.h>
#include <hardware/flash.h>
#include <hardware/sync.h>
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

    for (uint32_t i = 0; i < sector_count; ++i) {
        uint32_t off = PBGB_FS_FLASH_OFFSET + (lba + i) * PBGB_FS_SECTOR_SIZE;
        uint32_t intr = save_and_disable_interrupts();
        flash_range_erase(off, PBGB_FS_SECTOR_SIZE);
        flash_range_program(off, s + (size_t)i * PBGB_FS_SECTOR_SIZE, PBGB_FS_SECTOR_SIZE);
        restore_interrupts(intr);
    }
    return 0;
}
