/* a7800.h -- the Atari 7800 ProSystem for the PicoBoy Color Plus
 *
 * Written for the PicoBoy Color Plus (GPL-3.0-or-later), part of the 2600
 * emulator: the 6502 (cpu6502.c, compiled a second time), the TIA for sound
 * and buttons (tia.c) and the joystick and switch bits of a2600.h are
 * shared. New here: MARIA (display list list, display lists, direct and
 * character graphics, modes 160A/B and 320A-D, eight palettes, DMA time,
 * display list interrupts on the NMI), the 6532 RIOT, 4 KB of RAM and the
 * cartridge: flat up to 48 KB, SuperGame bank switching (with RAM or an
 * extra bank at 4000h), or, as the header says, Absolute (F-18 Hornet) or
 * Activision (Double Dragon, Rampage); from a .a78 (with its header) or a
 * bare image.
 * No BIOS: the game starts at its reset vector, as on the 7800 after the
 * BIOS has checked the cartridge. A POKEY on the cartridge (at 4000h,
 * 450h or 440h, as the header says) adds its sound (pokey.c).
 */
#ifndef A7800_H
#define A7800_H

#include <stdint.h>
#include <stdbool.h>

#define A78_WIDTH 320                   /* lines out: 320 pixels (160 modes doubled) */

/* the cartridge (stays where it is, e.g. in flash): false if unusable */
bool a7800_insert(const uint8_t *rom, uint32_t size);
void a7800_reset(void);
bool a7800_pal(void);                   /* the header asks for PAL (313 lines, 50 Hz) */

/* One frame. line(y, px) for the shown lines (y 0..A78_LINES-1): A78_WIDTH
 * TIA palette indices (colour >> 1). Joysticks and switches: a26_joy,
 * a26_switches (a2600.h; A26_BW is PAUSE here, A26_FIRE2 the right button). */
#define A78_LINES 240
typedef void (*a78_line_fn)(int y, const uint8_t *px);
void a7800_frame(a78_line_fn line, int16_t *audio, int samples);

uint8_t *a7800_ram(void);               /* 4 KB, for the link's comparison */

#endif
