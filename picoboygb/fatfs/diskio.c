/*
 * diskio.c
 *
 * FatFs <-> physical drive glue for the PicoBoyGB SDK build.
 * Backs FatFs onto the flash region described in flash_fs.h. A single drive
 * (pdrv 0). Sector size is fixed at 4096 (== flash erase unit), so every
 * FatFs sector maps to exactly one flash sector.
 */

#include "ff.h"
#include "diskio.h"
#include "../src/flash_fs.h"

DSTATUS disk_status(BYTE pdrv) {
    if (pdrv != 0) return STA_NOINIT;
    return 0;   /* always ready, never write-protected */
}

DSTATUS disk_initialize(BYTE pdrv) {
    if (pdrv != 0) return STA_NOINIT;
    return 0;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count) {
    if (pdrv != 0) return RES_PARERR;
    if ((uint32_t)sector + count > PBGB_FS_SECTOR_COUNT) return RES_PARERR;
    if (flash_fs_read((uint32_t)sector, buff, count) != 0) return RES_ERROR;
    return RES_OK;
}

#if FF_FS_READONLY == 0
DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count) {
    if (pdrv != 0) return RES_PARERR;
    if ((uint32_t)sector + count > PBGB_FS_SECTOR_COUNT) return RES_PARERR;
    if (flash_fs_write((uint32_t)sector, buff, count) != 0) return RES_ERROR;
    return RES_OK;
}
#endif

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff) {
    if (pdrv != 0) return RES_PARERR;
    switch (cmd) {
        case CTRL_SYNC:
            return RES_OK;                       /* writes are synchronous */
        case GET_SECTOR_COUNT:
            *(LBA_t *)buff = PBGB_FS_SECTOR_COUNT;
            return RES_OK;
        case GET_SECTOR_SIZE:
            *(WORD *)buff = PBGB_FS_SECTOR_SIZE;
            return RES_OK;
        case GET_BLOCK_SIZE:
            *(DWORD *)buff = 1;                  /* erase block = 1 sector */
            return RES_OK;
        default:
            return RES_PARERR;
    }
}

/* No RTC -> FatFs uses the fixed FF_NORTC_* date via this stub is not needed
 * because FF_FS_NORTC=1, but keep a definition available for FF_FS_NORTC=0 builds. */
#if FF_FS_NORTC == 0
DWORD get_fattime(void) {
    /* 2025-01-01 00:00:00 */
    return ((DWORD)(2025 - 1980) << 25) | ((DWORD)1 << 21) | ((DWORD)1 << 16);
}
#endif
