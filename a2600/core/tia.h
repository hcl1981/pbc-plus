/* tia.h -- the Atari 2600's TIA (video and sound)
 *
 * Written for the PicoBoy Color Plus (GPL-3.0-or-later). After Atari's
 * Stella Programmer's Guide: 228 colour clocks per line (68 of horizontal
 * blank, 160 pixels), playfield, two players, two missiles, the ball,
 * collisions, HMOVE, vertical delay; the paddle inputs (INPT0-3, dumped by
 * VBLANK bit 7); two sound channels.
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

/* a finished line of 160 colour register values (bit 0 unused); `blank`:
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
bool tia_vsync_pending(void);          /* the same, without clearing it */
/* WSYNC was written: the CPU waits for the end of the line */
bool tia_wsync(void);
extern uint8_t tia_wsync_req;          /* the same as a flag */

/* input: fire buttons (INPT4/5), active high */
void tia_set_fire(int player, bool pressed);

/* Paddle i (0..3, INPT0-3): its INPT bit goes high `clocks` colour clocks
 * after VBLANK bit 7 stops dumping; ~0u: no paddle (never high, the
 * default). */
void tia_set_paddle(int i, uint32_t clocks);
/* colour clocks from the end of the last dump to the next one */
uint32_t tia_paddle_window(void);
/* INPT0-3 reads outside the dump since the last call -- 0 if no dump ended
 * in that time. A paddle game polls them through the frame. */
int tia_paddle_reads(void);

/* sound: two updates per line at 31.4 kHz into buf; returns the count */
int tia_audio_line(int16_t *buf);

/* the palette (0x00RRGGBB), index = colour register >> 1: NTSC or PAL */
extern uint32_t tia_palette[128];
void tia_make_palette(bool pal);

#endif
