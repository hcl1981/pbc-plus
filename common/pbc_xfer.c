/* pbc_xfer.c -- see pbc_xfer.h
 *
 * Sender = master: DATA (offset, up to 128 bytes) until the receiver has
 * everything, then FIN until it says OK or BAD. Receiver = slave: takes DATA
 * at the offset it expects, ignores repeats, answers with the next offset
 * wanted; "busy" while its 16 KB buffer waits to be written (the writing is
 * done outside the exchange, interrupts on). Either side cancels with ABORT;
 * 3 s without a good exchange means the partner is gone.
 */
#include <string.h>
#include "usblink.h"
#include "pbc_xfer.h"

#ifdef LS_HOST
uint32_t ls_host_us(void);
#define NOW_US() ls_host_us()
#include <unistd.h>
#define PAUSE_US(n) usleep(n)
#else
#include "pico/stdlib.h"
#define NOW_US() time_us_32()
#define PAUSE_US(n) sleep_us(n)
#endif

#define MAGIC      0x31465850u      /* "PXF1" */
#define CHUNK      128
#define BUF        16384            /* the receiver writes this much at once */
#define GONE_US    3000000u
#define TICK_US    40000u
#define LINGER_US  600000u          /* the receiver answers repeats of the end */
#define ATTN_US    4000u            /* as usblink: the slave answers CLK within */

enum { X_DATA = 1, X_FIN, X_ABORT };
enum { A_ACK = 1, A_OK, A_BAD, A_FULL, A_ABORTED };

typedef struct {
    uint32_t magic;
    uint8_t  type, pad;
    uint16_t len;
    uint32_t off;
    uint8_t  data[CHUNK];
} pkt_t;
#define HDR ((int)offsetof(pkt_t, data))

typedef struct {
    uint32_t magic;
    uint8_t  type, busy;
    uint16_t pad;
    uint32_t next;
} ack_t;

uint32_t xfer_hash(uint32_t h, const void *data, size_t n)
{
    const uint8_t *p = data;
    while (n--)
        h = (h ^ *p++) * 16777619u;
    return h;
}

/* ---- sender (master) ---------------------------------------------------------- */

static int exchange(const pkt_t *tx, ack_t *rx)
{
    if (!usblink_master_begin_attention())
        return -3;
    uint32_t t0 = NOW_US();
    while (!usblink_master_slave_ready())
        if (NOW_US() - t0 > ATTN_US) {
            usblink_master_abort();
            return -1;
        }
    memset(rx, 0, sizeof *rx);
    return usblink_master_finish_exchange(tx, HDR + tx->len, rx, (int)sizeof *rx);
}

int xfer_send(const xfer_t *x)
{
    static pkt_t tx;
    static ack_t rx;
    uint32_t acked = 0, have = UINT32_MAX;  /* tx.data holds the chunk at `have` */
    uint32_t t_ok = NOW_US(), t_tick = 0;
    for (;;) {
        uint32_t now = NOW_US();
        if (now - t_tick >= TICK_US) {
            t_tick = now;
            if (!x->tick(x->user, acked)) {
                tx.magic = MAGIC;
                tx.type = X_ABORT;
                tx.len = 0;
                for (int i = 0; i < 20; i++) {  /* tell it, if it listens */
                    if (exchange(&tx, &rx) == (int)sizeof rx && rx.magic == MAGIC)
                        break;
                    PAUSE_US(2000);
                }
                return XFER_CANCEL;
            }
        }
        if (now - t_ok > GONE_US)
            return XFER_GONE;
        tx.magic = MAGIC;
        if (acked < x->size) {
            uint32_t n = x->size - acked < CHUNK ? x->size - acked : CHUNK;
            if (have != acked) {
                if (x->read(x->user, acked, tx.data, (int)n) <= 0)
                    return XFER_ERROR;
                have = acked;
            }
            tx.type = X_DATA;
            tx.off = acked;
            tx.len = (uint16_t)n;
        } else {
            tx.type = X_FIN;
            tx.off = x->size;
            tx.len = 0;
        }
        int r = exchange(&tx, &rx);
        if (r != (int)sizeof rx || rx.magic != MAGIC) {
            PAUSE_US(r == -1 ? 500 : 200);  /* no answer: it may be writing */
            continue;
        }
        t_ok = NOW_US();
        switch (rx.type) {
        case A_ACK:
            if (rx.next > acked && rx.next <= x->size)
                acked = rx.next;
            if (rx.busy)
                PAUSE_US(1000);
            break;
        case A_OK: return XFER_OK;
        case A_BAD: return XFER_BAD;
        case A_FULL: return XFER_FULL;
        case A_ABORTED: return XFER_CANCEL;
        default: break;
        }
    }
}

/* ---- receiver (slave) --------------------------------------------------------- */

static struct recv_state {
    const xfer_t *x;
    uint32_t expected, hash, fill;
    bool flush;                     /* the buffer waits to be written */
    uint8_t end;                    /* A_OK, A_BAD, A_FULL, A_ABORTED once over */
    bool cancelled;                 /* by this player */
    uint32_t last_us;
    uint8_t buf[BUF];
} rv;

/* between the master's packet and the answer: interrupts off on the device */
static int reply(int rxlen, void *user)
{
    void **p = user;
    const pkt_t *rx = p[0];
    ack_t *tx = p[1];
    if (rxlen < HDR || rx->magic != MAGIC) {
        if (rv.x->foreign)
            return rv.x->foreign(rx, rxlen, tx, (int)sizeof *tx);
        memset(tx, 0, sizeof *tx);
        return (int)sizeof *tx;
    }
    rv.last_us = NOW_US();
    memset(tx, 0, sizeof *tx);
    tx->magic = MAGIC;
    if (rv.cancelled || rx->type == X_ABORT) {
        rv.end = A_ABORTED;
        tx->type = A_ABORTED;
        return (int)sizeof *tx;
    }
    if (rv.x->refuse == XFER_FULL)
        rv.end = A_FULL;
    if (rv.end) {                   /* over; the master may ask again */
        tx->type = rv.end;
        return (int)sizeof *tx;
    }
    tx->type = A_ACK;
    if (rx->type == X_DATA && rx->off == rv.expected && rx->len <= CHUNK &&
        HDR + rx->len <= rxlen && rv.expected + rx->len <= rv.x->size) {
        if (rv.flush || rv.fill + rx->len > BUF) {
            rv.flush = true;
            tx->busy = 1;           /* full: try again when it is written */
        } else {
            memcpy(rv.buf + rv.fill, rx->data, rx->len);
            rv.fill += rx->len;
            rv.hash = xfer_hash(rv.hash, rx->data, rx->len);
            rv.expected += rx->len;
            if (rv.fill == BUF)
                rv.flush = true;
        }
    } else if (rx->type == X_FIN && rv.expected == rv.x->size) {
        if (rv.fill) {
            rv.flush = true;        /* the rest first */
            tx->busy = 1;
        } else {
            rv.end = rv.hash == rv.x->hash ? A_OK : A_BAD;
            tx->type = rv.end;
        }
    }
    tx->next = rv.expected;
    tx->busy |= rv.flush;
    return (int)sizeof *tx;
}

int xfer_recv(const xfer_t *x)
{
    static pkt_t rx;
    static ack_t tx;
    void *p[2] = { &rx, &tx };
    memset(&rv, 0, offsetof(struct recv_state, buf));
    rv.x = x;
    rv.hash = XFER_HASH_START;
    rv.last_us = NOW_US();
    uint32_t t_tick = 0, t_end = 0;
    for (;;) {
        uint32_t now = NOW_US();
        if (usblink_slave_request_pending()) {
            usblink_slave_exchange_cb(&rx, sizeof rx, &tx, sizeof tx, reply, p);
            continue;
        }
        if (rv.flush && !rv.end) {
            if (x->write(x->user, rv.buf, (int)rv.fill) < 0)
                rv.end = A_FULL;
            rv.fill = 0;
            rv.flush = false;
            continue;
        }
        if (rv.end) {               /* answer repeats for a moment, then go */
            if (!t_end)
                t_end = now;
            if (now - t_end > LINGER_US)
                return rv.end == A_OK ? XFER_OK : rv.end == A_BAD ? XFER_BAD :
                       rv.end == A_FULL ? XFER_FULL : XFER_CANCEL;
            continue;
        }
        if (now - t_tick >= TICK_US) {
            t_tick = now;
            if (!x->tick(x->user, rv.expected)) {
                rv.cancelled = true;
                rv.end = A_ABORTED;
                continue;
            }
        }
        if (now - rv.last_us > GONE_US)
            return XFER_GONE;
    }
}
