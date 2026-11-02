/* pbc_lockstep.c -- see pbc_lockstep.h
 *
 * Stop and wait over a link that can fail in any phase. The master repeats
 * frame N until an answer arrives; the slave takes N in the callback between
 * receiving and sending, so it has N even if its answer is lost. It may then
 * be a frame ahead when the repeat comes, so every answer carries its input
 * of the frame before as well. The slave can never be behind: the master
 * only moves on with an answer to N, and that answer means N has arrived.
 */
#include <string.h>
#include "usblink.h"
#include "pbc_lockstep.h"

#ifdef LS_HOST
uint32_t ls_host_us(void);
#define NOW_US() ls_host_us()
#define IN_RAM(f) f
#else
#include "pico/stdlib.h"
#define NOW_US() time_us_32()
#define IN_RAM(f) __not_in_flash_func(f)
#endif

#define ATTENTION_US    100000u     /* longer than the slowest frame */
#define CONNECT_US       20000u
#define LOST_US        3000000u
#define FIRST_FRAME_US 10000000u    /* the emulators start at their own pace */

#define SLICE 256                   /* bytes of RAM hashed per frame */

enum { PK_NONE, PK_HELLO, PK_FRAME };

typedef struct {
    uint32_t seq;
    uint32_t hash;                  /* HELLO: the configuration */
    uint16_t input, input_prev;
    uint8_t  type, pad[3];
} packet_t;

static struct {
    bool     master, started;
    uint32_t seq;                   /* the frame being agreed on */
    uint32_t config, hash;
    uint16_t input, input_prev;
    /* slave: what the callback got. It only ever runs inside slave_poll(). */
    bool     got, hello_seen;
    uint16_t remote;
    uint32_t hello_hash;
    bool     out_of_sync;
} ls;

static uint32_t slice_hash(const uint8_t *ram, size_t size, uint32_t seq)
{
    uint32_t h = 2166136261u ^ seq;
    if (ram && size) {
        size_t off = (size_t)seq * SLICE % size;
        for (int i = 0; i < SLICE && off + (size_t)i < size; i++)
            h = (h ^ ram[off + i]) * 16777619u;
    }
    return h;
}

void ls_begin(bool master)
{
    memset(&ls, 0, sizeof ls);
    ls.master = master;
    usblink_init();
}

void ls_end(void)
{
    usblink_master_abort();
    usblink_deinit();
}

bool ls_out_of_sync(void) { return ls.out_of_sync; }

/* ---- master ---------------------------------------------------------------- */

static int master_exchange(const packet_t *tx, packet_t *rx, uint32_t wait_us)
{
    if (!usblink_master_begin_attention())
        return -3;
    uint32_t t0 = NOW_US();
    while (!usblink_master_slave_ready()) {
        if (NOW_US() - t0 > wait_us) {
            usblink_master_abort();
            return -1;
        }
    }
    return usblink_master_finish_exchange(tx, sizeof *tx, rx, sizeof *rx);
}

static bool master_frame(uint16_t *remote)
{
    packet_t tx = { .seq = ls.seq, .hash = ls.hash, .input = ls.input, .type = PK_FRAME };
    packet_t rx;
    uint32_t t0 = NOW_US(), limit = ls.seq ? LOST_US : FIRST_FRAME_US;
    for (;;) {
        int n = master_exchange(&tx, &rx, ATTENTION_US);
        if (n == (int)sizeof rx && rx.type == PK_FRAME) {
            if (rx.seq == ls.seq) {
                if (rx.hash != ls.hash)
                    ls.out_of_sync = true;
                *remote = rx.input;
                return true;
            }
            if (rx.seq == ls.seq + 1) {     /* our last try's answer got lost */
                *remote = rx.input_prev;
                return true;
            }
        }
        if (NOW_US() - t0 > limit)
            return false;
    }
}

/* ---- slave ------------------------------------------------------------------ */

/* Between receiving and sending, interrupts off, the master waiting. */
static int IN_RAM(slave_reply)(int rxlen, void *user)
{
    packet_t **p = user;
    const packet_t *rx = p[0];
    packet_t *tx = p[1];
    memset(tx, 0, sizeof *tx);
    if (rxlen != (int)sizeof *rx)
        return (int)sizeof *tx;
    if (rx->type == PK_HELLO) {
        ls.hello_hash = rx->hash;
        ls.hello_seen = true;
        tx->type = PK_HELLO;
        tx->hash = ls.config;
    } else if (rx->type == PK_FRAME && ls.started) {
        tx->type = PK_FRAME;
        tx->seq = ls.seq;
        tx->hash = ls.hash;
        tx->input = ls.input;
        tx->input_prev = ls.input_prev;
        if (rx->seq == ls.seq && !ls.got) {
            if (rx->hash != ls.hash)
                ls.out_of_sync = true;
            ls.remote = rx->input;
            ls.got = true;
        }
    }
    return (int)sizeof *tx;
}

static void slave_poll(void)
{
    packet_t rx, tx;
    packet_t *p[2] = { &rx, &tx };
    if (usblink_slave_request_pending())
        usblink_slave_exchange_cb(&rx, sizeof rx, &tx, sizeof tx, slave_reply, p);
}

static bool slave_frame(uint16_t *remote)
{
    uint32_t t0 = NOW_US(), limit = ls.seq ? LOST_US : FIRST_FRAME_US;
    while (!ls.got) {
        slave_poll();
        if (NOW_US() - t0 > limit)
            return false;
    }
    *remote = ls.remote;
    return true;
}

/* ---- both ------------------------------------------------------------------- */

int ls_connect(uint32_t config)
{
    ls.config = config;
    if (ls.master) {
        packet_t tx = { .hash = config, .type = PK_HELLO }, rx;
        if (master_exchange(&tx, &rx, CONNECT_US) != (int)sizeof rx || rx.type != PK_HELLO)
            return 0;
        return rx.hash == config ? 1 : -1;
    }
    slave_poll();
    if (!ls.hello_seen)
        return 0;
    return ls.hello_hash == config ? 1 : -1;
}

bool ls_frame(uint16_t local, const uint8_t *ram, size_t ram_size, uint16_t *remote)
{
    if (ls.started) {
        ls.seq++;
        ls.input_prev = ls.input;
    }
    ls.started = true;
    ls.hash = slice_hash(ram, ram_size, ls.seq);
    ls.input = local;
    ls.got = false;
    return ls.master ? master_frame(remote) : slave_frame(remote);
}
