/* cpc.h -- an Amstrad CPC 6128 for the PicoBoy Color Plus
 *
 * Written for the PicoBoy Color Plus (GPL-3.0-or-later). The Z80 is
 * redcode/Z80 (LGPL-3.0-or-later); the ROMs (OS, BASIC 1.1, AMSDOS) are
 * Amstrad's and Locomotive's, built in with Amstrad's permission
 * (roms/LICENSE.roms).
 *
 * The machine: Z80 at 4 MHz with every instruction stretched to whole
 * microseconds (the gate array's wait states), 128 KB of RAM with the
 * 6128's banking, lower and upper ROMs, the gate array (inks, modes 0-2,
 * interrupt every 52 lines), the CRTC 6845 line by line (no mid-line
 * tricks), the 8255 PPI with keyboard and joystick, the AY-3-8912, the
 * cassette (.cdt, real time with motor control) and drive A (fdc.c).
 *
 * Lines go out 640 wide (mode 2 pixels; a mode 1 pixel is two of them,
 * mode 0 four), as hardware colour numbers 0..31, 280 of them: 40 lines
 * above the standard screen, its 200, 40 below.
 */
#ifndef CPC_H
#define CPC_H

#include <stdint.h>
#include <stdbool.h>

#define CPC_WIDTH  640
#define CPC_SHOWN  280

/* the keyboard: 10 rows of 8 (row 9 is the joystick); bit set: pressed */
#define CPC_KEY(row, bit) ((row) << 3 | (bit))
extern uint8_t cpc_keys[10];
static inline void cpc_key_set(int k) { cpc_keys[k >> 3] |= (uint8_t)(1 << (k & 7)); }
#define CPCK_RETURN CPC_KEY(2, 2)
#define CPCK_SPACE  CPC_KEY(5, 7)
#define CPCK_SHIFT  CPC_KEY(2, 5)
#define CPCK_COPY   CPC_KEY(1, 1)
#define CPCK_ESC    CPC_KEY(8, 2)
#define CPCK_UP     CPC_KEY(0, 0)
#define CPCK_RIGHT  CPC_KEY(0, 1)
#define CPCK_DOWN   CPC_KEY(0, 2)
#define CPCK_LEFT   CPC_KEY(1, 0)
#define CPCK_JOY_UP    CPC_KEY(9, 0)
#define CPCK_JOY_DOWN  CPC_KEY(9, 1)
#define CPCK_JOY_LEFT  CPC_KEY(9, 2)
#define CPCK_JOY_RIGHT CPC_KEY(9, 3)
#define CPCK_JOY_FIRE2 CPC_KEY(9, 4)
#define CPCK_JOY_FIRE1 CPC_KEY(9, 5)
/* key of a character (letters either case, digits, a few signs), with
 * SHIFT in bit 8; -1 if there is none */
int cpc_key_of_char(char c);

/* rom: OS + BASIC 1.1 (32 KB), amsdos: 16 KB */
void cpc_init(const uint8_t *rom, const uint8_t *amsdos);
void cpc_reset(void);

/* the file in flash: false if it is not one */
bool cpc_insert_disk(const uint8_t *d, uint32_t n);
bool cpc_insert_tape(const uint8_t *d, uint32_t n);
bool cpc_load_snapshot(const uint8_t *d, uint32_t n);
/* types the start of the disk's program or of the tape over the next
 * frames; false if a disk shows nothing to start */
bool cpc_autoload_disk(void);
void cpc_autoload_tape(void);
/* the text, typed from frame `delay` on ('\n' RETURN, '~' a pause) */
void cpc_type(const char *s, int delay);
void cpc_tape_rewind(void);
bool cpc_tape_busy(void);               /* the motor runs a tape */

typedef void (*cpc_line_fn)(int y, const uint8_t *px);
void cpc_frame(cpc_line_fn line, int16_t *audio, int n);

#define CPC_FRAME_US 19968u

/* RGB of hardware colour i (0..31) */
uint32_t cpc_palette(int i);

#endif
