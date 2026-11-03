/* sram.h -- battery-backed cartridge RAM (saved games), kept as a .sav file
 *
 * The cartridge header says whether there is any: "RA" at $1B0, the kind
 * at $1B2 (odd bytes, even bytes or both), first and last address at $1B4
 * and $1B8 (usually $200001-$203FFF, 8 KB in the odd bytes). It is shown
 * at its addresses when the ROM ends below them, otherwise only after the
 * game writes 1 to $A130F1 (e.g. Phantasy Star IV, 3 MB).
 *
 * The file "<game>.sav" next to the ROM holds the used bytes only, in
 * address order, at most 8 KB (more has hardly any game). It is written while the game runs, as in ../gbc: once
 * the RAM has not changed for a second, or has been changing for 20 s.
 * In link play the RAM starts empty on both devices and is not saved:
 * the two emulations must stay alike.
 */
#ifndef PBC_SRAM_H
#define PBC_SRAM_H

#include <stdbool.h>
#include <stdint.h>

/* After the ROM is in place and pbc_link_start(); the FatFs volume must be
 * mounted. False: the game has no such RAM. */
bool sram_init(const char *game);

/* Once per frame: writes the file when it is time. */
void sram_poll(void);

/* The bus: a byte of the cartridge RAM, or -1 when `addr` is ROM. */
int  sram_read8(unsigned int addr);
/* true: the write went to the cartridge RAM */
bool sram_write8(unsigned int addr, unsigned int value);
/* $A130F1 */
void sram_control(unsigned int value);

/* The CPU reads ROM directly below this address (m68kcpu.h); the bus takes
 * the rest, which then may be cartridge RAM. 0x800000 without one. */
extern unsigned int m68k_rom_end;

#endif
