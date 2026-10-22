/* c64_roms.h -- the player's own Commodore ROMs from the USB drive
 *
 * Built in are the free MEGA65 OpenROMs. Whoever owns the original ROMs can
 * put kernal.rom and basic.rom (optionally chargen.rom) on the drive; they
 * are used instead, for games that need the real KERNAL. Nothing of them is
 * part of this firmware. */
#ifndef C64_ROMS_H
#define C64_ROMS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Needs the volume mounted. True when KERNAL and BASIC were found and the
 * KERNAL is a stock one (patched for the drive emulation in RAM). */
bool c64_roms_load(void);

const uint8_t *c64_roms_basic(void);
const uint8_t *c64_roms_kernal(void);
const uint8_t *c64_roms_chargen(void);   /* NULL: keep the built-in one */

/* Copies the built-in ROMs into the RAM buffers that own ROMs did not
 * fill, so the 6510 and the VIC read them from RAM rather than flash.
 * Afterwards the three getters always return RAM. */
void c64_roms_fill_builtin(const uint8_t *basic_rom, const uint8_t *kernal_rom,
                           const uint8_t *char_rom);

/* Processor-level 1541 ("True drive"): needs the drive's own ROM, 1541.rom
 * (16 KB, also found as 1541.bin / dos1541.rom), on the drive. Loads it
 * into RAM with Frodo's patches. Needs the volume mounted. */
bool c64_roms_load_drive(void);
bool c64_roms_drive_present(void);       /* the file is there (options) */
const uint8_t *c64_roms_drive(void);     /* NULL: drive 8 at DOS level */

/* The player's own KERNAL without the DOS-level drive traps (the real 1541
 * talks over the serial bus). False if the built-in one is in use: the
 * caller puts back its bytes (BuiltinKernalTraps). */
bool c64_roms_untrap_own(void);

#ifdef __cplusplus
}
#endif

#endif
