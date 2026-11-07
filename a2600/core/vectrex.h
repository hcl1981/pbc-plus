/* vectrex.h -- the GCE Vectrex for the PicoBoy Color Plus
 *
 * Written for the PicoBoy Color Plus (GPL-3.0-or-later), part of the Atari
 * emulator (it shares its platform: video ring, sound, menus). New here:
 * the Motorola 6809 (all instructions and addressing modes, cycle counts
 * after Motorola's tables), the 6522 VIA (timers 1 and 2, PB7 one-shot,
 * shift register on CB2, CA2/CB2 control), the analog part that moves the
 * beam (the DAC on port A, the sample-and-hold multiplexer on port B, the
 * X/Y integrators run by RAMP, ZERO and BLANK, brightness, the joystick
 * comparator) and the AY-3-8910 behind port A (../../msx/core/msx1/
 * ay8910.c), its port A the four buttons.
 *
 * The beam is followed cycle by cycle: each stretch it moves while lit
 * becomes a line, and a frame (30000 cycles, 50 Hz) draws them into an
 * 8-bit brightness picture that fades by half each frame, like the
 * phosphor. The System ROM (with Mine Storm) is built in, as vecx has it;
 * Smith Engineering let the Vectrex ROMs be copied freely for
 * non-commercial use in 1992 (LICENSE.vectrex).
 */
#ifndef VECTREX_H
#define VECTREX_H

#include <stdint.h>
#include <stdbool.h>

#define VEC_W 240                       /* the picture: the whole panel */
#define VEC_H 280

/* the cartridge (copied into RAM, up to 32 KB): false if unusable */
bool vectrex_insert(const uint8_t *rom, uint32_t size);
void vectrex_reset(void);

/* the controller: buttons 1..4 in bits 0..3 (pressed = 1), the stick as
 * -128..127 (right, up positive) */
void vectrex_input(uint8_t buttons, int x, int y);

/* One frame (1/50 s): the CPU, the beam, the sound (`samples` signed
 * samples at AUDIO_RATE). Its lines are handed to vectrex_picture. */
void vectrex_frame(int16_t *audio, int samples);

/* For the other core: if lines came since the last call, fades the
 * phosphor, draws them and returns true. The picture's rows (VEC_W
 * brightness values 0..255) stay as they are until the next call. */
bool vectrex_picture(void);
const uint8_t *vectrex_row(int y);
uint8_t vectrex_row_or(int y);          /* all the row's values ORed */

uint8_t *vectrex_ram(void);             /* 1 KB */

/* The Vectrex's big buffers (word aligned), idle while another system
 * runs: room for that one's cartridge. */
uint8_t *vectrex_arena(uint32_t *size);

#endif
