/* pbc_lobby.h -- two launchers find each other over the USB-C cable.
 *
 * While the launcher runs, a 1 ms timer looks after the line (usblink): it
 * listens, and about once a second it knocks for 5 ms. Whoever answers is the
 * partner. Whoever starts a game offers it, and the other device starts the
 * same game with the free controller. The offering device becomes the link's
 * master; which controller it plays is a separate choice.
 *
 * test/lobby_test.c runs this on a PC over a link that loses packets.
 */
#ifndef PBC_LOBBY_H
#define PBC_LOBBY_H

#include <stdint.h>
#include <stdbool.h>

#define LOBBY_NAME_MAX 96           /* = PBC_LAUNCH_NAME_MAX */

typedef struct {
    int  device;                    /* launcher's device index */
    int  player;                    /* the controller of the device that joins */
    char game[LOBBY_NAME_MAX];
} lobby_offer_t;

enum { LOBBY_ASKING, LOBBY_ACCEPTED, LOBBY_NO_GAME, LOBBY_GONE,
       LOBBY_LOST };                /* the partner offered too and goes first */

/* `id` tells the devices apart: both offering at once, the lower one wins. */
void lobby_begin(uint32_t id);
void lobby_end(void);
void lobby_tick(void);              /* the timer's work; a test calls it itself */

bool lobby_partner(void);

/* This device starts a game. Then lobby_offer_state() until not ASKING. */
void lobby_offer(const lobby_offer_t *o);
int  lobby_offer_state(void);
void lobby_offer_cancel(void);

/* The partner lacks the game: this device sends it (pbc_xfer.h). Announce
 * it, lobby_send_state() until not ASKING; ACCEPTED: lobby_pause(), then
 * xfer_send(), then lobby_resume(). GONE, LOST (the partner is starting a
 * game itself): not now. */
typedef struct {
    char     name[LOBBY_NAME_MAX];
    uint32_t size, hash;
} lobby_file_t;
void lobby_send(const lobby_file_t *f);
int  lobby_send_state(void);
void lobby_send_cancel(void);

/* The partner sends a file: true once for each announcement (with `f`; NULL
 * only looks). Then lobby_pause(), xfer_recv() with lobby_foreign_reply as
 * its `foreign`, lobby_resume(). */
bool lobby_incoming_send(lobby_file_t *f);
int  lobby_foreign_reply(const void *rx, int rxlen, void *tx, int txmax);

/* The timer stops, the line stays ours (for pbc_xfer); and on again. */
void lobby_pause(void);
void lobby_resume(void);

/* The partner started a game: an offer not answered yet. `o` may be NULL. */
bool lobby_incoming(lobby_offer_t *o);
void lobby_answer(bool accept);
/* After accepting: 0 wait, 1 go, -1 the partner is gone. */
int  lobby_went(void);

#endif
