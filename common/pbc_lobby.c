/* pbc_lobby.c -- see pbc_lobby.h
 *
 * The line has no fixed master here. Each device listens, and when its time
 * comes it raises CLK and waits briefly for the partner's answer. If both
 * knock at once neither answers; the random gaps part them. A partner that
 * is knocking itself sees CLK high and becomes the slave of this exchange.
 *
 * The knock is short and rare because a PC on the other end of the cable
 * sees it too, as a device that comes and goes before the 100 ms debounce of
 * a USB port is over.
 *
 * Starting a game: OFFER is repeated until the partner has answered. An
 * accepted offer is followed by GO, which sends the partner off. Should the
 * answer to GO get lost, the master starts anyway after a moment; the games
 * meet in pbc_link_start() in any case.
 */
#include <stddef.h>
#include <string.h>
#include "usblink.h"
#include "pbc_lobby.h"

#ifdef LS_HOST
uint32_t ls_host_ms(void);
#define NOW_MS() ls_host_ms()
#define LOCK() 0u
#define UNLOCK(s) ((void)(s))
#else
#include "pico/stdlib.h"
#include "hardware/sync.h"
#define NOW_MS() to_ms_since_boot(get_absolute_time())
#define LOCK() save_and_disable_interrupts()
#define UNLOCK(s) restore_interrupts(s)
#endif

#define MAGIC        0x324B4C50u    /* "PLK2" (with SEND); same firmware on both */
#define KNOCK_MS     5              /* > the partner's tick */
#define KNOCK_EVERY  700            /* + up to KNOCK_SPREAD */
#define KNOCK_SPREAD 600
#define OFFER_EVERY  30
#define HOLD_MS      20             /* after a failed exchange: line in odd state */
#define SEEN_MS      3000           /* the partner is gone */
#define OFFER_MS     1500           /* an offer not repeated is withdrawn */
#define GO_MS        300

enum { T_NONE, T_HELLO, T_OFFER, T_GO, T_SEND };        /* master */
enum { R_NONE, R_HELLO, R_WAIT, R_ACCEPT, R_NO_GAME, R_BUSY, R_GO, R_SEND_GO };   /* slave */

typedef struct {
    uint32_t magic, id, nonce;
    uint8_t  type, device, player, pad;
    char     game[LOBBY_NAME_MAX];  /* OFFER and SEND only: HELLO and GO stop before it */
    uint32_t size, hash;            /* SEND: the file */
} packet_t;

typedef struct {
    uint32_t magic, nonce;
    uint8_t  type, pad[3];
} reply_t;

enum { OFFER_NONE, OFFER_ASK, OFFER_GO };

static struct {
    uint32_t id, rnd, nonce;
    uint32_t next_ms, hold_ms, knock_ms, seen_ms;
    bool     knocking, seen;
    /* ours */
    int      offer, result;
    uint32_t go_ms;
    lobby_offer_t mine;
    /* a file we send */
    bool     send;
    int      send_result;
    uint32_t send_nonce;
    lobby_file_t send_file;
    /* a file the partner sends */
    bool     sin_valid, sin_taken;
    uint32_t sin_id, sin_nonce, sin_ms;
    lobby_file_t sin;
    /* the partner's */
    bool     in_valid, in_go;
    uint8_t  in_answer;
    uint32_t in_id, in_nonce, in_ms;
    lobby_offer_t in;
} lb;

static uint32_t rnd(void)
{
    lb.rnd = lb.rnd * 1103515245u + 12345u;
    return lb.rnd >> 16;
}

static void saw_partner(uint32_t now)
{
    lb.seen = true;
    lb.seen_ms = now;
}

/* ---- slave ------------------------------------------------------------------ */

/* Between receiving and sending, interrupts off, the master waiting. */
static int slave_reply(int rxlen, void *user)
{
    packet_t **p = user;
    const packet_t *rx = p[0];
    reply_t *tx = (reply_t *)p[1];
    uint32_t now = NOW_MS();

    memset(tx, 0, sizeof *tx);
    if (rxlen < (int)offsetof(packet_t, game) || rx->magic != MAGIC)
        return (int)sizeof *tx;
    tx->magic = MAGIC;
    tx->nonce = rx->nonce;
    saw_partner(now);

    if (rx->type == T_HELLO) {
        tx->type = R_HELLO;
    } else if (rx->type == T_OFFER && rxlen == (int)sizeof *rx) {
        if (lb.offer != OFFER_NONE) {
            if (lb.id < rx->id) {
                tx->type = R_BUSY;      /* ours goes first */
                return (int)sizeof *tx;
            }
            lb.offer = OFFER_NONE;
            lb.result = LOBBY_LOST;
        }
        if (!lb.in_valid || lb.in_id != rx->id || lb.in_nonce != rx->nonce) {
            lb.in_valid = true;
            lb.in_go = false;
            lb.in_answer = R_NONE;
            lb.in_id = rx->id;
            lb.in_nonce = rx->nonce;
            lb.in.device = rx->device;
            lb.in.player = rx->player;
            memcpy(lb.in.game, rx->game, sizeof lb.in.game);
            lb.in.game[sizeof lb.in.game - 1] = 0;
        }
        lb.in_ms = now;
        tx->type = lb.in_answer ? lb.in_answer : R_WAIT;
    } else if (rx->type == T_SEND && rxlen == (int)sizeof *rx) {
        if (lb.offer != OFFER_NONE) {
            tx->type = R_BUSY;          /* we are starting a game ourselves */
            return (int)sizeof *tx;
        }
        if (!lb.sin_valid || lb.sin_id != rx->id || lb.sin_nonce != rx->nonce) {
            lb.sin_valid = true;
            lb.sin_taken = false;
            lb.sin_id = rx->id;
            lb.sin_nonce = rx->nonce;
            memcpy(lb.sin.name, rx->game, sizeof lb.sin.name);
            lb.sin.name[sizeof lb.sin.name - 1] = 0;
            lb.sin.size = rx->size;
            lb.sin.hash = rx->hash;
        }
        lb.sin_ms = now;
        tx->type = R_SEND_GO;
    } else if (rx->type == T_GO && lb.in_valid &&
               lb.in_id == rx->id && lb.in_nonce == rx->nonce) {
        lb.in_go = true;
        lb.in_ms = now;
        tx->type = R_GO;
    }
    return (int)sizeof *tx;
}

/* ---- master ----------------------------------------------------------------- */

static void master_exchange(uint32_t now)
{
    static packet_t tx;
    static reply_t rx;
    tx.magic = MAGIC;
    tx.id = lb.id;
    tx.nonce = lb.nonce;
    int len = (int)offsetof(packet_t, game);
    if (lb.offer == OFFER_NONE && lb.send) {
        tx.type = T_SEND;
        tx.nonce = lb.send_nonce;
        memcpy(tx.game, lb.send_file.name, sizeof tx.game);
        tx.size = lb.send_file.size;
        tx.hash = lb.send_file.hash;
        len = (int)sizeof tx;
    } else if (lb.offer == OFFER_ASK) {
        tx.type = T_OFFER;
        tx.device = (uint8_t)lb.mine.device;
        tx.player = (uint8_t)lb.mine.player;
        memcpy(tx.game, lb.mine.game, sizeof tx.game);
        len = (int)sizeof tx;
    } else {
        tx.type = lb.offer == OFFER_GO ? T_GO : T_HELLO;
    }
    memset(&rx, 0, sizeof rx);
    int n = usblink_master_finish_exchange(&tx, len, &rx, sizeof rx);
    if (n != (int)sizeof rx || rx.magic != MAGIC)
        return;
    saw_partner(now);
    if (tx.type == T_SEND) {
        if (rx.nonce == lb.send_nonce && rx.type == R_SEND_GO) {
            lb.send = false;
            lb.send_result = LOBBY_ACCEPTED;
        } else if (rx.nonce == lb.send_nonce && rx.type == R_BUSY) {
            lb.send = false;
            lb.send_result = LOBBY_LOST;
        }
        return;
    }
    if (rx.nonce != lb.nonce)
        return;
    if (lb.offer == OFFER_ASK && rx.type == R_ACCEPT) {
        lb.offer = OFFER_GO;
        lb.go_ms = now;
    } else if (lb.offer == OFFER_ASK && rx.type == R_NO_GAME) {
        lb.offer = OFFER_NONE;
        lb.result = LOBBY_NO_GAME;
    } else if (lb.offer == OFFER_GO && rx.type == R_GO) {
        lb.offer = OFFER_NONE;
        lb.result = LOBBY_ACCEPTED;
    }
}

/* ---- the timer -------------------------------------------------------------- */

static void schedule(uint32_t now)
{
    lb.next_ms = now + (lb.offer != OFFER_NONE || lb.send ? OFFER_EVERY
                                               : KNOCK_EVERY + rnd() % KNOCK_SPREAD);
}

void lobby_tick(void)
{
    uint32_t now = NOW_MS();

    if (lb.seen && now - lb.seen_ms > SEEN_MS)
        lb.seen = false;
    if (lb.in_valid && now - lb.in_ms > OFFER_MS)
        lb.in_valid = false;
    if (lb.offer == OFFER_ASK && !lb.seen) {
        lb.offer = OFFER_NONE;
        lb.result = LOBBY_GONE;
    }
    if (lb.send && !lb.seen) {
        lb.send = false;
        lb.send_result = LOBBY_GONE;
    }
    if (lb.sin_valid && !lb.sin_taken && now - lb.sin_ms > OFFER_MS)
        lb.sin_valid = false;
    if (lb.offer == OFFER_GO && now - lb.go_ms > GO_MS) {
        lb.offer = OFFER_NONE;
        lb.result = LOBBY_ACCEPTED;
    }

    if (lb.knocking) {
        if (usblink_master_slave_ready()) {
            lb.knocking = false;
            master_exchange(now);
            schedule(now);
        } else if (now - lb.knock_ms > KNOCK_MS) {
            lb.knocking = false;
            usblink_master_abort();
        }
        return;
    }
    if ((int32_t)(now - lb.hold_ms) < 0)
        return;
    if (usblink_slave_request_pending()) {
        static packet_t rx;
        static reply_t tx;
        packet_t *p[2] = { &rx, (packet_t *)&tx };
        int r = usblink_slave_exchange_cb(&rx, sizeof rx, &tx, sizeof tx, slave_reply, p);
        if (r < 0 && r != -4)
            lb.hold_ms = now + HOLD_MS; /* a line held high must not eat the CPU */
        return;
    }
    if ((int32_t)(now - lb.next_ms) >= 0) {
        if (usblink_master_begin_attention()) {
            lb.knocking = true;
            lb.knock_ms = now;
        }
        schedule(now);
    }
}

#ifndef LS_HOST
static repeating_timer_t timer;
static bool timer_on;

static bool on_timer(repeating_timer_t *t)
{
    (void)t;
    lobby_tick();
    return true;
}
#endif

void lobby_begin(uint32_t id)
{
    memset(&lb, 0, sizeof lb);
    lb.id = id;
    lb.rnd = id ^ NOW_MS();
    lb.next_ms = NOW_MS() + rnd() % KNOCK_EVERY;
    lb.hold_ms = NOW_MS();
    usblink_init();
#ifndef LS_HOST
    timer_on = add_repeating_timer_ms(1, on_timer, NULL, &timer);
#endif
}

void lobby_pause(void)
{
#ifndef LS_HOST
    if (timer_on)
        cancel_repeating_timer(&timer);
    timer_on = false;
#endif
    lb.knocking = false;
    usblink_master_abort();
}

void lobby_resume(void)
{
    uint32_t now = NOW_MS();
    lb.knocking = false;
    lb.hold_ms = now;
    lb.next_ms = now + rnd() % KNOCK_EVERY;
    if (lb.seen)
        lb.seen_ms = now;               /* it was busy with us all the time */
#ifndef LS_HOST
    timer_on = add_repeating_timer_ms(1, on_timer, NULL, &timer);
#endif
}

void lobby_end(void)
{
#ifndef LS_HOST
    if (timer_on)
        cancel_repeating_timer(&timer);
    timer_on = false;
#endif
    usblink_master_abort();
    usblink_deinit();
}

/* ---- what the launcher calls ---------------------------------------------------- */

bool lobby_partner(void)
{
    return lb.seen;
}

void lobby_offer(const lobby_offer_t *o)
{
    uint32_t s = LOCK();
    if (lb.in_valid && !lb.in_answer) {
        lb.result = LOBBY_LOST;         /* the partner was quicker */
    } else {
        lb.mine = *o;
        lb.nonce = lb.nonce * 1103515245u + 12345u + rnd();
        lb.offer = OFFER_ASK;
        lb.result = LOBBY_ASKING;
        lb.next_ms = NOW_MS();
    }
    UNLOCK(s);
}

int lobby_offer_state(void)
{
    return lb.result;
}

void lobby_offer_cancel(void)
{
    uint32_t s = LOCK();
    lb.offer = OFFER_NONE;
    UNLOCK(s);
}

void lobby_send(const lobby_file_t *f)
{
    uint32_t s = LOCK();
    lb.send_file = *f;
    lb.send_nonce = lb.send_nonce * 1103515245u + 12345u + rnd();
    lb.send = true;
    lb.send_result = LOBBY_ASKING;
    lb.next_ms = NOW_MS();
    UNLOCK(s);
}

int lobby_send_state(void)
{
    return lb.send_result;
}

void lobby_send_cancel(void)
{
    uint32_t s = LOCK();
    lb.send = false;
    UNLOCK(s);
}

bool lobby_incoming_send(lobby_file_t *f)
{
    uint32_t s = LOCK();
    bool r = lb.sin_valid && !lb.sin_taken;
    if (r && f) {
        *f = lb.sin;
        lb.sin_taken = true;
    }
    UNLOCK(s);
    return r;
}

/* while pbc_xfer has the line: the announcement again (our answer to it got
 * lost), anything else of the lobby gets an answer it ignores */
int lobby_foreign_reply(const void *rxv, int rxlen, void *txv, int txmax)
{
    const packet_t *rx = rxv;
    reply_t *tx = txv;
    if (txmax < (int)sizeof *tx)
        return -1;
    memset(tx, 0, sizeof *tx);
    if (rxlen >= (int)offsetof(packet_t, game) && rx->magic == MAGIC && rx->type == T_SEND) {
        tx->magic = MAGIC;
        tx->nonce = rx->nonce;
        tx->type = R_SEND_GO;
    }
    return (int)sizeof *tx;
}

bool lobby_incoming(lobby_offer_t *o)
{
    uint32_t s = LOCK();
    bool r = lb.in_valid && !lb.in_answer;
    if (r && o)
        *o = lb.in;
    UNLOCK(s);
    return r;
}

void lobby_answer(bool accept)
{
    uint32_t s = LOCK();
    lb.in_answer = accept ? R_ACCEPT : R_NO_GAME;
    UNLOCK(s);
}

int lobby_went(void)
{
    uint32_t s = LOCK();
    int r = lb.in_go ? 1 : lb.in_valid ? 0 : -1;
    UNLOCK(s);
    return r;
}
