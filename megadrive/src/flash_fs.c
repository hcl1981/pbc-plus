/*
 * flash_fs.c
 *
 * Low-level flash block device for the FatFs partition (see flash_fs.h).
 * Whole-sector (4096-byte) reads and erase+program writes.
 *
 * Multicore: in USB stick mode only core 0 runs; while a game runs, .sav
 * files (sram.c) are written with core 1 parked in RAM for each sector
 * (flash_fs_park(), main.c).
 */

#include "flash_fs.h"

#include <string.h>
#include <hardware/flash.h>
#include <hardware/sync.h>
#include <hardware/regs/addressmap.h>   /* XIP_BASE */
#include <hardware/structs/qmi.h>

/* main.c parks core 1 while a game runs; false: it would not stop */
bool __attribute__((weak)) flash_fs_park(bool on) { (void)on; return true; }

/* The SDK's flash functions re-enter XIP through the bootrom's setup,
 * which also resets the flash clock (QMI timing) to the bootrom's. Put
 * back the one main.c set; from RAM, as the timing changes under XIP. */
static void __no_inline_not_in_flash_func(restore_timing)(uint32_t timing)
{
    qmi_hw->m[0].timing = timing;
}

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
    uint32_t timing = qmi_hw->m[0].timing;

    for (uint32_t i = 0; i < sector_count; ++i) {
        uint32_t off = PBGB_FS_FLASH_OFFSET + (lba + i) * PBGB_FS_SECTOR_SIZE;
        /* a .sav is rewritten as a whole, but only a few of its sectors
         * change: leave identical ones alone (no stall, no wear) */
        if (memcmp((const void *)(XIP_BASE + off), s + (size_t)i * PBGB_FS_SECTOR_SIZE,
                   PBGB_FS_SECTOR_SIZE) == 0)
            continue;
        if (!flash_fs_park(true))
            return -1;
        uint32_t intr = save_and_disable_interrupts();
        flash_range_erase(off, PBGB_FS_SECTOR_SIZE);
        flash_range_program(off, s + (size_t)i * PBGB_FS_SECTOR_SIZE, PBGB_FS_SECTOR_SIZE);
        restore_timing(timing);
        restore_interrupts(intr);
        flash_fs_park(false);
    }
    return 0;
}
