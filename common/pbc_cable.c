/* pbc_cable.c -- see pbc_cable.h */
#include <string.h>
#include "pico/stdlib.h"
#include "usblink.h"
#include "pbc_cable.h"

#define MAGIC       0x4C424347u     /* "GCBL" */
#define WAIT_US     20000u          /* a partner that answered lately: its next poll */
#define LONELY_US    1500u          /* nobody answered for a while: don't hold the game up */
#define LONELY_AFTER 2000000u       /* ... after 2 s without an answer */
#define READY_US     3000u          /* the partner polls every scanline (0.1 ms) */

typedef struct { uint32_t magic; uint16_t seq; uint8_t byte, pad; } request_t;
typedef struct { uint32_t magic; uint16_t seq; uint8_t byte, pad; } reply_t;

static bool on;
static pbc_cable_answer_fn answer;
static uint16_t my_seq;
static uint16_t in_seq = 0xffff;    /* the partner's last request, answered */
static uint8_t in_reply;
static uint64_t seen_us;            /* last answer from the partner */
static uint32_t rnd_state = 0x9e3779b9u;

static uint32_t rnd(void)
{
    rnd_state ^= rnd_state << 13;
    rnd_state ^= rnd_state >> 17;
    rnd_state ^= rnd_state << 5;
    return rnd_state;
}

void pbc_cable_begin(pbc_cable_answer_fn fn)
{
    answer = fn;
    rnd_state ^= (uint32_t)time_us_64();
    usblink_init();
    on = true;
}

bool pbc_cable_on(void) { return on; }

/* the partner's request: answered once, the same answer again if it asks
 * again (our reply got lost) */
static int reply_cb(int rxlen, void *user)
{
    void **p = user;
    const request_t *rx = p[0];
    reply_t *tx = p[1];
    memset(tx, 0, sizeof *tx);
    if (rxlen != (int)sizeof *rx || rx->magic != MAGIC)
        return (int)sizeof *tx;
    if (rx->seq != in_seq) {
        in_seq = rx->seq;
        in_reply = answer ? answer(rx->byte) : 0xff;
    }
    tx->magic = MAGIC;
    tx->seq = rx->seq;
    tx->byte = in_reply;
    return (int)sizeof *tx;
}

void pbc_cable_poll(void)
{
    if (!on || !usblink_slave_request_pending())
        return;
    static request_t rx;
    static reply_t tx;
    void *p[2] = { &rx, &tx };
    if (usblink_slave_exchange_cb(&rx, sizeof rx, &tx, sizeof tx, reply_cb, p) >= 0)
        seen_us = time_us_64();
}

bool pbc_cable_transfer(uint8_t out, uint8_t *in)
{
    if (!on)
        return false;
    uint64_t t0 = time_us_64();
    uint32_t limit = seen_us && t0 - seen_us < LONELY_AFTER ? WAIT_US : LONELY_US;
    request_t tx = { MAGIC, ++my_seq, out, 0 };
    while (time_us_64() - t0 < limit) {
        pbc_cable_poll();               /* the partner may be asking us right now */
        if (!usblink_master_begin_attention()) {
            busy_wait_us(50 + rnd() % 200);
            continue;
        }
        uint64_t k = time_us_64();
        bool ready;
        while (!(ready = usblink_master_slave_ready()) && time_us_64() - k < READY_US)
            ;
        if (!ready) {                   /* both knocked, or nobody there */
            usblink_master_abort();
            busy_wait_us(50 + rnd() % 300);
            continue;
        }
        reply_t rx;
        memset(&rx, 0, sizeof rx);
        int n = usblink_master_finish_exchange(&tx, sizeof tx, &rx, sizeof rx);
        if (n == (int)sizeof rx && rx.magic == MAGIC && rx.seq == my_seq) {
            *in = rx.byte;
            seen_us = time_us_64();
            return true;
        }
        busy_wait_us(50 + rnd() % 200);
    }
    return false;
}
