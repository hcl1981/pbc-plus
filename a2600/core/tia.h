/* tia.h -- the Atari 2600's TIA (video and sound)
 *
 * Written for the PicoBoy Color Plus (GPL-3.0-or-later). After Atari's
 * Stella Programmer's Guide: 228 colour clocks per line (68 of horizontal
 * blank, 160 pixels), playfield, two players, two missiles, the ball,
 * collisions, HMOVE, vertical delay; two sound channels.
 *
 * The machine calls tia_write() / tia_read() with the colour clock of the
 * access within the line; the picture is drawn up to that point first
 * ("catching up"), so writes in the middle of a line take effect where the
 * beam is.
 */
#ifndef TIA_H
#define TIA_H

#include <stdint.h>
#include <stdbool.h>

#define TIA_WIDTH 160

/* a finished line of 160 colour indices (0..127, NTSC palette); `blank`:
 * the beam was off (VBLANK) the whole line */
typedef void (*tia_line_fn)(const uint8_t *pixels, bool blank);

void tia_reset(void);
void tia_write(uint8_t reg, uint8_t v, int clock);
uint8_t tia_read(uint8_t reg, int clock);

/* end of a line (228 colour clocks): draws the rest, hands it over */
void tia_end_line(tia_line_fn out);

/* VSYNC on (frame start) since the last call */
bool tia_vsync_started(void);
bool tia_vsync(void);
/* WSYNC was written: the CPU waits for the end of the line */
bool tia_wsync(void);
extern uint8_t tia_wsync_req;          /* the same as a flag */

/* input: fire buttons (INPT4/5), active high */
void tia_set_fire(int player, bool pressed);

/* sound: two updates per line at 31.4 kHz into buf; returns the count */
int tia_audio_line(int16_t *buf);

/* the palette (0x00RRGGBB), index = colour register >> 1: NTSC or PAL */
extern uint32_t tia_palette[128];
void tia_make_palette(bool pal);

#endif
