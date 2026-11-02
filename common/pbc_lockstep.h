/* pbc_lockstep.h -- two PicoBoys run one game frame by frame.
 *
 * Nothing but the buttons travels: both devices emulate the same game from
 * the same start, and before every frame they swap their inputs over
 * usblink (D+/D- of the USB-C cable). Neither starts a frame without the
 * other's input, so both compute exactly the same frames.
 *
 * The link's master keeps the pace, the other follows; it is the device that
 * started the game, whichever controller it plays.
 * This file is the protocol alone, see pbc_link.h for what the emulators
 * call; test/lockstep_test.c runs it on a PC over a link that loses packets.
 */
#ifndef PBC_LOCKSTEP_H
#define PBC_LOCKSTEP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void ls_begin(bool master);
void ls_end(void);

/* One try to meet the partner; call until it is not 0.
 *   1: connected, -1: the partner's `config` differs, 0: nobody yet */
int ls_connect(uint32_t config);

/* Swaps the inputs of the next frame; returns when both have them. `ram`
 * is hashed a slice per frame and compared with the partner's. False: no
 * answer for seconds, the partner is gone. */
bool ls_frame(uint16_t local, const uint8_t *ram, size_t ram_size, uint16_t *remote);

/* The emulations have drifted apart: a RAM slice differed. */
bool ls_out_of_sync(void);

#ifdef __cplusplus
}
#endif

#endif
