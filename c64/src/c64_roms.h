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

#ifdef __cplusplus
}
#endif

#endif
