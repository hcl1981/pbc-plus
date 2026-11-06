/* pbc_cable.h -- a link cable for one byte at a time, over the USB-C cable
 *
 * For consoles whose games talk over a serial port byte by byte (the Game
 * Boy's link cable): both devices run on their own, like two Game Boys, and
 * only a byte that one of them clocks out goes over the line. Whoever has
 * one knocks (usblink attention), the partner answers within the same
 * exchange with the byte it had ready -- as on a real cable, where both
 * sides swap a byte at once. No fixed master: like the launcher's lobby,
 * the one with something to send asks, and if both ask at once they part
 * by random waits. The two games need not be the same (Red trades with
 * Blue). */
#ifndef PBC_CABLE_H
#define PBC_CABLE_H

#include <stdint.h>
#include <stdbool.h>

/* The answer to the partner's byte `in`: this side's byte. Called with
 * interrupts off, in the middle of an exchange: quick, no flash. */
typedef uint8_t (*pbc_cable_answer_fn)(uint8_t in);

void pbc_cable_begin(pbc_cable_answer_fn answer);
bool pbc_cable_on(void);

/* As often as possible (every scanline): serves the partner's knock. */
void pbc_cable_poll(void);

/* This side clocks `out` (its game is the cable's master): true with the
 * partner's byte in *in, false if no partner answered. Blocks for at most
 * a few milliseconds. */
bool pbc_cable_transfer(uint8_t out, uint8_t *in);

#endif
