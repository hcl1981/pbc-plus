/* rom_store.h -- the running game lives in flash and is executed in place.
 *
 * A ROM picked in the menu is copied unchanged from the FatFs partition
 * into the ROM region (see flash_fs.h for the map). SMS Plus maps its 16 KB
 * pages straight onto that XIP flash, so the ROM itself costs no RAM.
 *
 * A small info record remembers which file is in the region, so starting
 * the same game again does not rewrite the flash. The layout is the same
 * as in megadrive and nes: the firmwares share the USB drive, and each simply
 * reprograms the region when it finds the other one's game there.
 */
#ifndef PBC_ROM_STORE_H
#define PBC_ROM_STORE_H

#include <stdint.h>
#include <stdbool.h>

#ifndef ROM_FLASH_OFFSET          /* combined firmware: common/pbc_multi.cmake */
#define ROM_FLASH_OFFSET  0x100000u
#define ROM_FLASH_SIZE    0x500000u          /* 5 MB */
#endif
#ifndef ROM_INFO_OFFSET           /* combined firmware: its own sector */
#define ROM_INFO_OFFSET   0xFF0000u          /* one 4 KB sector */
#endif

#define ROM_NAME_MAX 96

/* Progress callback: done/total in bytes. */
typedef void (*rom_progress_fn)(uint32_t done, uint32_t total);

/* Name of the file currently in the ROM region, or NULL. */
const char *rom_store_current(void);

/* True if `name` (in the FatFs root) is already in the ROM region. */
bool rom_store_matches(const char *name);

/* Copy `name` into the ROM region. The FatFs volume must be mounted.
 * `scratch` must hold at least 16 KB and is clobbered. Returns NULL on
 * success, otherwise a short error text. */
const char *rom_store_program(const char *name, uint8_t *scratch, rom_progress_fn progress);

/* Pointer to the ROM in XIP flash and its size. */
const uint8_t *rom_store_data(void);
uint32_t       rom_store_size(void);

/* True for .sms and .gg (case-insensitive). */
bool rom_store_is_rom_name(const char *name);

#endif /* PBC_ROM_STORE_H */
