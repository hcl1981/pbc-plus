/* lobby_test.c -- common/pbc_lobby.c on a PC; ./run.sh
 *
 * Two threads stand for the two launchers, with their own copy of the
 * protocol (a_, b_), over the failing link of fake_link.h. Each thread also
 * does what the timer does on the device; a device that is "off" stops it.
 */
#include "fake_link.h"
#include "pbc_lobby.h"

#define DECL(x) \
    void x##lobby_begin(uint32_t); void x##lobby_end(void); void x##lobby_tick(void); \
    bool x##lobby_partner(void); void x##lobby_offer(const lobby_offer_t *); \
    int x##lobby_offer_state(void); void x##lobby_offer_cancel(void); \
    bool x##lobby_incoming(lobby_offer_t *); void x##lobby_answer(bool); int x##lobby_went(void);
DECL(a_)
DECL(b_)

typedef struct {
    void (*begin)(uint32_t); void (*tick)(void); bool (*partner)(void);
    void (*offer)(const lobby_offer_t *); int (*state)(void); void (*cancel)(void);
    bool (*incoming)(lobby_offer_t *); void (*answer)(bool); int (*went)(void);
} api_t;
#define API(x) { x##lobby_begin, x##lobby_tick, x##lobby_partner, x##lobby_offer, x##lobby_offer_state, \
                 x##lobby_offer_cancel, x##lobby_incoming, x##lobby_answer, x##lobby_went }
static const api_t api[2] = { API(a_), API(b_) };

enum { DO_NOTHING, DO_OFFER, DO_JOIN, DO_REFUSE };

typedef struct {
    int who;                        /* 0, 1 */
    uint32_t id;
    int plan;                       /* what this device does */
    int found;
    int result;                     /* offer: LOBBY_*; join: lobby_went() */
    lobby_offer_t got;
    volatile int off;
} peer_t;

static peer_t dev[2];
static volatile int stop;

static void *run(void *arg)
{
    peer_t *d = arg;
    const api_t *a = &api[d->who];
    unsigned long t0 = ls_host_ms();
    bool offered = false, answered = false;
    d->result = -1;
    a->begin(d->id);
    while (!stop && ls_host_ms() - t0 < 8000) {
        if (d->off)
            break;
        a->tick();
        usleep(200);
        if (!d->found && a->partner())
            d->found = 1;
        if (d->plan == DO_OFFER && dev[0].found && (dev[1].found || dev[1].plan != DO_OFFER) && !offered) {
            lobby_offer_t o = { .device = 1, .player = 2 };
            snprintf(o.game, sizeof o.game, "game of %d", d->who);
            a->offer(&o);
            offered = true;
        } else if (offered && d->result < 0 && a->state() != LOBBY_ASKING) {
            d->result = a->state();
            if (d->result != LOBBY_LOST)
                break;
        }
        /* a device whose offer lost joins the other's */
        if (d->plan == DO_JOIN || d->result == LOBBY_LOST || d->plan == DO_REFUSE) {
            if (!answered && a->incoming(&d->got)) {
                a->answer(d->plan != DO_REFUSE);
                answered = true;
            }
            if (answered && d->plan != DO_REFUSE && a->went() != 0) {
                d->result = a->went() > 0 ? 100 : 101;
                break;
            }
        }
    }
    /* the partner may need answers for a moment */
    for (unsigned long t = ls_host_ms(); !d->off && !stop && ls_host_ms() - t < 500;) {
        a->tick();
        usleep(200);
    }
    if (offered && d->result >= 0 && d->result != LOBBY_LOST)
        stop = 1;                       /* the scenario is over */
    return NULL;
}

static void go(uint32_t id0, int plan0, uint32_t id1, int plan1, int off_after_ms)
{
    pthread_t t[2];
    attention = ready = m_busy = m_done = 0;
    stop = 0;
    dev[0] = (peer_t){ .who = 0, .id = id0, .plan = plan0 };
    dev[1] = (peer_t){ .who = 1, .id = id1, .plan = plan1 };
    pthread_create(&t[0], NULL, run, &dev[0]);
    pthread_create(&t[1], NULL, run, &dev[1]);
    if (off_after_ms) {
        while (!dev[0].found || !dev[1].found)
            usleep(1000);
        usleep((useconds_t)off_after_ms * 1000);
        dev[1].off = 1;
    }
    pthread_join(t[0], NULL);
    pthread_join(t[1], NULL);
}

static int bad;
static void check(const char *what, int ok)
{
    printf("%-34s %s\n", what, ok ? "ok" : "FAILED");
    if (!ok)
        bad = 1;
}

int main(void)
{
    /* the link's failures are per exchange, three chances of 10 % each */
    fail_rate = 10;

    for (int i = 0; i < 5; i++) {
        go(10, DO_OFFER, 20, DO_JOIN, 0);
        check("offer: both found", dev[0].found && dev[1].found);
        check("offer: starts on both", dev[0].result == LOBBY_ACCEPTED && dev[1].result == 100);
        check("offer: game and player arrive",
              !strcmp(dev[1].got.game, "game of 0") && dev[1].got.player == 2 && dev[1].got.device == 1);
    }

    go(10, DO_OFFER, 20, DO_REFUSE, 0);
    check("game missing: starter told", dev[0].result == LOBBY_NO_GAME);

    /* Both at once. One of them is first if its offer gets through before
     * the other has made one, and then rightly wins whatever its id; so the
     * rule is only asked of most of the runs. One game on both, always. */
    int lower = 0, runs = 0;
    for (int i = 0; i < 10; i++) {
        int swap = i & 1;
        go(swap ? 30 : 10, DO_OFFER, 20, DO_OFFER, 0);
        int w = dev[0].result == LOBBY_ACCEPTED ? 0 : 1;
        char want[16];
        snprintf(want, sizeof want, "game of %d", w);
        check("both offer: one game on both",
              dev[w].result == LOBBY_ACCEPTED && dev[!w].result == 100 && !strcmp(dev[!w].got.game, want));
        runs++;
        if (dev[w].id < dev[!w].id)
            lower++;
    }
    printf("both offer: the lower id won %d of %d\n", lower, runs);
    check("both offer: the lower id goes first", lower >= runs / 2);

    go(10, DO_NOTHING, 20, DO_NOTHING, 200);
    check("partner off: seen at first", dev[0].found);
    /* (device 0 ran on to its 8 s limit; lobby_partner() is its last word) */
    check("partner off: gone in the end", !a_lobby_partner());

    puts(bad ? "FAILED" : "ok");
    return bad;
}
