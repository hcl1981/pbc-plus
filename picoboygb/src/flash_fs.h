/*
 * flash_fs.h
 *
 * Flash layout of the PicoBoyGB SDK build and the low-level block device that
 * backs both FatFs (diskio.c) and the USB mass-storage export (usb_msc.c).
 *
 * 16 MB flash map (offsets from XIP_BASE / flash start):
 *
 *   0x000000 .. 0x0FFFFF   (1 MB)    firmware   (code, well under 1 MB)
 *   0x100000 .. 0x2FFFFF   (2 MB)    ROM region (rom.h: ROM_FLASH_OFFSET, single active .gb)
 *   0x300000 .. 0xEFFFFF   (12 MB)   FatFs partition (this file) -> USB stick
 *   0xF00000 .. 0xFEFFFF   (~1 MB)   unused padding
 *   0xFF0000 .. 0xFFFFFF   (64 KB)   save region (main.cpp: SAVE_FLASH_OFFSET)
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

#define PBGB_FS_FLASH_OFFSET   (3u * 1024u * 1024u)    /* 0x300000 */
#define PBGB_FS_FLASH_SIZE     (12u * 1024u * 1024u)   /* 0xC00000 */
#define PBGB_FS_SECTOR_SIZE    4096u                    /* == flash erase unit */
#define PBGB_FS_SECTOR_COUNT   (PBGB_FS_FLASH_SIZE / PBGB_FS_SECTOR_SIZE)  /* 3072 */

/* Raw block access used by diskio.c and the USB MSC callbacks.
 * lba is a sector index within the FS region (0 .. PBGB_FS_SECTOR_COUNT-1).
 * Both operate on whole PBGB_FS_SECTOR_SIZE sectors. Return 0 on success. */
int  flash_fs_read (uint32_t lba, void *dst, uint32_t sector_count);
int  flash_fs_write(uint32_t lba, const void *src, uint32_t sector_count);

#ifdef __cplusplus
}
#endif

#endif /* PBGB_FLASH_FS_H */
