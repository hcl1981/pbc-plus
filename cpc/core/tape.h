/* tape.h -- the cassette: .tap and .tzx / .cdt (copied from ../zx)
 *
 * Written for the PicoBoy Color Plus (GPL-3.0-or-later). The tape is read
 * in place (from flash). Two ways to load from it:
 *   - standard blocks (TAP, TZX 10h) go to the ROM's LD-BYTES at once
 *     (zx.c traps it): tape_next_standard();
 *   - everything else plays in real time as a signal on the EAR input,
 *     edge by edge in T-states (3.5 MHz): tape_play(), tape_advance(),
 *     tape_level(). TZX blocks 10h-15h, 20h-25h, 2Ah, 2Bh and the
 *     information blocks; CSW (18h) and generalized data (19h) are skipped.
 */
#ifndef ZX_TAPE_H
#define ZX_TAPE_H

#include <stdint.h>
#include <stdbool.h>

/* false: not a tape (a .tzx without its signature) */
bool tape_insert(const uint8_t *data, uint32_t size, bool tzx);
void tape_rewind(void);
bool tape_present(void);

/* The next block, if it is a standard one (information blocks before it
 * are passed over): its bytes (flag first, checksum last) and length, and
 * the tape moves past it. False if the next block is anything else or the
 * tape is at its end; the position stays. */
bool tape_next_standard(const uint8_t **bytes, uint32_t *len);

/* Whether tape_next_standard() would find one, without moving on. */
bool tape_peek_standard(void);

/* Real-time play. tape_advance() runs the signal on by t T-states; it
 * stops by itself at a "stop the tape" block or the end. */
void tape_play(bool on);
bool tape_playing(void);
void tape_advance(uint32_t t);
int  tape_level(void);
bool tape_at_end(void);

/* The machine is a 48K: TZX block 2Ah (stop the tape if in 48K mode). */
extern bool tape_is_48k;

#endif
