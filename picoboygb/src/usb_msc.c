/*
 * usb_msc.c -- TinyUSB mass-storage callbacks for the PicoBoyGB USB stick.
 *
 * Exports the FatFs flash partition (flash_fs.h) to the host. Logical block
 * size equals the flash sector size (4096). TinyUSB hands us a sector in
 * CFG_TUD_MSC_EP_BUFSIZE-sized chunks (offset grows across a block), so writes
 * are reassembled into a full-sector buffer and flushed once, giving exactly
 * one flash erase+program per 4096-byte block (same strategy as arduino-pico's
 * FatFSUSB).
 */

#include "tusb.h"
#include "flash_fs.h"
#include <string.h>

// One-sector scratch buffer for read/write reassembly.
static uint8_t  s_sect_buf[PBGB_FS_SECTOR_SIZE];
static int32_t  s_sect_num = -1;   // sector currently held in s_sect_buf, or -1

static void flush_pending(void) {
    if (s_sect_num >= 0) {
        flash_fs_write((uint32_t)s_sect_num, s_sect_buf, 1);
        s_sect_num = -1;
    }
}

// Number of logical units: one.
uint8_t tud_msc_get_maxlun_cb(void) {
    return 1;
}

// SCSI INQUIRY response strings.
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16],
                        uint8_t product_rev[4]) {
    (void)lun;
    const char vid[] = "PicoBoy";
    const char pid[] = "GB ROM Stick";
    const char rev[] = "1.0";
    memcpy(vendor_id,  vid, strlen(vid));
    memcpy(product_id, pid, strlen(pid));
    memcpy(product_rev, rev, strlen(rev));
}

// Medium is always present.
bool tud_msc_test_unit_ready_cb(uint8_t lun) {
    (void)lun;
    return true;
}

// Report block count and block size.
void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size) {
    (void)lun;
    *block_count = PBGB_FS_SECTOR_COUNT;
    *block_size  = PBGB_FS_SECTOR_SIZE;
}

// Drive is writable.
bool tud_msc_is_writable_cb(uint8_t lun) {
    (void)lun;
    return true;
}

// READ10: return up to bufsize bytes at (lba, offset).
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                          void *buffer, uint32_t bufsize) {
    (void)lun;
    if (lba >= PBGB_FS_SECTOR_COUNT) return -1;
    if (offset + bufsize > PBGB_FS_SECTOR_SIZE) return -1;

    // If a different sector is buffered for writing, flush it before reusing scratch.
    if (s_sect_num >= 0 && (uint32_t)s_sect_num != lba) flush_pending();

    if (s_sect_num != (int32_t)lba) {
        flash_fs_read(lba, s_sect_buf, 1);
    }
    memcpy(buffer, s_sect_buf + offset, bufsize);
    // Reads do not keep the buffer "dirty"; drop ownership if it was only read.
    if (s_sect_num != (int32_t)lba) s_sect_num = -1;
    return (int32_t)bufsize;
}

// WRITE10: accumulate a full sector, then erase+program once.
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                           uint8_t *buffer, uint32_t bufsize) {
    (void)lun;
    if (lba >= PBGB_FS_SECTOR_COUNT) return -1;
    if (offset + bufsize > PBGB_FS_SECTOR_SIZE) return -1;

    // Full-sector write in one shot.
    if (offset == 0 && bufsize == PBGB_FS_SECTOR_SIZE) {
        flash_fs_write(lba, buffer, 1);
        return (int32_t)bufsize;
    }

    if (s_sect_num == (int32_t)lba) {
        memcpy(s_sect_buf + offset, buffer, bufsize);
    } else {
        flush_pending();
        flash_fs_read(lba, s_sect_buf, 1);   // preserve untouched bytes
        memcpy(s_sect_buf + offset, buffer, bufsize);
        s_sect_num = (int32_t)lba;
    }

    if (offset + bufsize >= PBGB_FS_SECTOR_SIZE) {
        flush_pending();
    }
    return (int32_t)bufsize;
}

// Handle the few SCSI commands TinyUSB does not answer itself.
int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16],
                        void *buffer, uint16_t bufsize) {
    (void)lun; (void)buffer; (void)bufsize;
    switch (scsi_cmd[0]) {
        case SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL:
            return 0;   // OK, no data
        default:
            tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);
            return -1;
    }
}

// Eject / start-stop: flush any buffered sector so nothing is lost.
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject) {
    (void)lun; (void)power_condition; (void)start;
    if (load_eject) flush_pending();
    return true;
}
