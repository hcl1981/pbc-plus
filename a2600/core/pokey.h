/* pokey.h -- Atari's POKEY (sound part), for 7800 cartridges that carry one
 *
 * Written for the PicoBoy Color Plus (GPL-3.0-or-later), after Atari's
 * POKEY data sheet: four channels, each a divider of the 64 kHz or 15 kHz
 * base clock or of the 1.79 MHz machine clock (channels 1 and 3), pairs
 * joined into 16-bit dividers, the 4-, 5-, 9- and 17-bit polynomial
 * counters, volume-only output, the two high-pass filters (channel 1 by 3,
 * 2 by 4), STIMER and RANDOM. Not done: the timer interrupts, the serial
 * port, the pot and keyboard inputs.
 */
#ifndef POKEY_H
#define POKEY_H

#include <stdint.h>

void pokey_reset(void);
void pokey_write(uint8_t reg, uint8_t v);
uint8_t pokey_read(uint8_t reg);

/* Runs `clocks` cycles of 1.79 MHz; returns the mean output over them,
 * the four channels summed: 0..60 in 1/256 steps (0..15360). */
int pokey_run(int clocks);

#endif
