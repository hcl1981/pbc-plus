/* a2600.h -- the Atari 2600 (VCS) for the PicoBoy Color Plus
 *
 * Written for the PicoBoy Color Plus (GPL-3.0-or-later): 6507 CPU
 * (cpu6502.c), TIA video and sound (tia.c), RIOT 6532 (128 bytes of RAM,
 * timer, joysticks and console switches) and the cartridge with its bank
 * switching: 2 KB, 4 KB, F8, F6, F4 (with or without Superchip RAM), E0
 * (Parker Brothers), FE (Activision), 3F (Tigervision).
 *
 * A frame runs from one VSYNC to the next, as the game makes it (262
 * lines on NTSC), at most 320 lines.
 */
#ifndef A2600_H
#define A2600_H

#include <stdint.h>
#include <stdbool.h>

enum { CART_2K, CART_4K, CART_F8, CART_F6, CART_F4, CART_E0, CART_FE, CART_3F };

/* joystick bits (active high) */
#define A26_UP    0x01
#define A26_DOWN  0x02
#define A26_LEFT  0x04
#define A26_RIGHT 0x08
#define A26_FIRE  0x10

/* console switches (active high: pressed / set) */
#define A26_RESET  0x01
#define A26_SELECT 0x02
#define A26_BW     0x04             /* black and white */
#define A26_DIFF0  0x08             /* left difficulty A (expert) */
#define A26_DIFF1  0x10             /* right difficulty A */

extern uint8_t a26_joy[2];
extern uint8_t a26_switches;

/* the cartridge (stays where it is, e.g. in flash); returns the bank
 * switching scheme, -1 if the size makes no sense */
int a2600_insert(const uint8_t *rom, uint32_t size);
void a2600_reset(void);

/* One frame: `line` gets every line of the frame (y from 0 at the end of
 * VSYNC) as 160 colour indices (palette tia_palette); `audio` gets
 * `samples` samples. Returns the number of lines. */
typedef void (*a26_line_fn)(int y, const uint8_t *pixels, bool blank);
int a2600_frame(a26_line_fn line, int16_t *audio, int samples);

/* the 128 bytes of RAM, for the link's comparison */
uint8_t *a2600_ram(void);

const char *a2600_cart_name(int type);

#endif
