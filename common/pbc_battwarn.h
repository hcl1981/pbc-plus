/* pbc_battwarn.h -- the empty-battery warning in the games (PBCemu)
 *
 * When the batteries are as low as the launcher shows them with no bar
 * left (pbcemu/src/battery.c, read here the same way), the emulators blink
 * its battery symbol at the same place, top right, in red and struck
 * through, twice a second. On the panel's black border it is drawn
 * straight into the border between two frames; where the game's picture
 * lies it is mixed into the picture's rows as they go out. On USB power
 * or with fresh batteries nothing happens. All on core 1, which draws. */
#ifndef PBC_BATTWARN_H
#define PBC_BATTWARN_H

#include <stdint.h>
#include <stdbool.h>

#if PBC_MULTI
/* Before a frame's window opens: where on the panel the picture goes. */
void pbc_battwarn_frame(int x, int y, int w, int h);

/* True while the symbol has to go into the picture's rows (an emulator
 * that sends a whole frame in one piece sends it row by row then). */
bool pbc_battwarn_rows(void);

/* A row of the picture right before it is sent: panel row `y`, its first
 * pixel at panel column `x`, `w` pixels. */
void pbc_battwarn_row(uint16_t *row, int y, int x, int w);
#else
/* the standalone firmwares: none */
static inline void pbc_battwarn_frame(int x, int y, int w, int h) { (void)x; (void)y; (void)w; (void)h; }
static inline bool pbc_battwarn_rows(void) { return false; }
static inline void pbc_battwarn_row(uint16_t *row, int y, int x, int w) { (void)row; (void)y; (void)x; (void)w; }
#endif

#endif
