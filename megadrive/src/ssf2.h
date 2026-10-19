/* ssf2.h -- the Super Street Fighter II mapper (Sega 315-5709).
 *
 * SSF2 is 5 MB; the console sees 4 MB, split into eight 512 KB slots. The
 * registers $A130F3..$A130FF pick the bank shown in slots 1..7; the game
 * uses this to swap banks 8 and 9 into the upper megabyte ($300000).
 *
 * The 68K core reads the ROM as ROM_DATA + address in many hand-written
 * places, so a bank table in every access would slow every game down.
 * Instead the RP2350's flash address translation (QMI ATRANS) does it:
 * ROM_DATA points into XIP so that 0..3 MB fall into window 1 (fixed,
 * banks 0..5) and 3..4 MB into window 2, whose flash base is changed on
 * each bank switch -- one register write, no copying. Only for ROMs
 * bigger than 4 MB; all other games keep the plain identity mapping.
 * A restart of the chip resets the translation.
 */
#ifndef PBC_SSF2_H
#define PBC_SSF2_H

#include <stdbool.h>

/* After the ROM is in flash: sets up the mapper and ROM_DATA if the game
 * needs it. Returns true when the mapper is active. */
bool ssf2_init(void);

/* Called by the bus for writes to $A130xx (core/bus/gwenesis_bus.c). */
void gwenesis_mapper_write(unsigned int addr, unsigned int value);

#endif /* PBC_SSF2_H */
