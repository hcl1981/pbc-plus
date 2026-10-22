/* cart_store.h -- a cartridge (.crt) lives in flash and is read in place.
 *
 * The first start of a .crt lays its banks out flat in the ROM region the
 * other emulators keep their game in (see their rom_store.h): a ROM
 * cartridge's bank n at n * bank size, an EasyFlash's ROML at 0 and its
 * ROMH at 512 KB. The cartridge emulation (core/Cartridge.cpp) then reads
 * the banks straight from XIP flash, so they cost no RAM. The info record
 * is the one of rom_store.c in the other emulators: they see that the
 * region holds something else and reprogram it, and so does this.
 */
#ifndef PBC_CART_STORE_H
#define PBC_CART_STORE_H

#include <stdint.h>
#include <stdbool.h>

#ifndef ROM_FLASH_OFFSET          /* combined firmware: common/pbc_multi.cmake */
#define ROM_FLASH_OFFSET  0x100000u
#define ROM_FLASH_SIZE    0x500000u          /* 5 MB */
#endif
#ifndef ROM_INFO_OFFSET           /* combined firmware: its own sector */
#define ROM_INFO_OFFSET   0xFF0000u          /* one 4 KB sector */
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*cart_progress_fn)(uint32_t done, uint32_t total);

/* True for .crt (case-insensitive). */
bool cart_store_is_cart_name(const char *name);

/* True if `name` (in the FatFs root) is already in the ROM region. */
bool cart_store_matches(const char *name);

/* Lays out `name` in the ROM region (core/Cartridge.cpp parses it and
 * calls the functions below). The FatFs volume must be mounted, core 1
 * must not run. Returns NULL on success, otherwise a short error text. */
const char *cart_store_program(const char *name, cart_progress_fn progress);

/* For core/Cartridge.cpp while cart_store_program() runs: */
bool cart_store_writing(void);
void cart_store_erase(uint32_t size);                     /* the banks' space */
void cart_store_write(uint32_t offset, const uint8_t *data, uint32_t len);
void cart_store_progress(uint32_t done, uint32_t total);   /* in file bytes */

/* The banks in XIP flash. */
const uint8_t *cart_store_data(void);

#ifdef __cplusplus
}
#endif

#endif /* PBC_CART_STORE_H */
