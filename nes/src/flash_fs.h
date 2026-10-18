/*
 * flash_fs.h
 *
 * Flash layout of PBC NES and the low-level block device that backs both
 * FatFs (diskio.c) and the USB mass-storage export (usb_msc.c).
 * Taken from PicoBoyGB, with the same layout as megadrive -- so both
 * firmwares find the same USB drive and the same ROM region.
 *
 * 16 MB flash map (offsets from XIP_BASE / flash start):
 *
 *   0x000000 .. 0x0FFFFF   (1 MB)    firmware
 *   0x100000 .. 0x5FFFFF   (5 MB)    ROM region (rom_store.h), the running game,
 *                                    read in place by the emulator
 *   0x600000 .. 0xFEFFFF   (~9.9 MB) FatFs partition (this file) -> USB stick
 *   0xFF0000 .. 0xFFFFFF   (64 KB)   reserved (0x10FFFF00 must stay free: the
 *                                    SDK's UF2 writes an extra block there)
 *
 * One FatFs sector == one 4096-byte flash sector, so a sector write maps
 * directly to flash_range_erase(4096) + flash_range_program(4096) with no
 * read-modify-write.
 */

#ifndef PBGB_FLASH_FS_H
#define PBGB_FLASH_FS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PBGB_FS_FLASH_OFFSET   0x600000u
#define PBGB_FS_FLASH_SIZE     (0xFF0000u - PBGB_FS_FLASH_OFFSET)          /* 0x9F0000 */
#define PBGB_FS_SECTOR_SIZE    4096u                    /* == flash erase unit */
#define PBGB_FS_SECTOR_COUNT   (PBGB_FS_FLASH_SIZE / PBGB_FS_SECTOR_SIZE)  /* 2544 */

/* Raw block access used by diskio.c and the USB MSC callbacks.
 * lba is a sector index within the FS region (0 .. PBGB_FS_SECTOR_COUNT-1).
 * Both operate on whole PBGB_FS_SECTOR_SIZE sectors. Return 0 on success. */
int  flash_fs_read (uint32_t lba, void *dst, uint32_t sector_count);
int  flash_fs_write(uint32_t lba, const void *src, uint32_t sector_count);

#ifdef __cplusplus
}
#endif

#endif /* PBGB_FLASH_FS_H */
