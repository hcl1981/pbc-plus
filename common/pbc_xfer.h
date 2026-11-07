/* pbc_xfer.h -- a file from one launcher to the other over the USB-C cable.
 *
 * Link play needs the game on both devices. If the partner lacks it, the
 * launcher that starts the game can send it first: the lobby announces it
 * (lobby_send), then both leave the lobby's timer and run this, the sender as
 * the line's master. Packets of 128 bytes, each answered in the same exchange
 * with the offset the receiver wants next (stop-and-wait: a lost packet or
 * answer just repeats). The receiver collects 16 KB in RAM and writes them
 * while the sender waits; at the end it compares a hash over the whole file.
 *
 * test/xfer_test.c runs this on a PC over a link that loses packets.
 */
#ifndef PBC_XFER_H
#define PBC_XFER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

enum { XFER_OK, XFER_CANCEL, XFER_GONE, XFER_BAD, XFER_FULL, XFER_ERROR };

typedef struct {
    uint32_t size, hash;            /* the file: bytes, xfer_hash() over them */
    /* sender: `len` bytes from `off` into buf; 0 or <0 for an error */
    int (*read)(void *user, uint32_t off, void *buf, int len);
    /* receiver: the next `len` bytes; <0: the drive is full / failed */
    int (*write)(void *user, const void *buf, int len);
    /* both, every few ms: show `done` bytes; false = the player cancels */
    bool (*tick)(void *user, uint32_t done);
    /* receiver: answer for a packet that is not ours (the lobby's announcement
     * repeated after its answer got lost); its length, or < 0 */
    int (*foreign)(const void *rx, int rxlen, void *tx, int txmax);
    /* receiver: XFER_FULL refuses at once (no room on the drive) */
    int refuse;
    void *user;
} xfer_t;

uint32_t xfer_hash(uint32_t h, const void *data, size_t n);
#define XFER_HASH_START 2166136261u

int xfer_send(const xfer_t *x);
int xfer_recv(const xfer_t *x);

#endif
