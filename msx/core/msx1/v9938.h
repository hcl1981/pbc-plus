/* v9938.h -- the MSX2 video chip (Yamaha V9938) for the PicoBoy Color Plus
 *
 * Written for the PicoBoy Color Plus (GPL-3.0-or-later), after the V9938
 * technical data book: 128 KB VRAM, the TMS9918 modes (graphics 1 and 2,
 * multicolour, text 40) and the new ones (graphics 3 to 7, text 80),
 * sprite mode 2 (8 per line, a colour per line), 16 colours out of 512,
 * vertical scrolling, line interrupt and the command engine (copy, fill,
 * line, search, logical operations), which here finishes at once except
 * for the transfers through the CPU.
 *
 * A line comes out as 256 RGB565 pixels; the 512-pixel modes (graphics 5
 * and 6, text 80) average each pair.
 */
#ifndef V9938_H
#define V9938_H

#include <stdint.h>
#include <stdbool.h>

#define V99_WIDTH 256

void v99_reset(void);

/* ports 98h-9Bh */
uint8_t v99_read_data(void);
uint8_t v99_read_status(void);
void v99_write_data(uint8_t v);
void v99_write_ctrl(uint8_t v);
void v99_write_palette(uint8_t v);
void v99_write_indirect(uint8_t v);

/* lines shown: 192 or 212 */
int v99_lines(void);

/* Line y of the frame (0 = first shown line): draws it and raises the
 * line interrupt there. Call for every shown line, then v99_vblank(). */
void v99_line(int y, uint16_t *out);
void v99_vblank(void);
/* the start of a frame: the vertical retrace ends */
void v99_frame_start(void);

/* the interrupt output */
bool v99_irq(void);

/* RGB565 of a 9-bit colour (set by the platform before v99_reset) */
extern uint16_t (*v99_rgb565)(int r, int g, int b);

/* the CPU cycle within the line (0..227), for the HR flag of S2 */
extern int v99_hpos;

#endif
