/* zx.h -- a ZX Spectrum 48K / 128K for the PicoBoy Color Plus
 *
 * Written for the PicoBoy Color Plus (GPL-3.0-or-later). The Z80 is
 * redcode/Z80 (LGPL-3.0-or-later); the ROMs are Amstrad's, built in with
 * their permission (roms/LICENSE.roms).
 *
 * The machine: ROM, 48 KB or 128 KB of RAM (128K: paging through port
 * 7FFDh), the ULA (screen line by line, border per line, keyboard, beeper,
 * EAR input), Kempston joystick at port 1Fh, the AY-3-8912 of the 128K
 * (ports FFFDh/BFFDh), the floating bus. Memory contention is not
 * emulated: code runs a little faster than on the real machine, which
 * only timing-exact effects and a few loaders notice.
 *
 * Loading: snapshots (.sna, .z80 versions 1-3) at once; tapes (.tap, .tzx)
 * through the ROM's own LOAD -- standard blocks in an instant (LD-BYTES is
 * trapped), the rest in real time (tape.c), which zx_tape_busy() reports
 * so that the platform can run faster meanwhile.
 */
#ifndef ZX_H
#define ZX_H

#include <stdint.h>
#include <stdbool.h>

#define ZX_WIDTH   256
#define ZX_SHOWN   280                  /* lines given out: 44 border, 192 screen, 44 border */
#define ZX_BORDER  44

enum { ZX_48K, ZX_128K };

/* the keyboard: 8 half rows (port FEh, address bits A8..A15), 5 keys each;
 * a key is ZX_KEY(row, bit) */
#define ZX_KEY(row, bit) ((row) << 3 | (bit))
extern uint8_t zx_keys[8];              /* bit set: pressed */
static inline void zx_key_set(int k) { zx_keys[k >> 3] |= (uint8_t)(1 << (k & 7)); }
#define ZXK_CAPS   ZX_KEY(0, 0)
#define ZXK_SYM    ZX_KEY(7, 1)
#define ZXK_ENTER  ZX_KEY(6, 0)
#define ZXK_SPACE  ZX_KEY(7, 0)
/* key of a character 0-9, A-Z (either case), or -1 */
int zx_key_of_char(char c);

/* Kempston: bits 0 right, 1 left, 2 down, 3 up, 4 fire */
extern uint8_t zx_kempston;

/* rom48: the 48K ROM, or for the 128K rom128 the two (editor, 48 BASIC) */
void zx_init(int model, const uint8_t *rom48, const uint8_t *rom128_0, const uint8_t *rom128_1);
int  zx_model(void);
void zx_reset(void);

/* the file in flash: false if it is not one the machine can take
 * (a 128K snapshot switches the machine to 128K) */
bool zx_load_snapshot(const uint8_t *d, uint32_t n, bool z80);
bool zx_insert_tape(const uint8_t *d, uint32_t n, bool tzx);
/* types LOAD "" (48K) or picks the Tape Loader (128K) over the next frames */
void zx_autoload(void);
void zx_tape_rewind(void);
bool zx_tape_busy(void);                /* real-time loading going on */

/* One frame (50 Hz). line(y, px) for each of the ZX_SHOWN lines: ZX_WIDTH
 * colour indices 0..15 (8..15 bright); NULL: not drawn. samples: mono
 * 16-bit at the platform's rate, `n` of them for this frame. */
typedef void (*zx_line_fn)(int y, const uint8_t *px);
void zx_frame(zx_line_fn line, int16_t *audio, int n);

/* all of the RAM (the 48K uses banks 5, 2, 0), for link play's check */
#define ZX_RAM_SIZE 0x20000
const uint8_t *zx_ram(void);

/* microseconds per frame: 19968 (48K), 19992 (128K) */
uint32_t zx_frame_us(void);

/* RGB of colour i (0..15) */
uint32_t zx_palette(int i);

#endif
