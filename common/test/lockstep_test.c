/* lockstep_test.c -- the protocol of common/pbc_lockstep.c on a PC; ./run.sh
 *
 * Two threads stand for the two devices. Each has its own copy of the
 * protocol (run.sh links it twice, as a_ and b_) and they share a fake usblink
 * that fails like the real one: an exchange can die before the slave has the
 * packet, with the answer on its way, or at the slave's end alone. Each
 * "frame" mixes both inputs into a state, as an emulator would; the states
 * have to stay equal. Frames also take random time, so either side waits.
 */
#include <stddef.h>
#include "fake_link.h"

typedef struct {
    void (*begin)(bool);
    int  (*connect)(uint32_t);
    bool (*frame)(uint16_t, const uint8_t *, size_t, uint16_t *);
    bool (*oos)(void);
} proto_t;
#define PROTO(x) \
    void x##ls_begin(bool); int x##ls_connect(uint32_t); \
    bool x##ls_frame(uint16_t, const uint8_t *, size_t, uint16_t *); bool x##ls_out_of_sync(void); \
    static const proto_t x##proto = { x##ls_begin, x##ls_connect, x##ls_frame, x##ls_out_of_sync };
PROTO(a_)
PROTO(b_)

#define FRAMES 300000

/* ---- two devices ---- */
typedef struct {
    const proto_t *p;
    int master;
    uint32_t config;
    int connect;
    uint32_t state;
    long done, flip_at;
    int oos, lost;
    uint8_t ram[2048];
} peer_t;

static long frames;

static uint32_t mix(uint32_t s, uint32_t v) { return (s ^ v) * 16777619u; }

static void *run(void *arg)
{
    peer_t *d = arg;
    unsigned r = d->master ? 7 : 99;
    d->p->begin(d->master);
    int c;
    while ((c = d->p->connect(d->config)) == 0)
        ;
    d->connect = c;
    if (c < 0)
        return NULL;
    for (long f = 0; f < frames; f++) {
        r = r * 1103515245u + 12345u;
        uint16_t me = (uint16_t)(r >> 16), other, p1, p2;
        if (!d->p->frame(me, d->ram, sizeof d->ram, &other)) {
            d->lost = 1;
            return NULL;
        }
        p1 = d->master ? me : other;
        p2 = d->master ? other : me;
        d->state = mix(mix(d->state, p1), p2);
        d->ram[f % sizeof d->ram] = (uint8_t)d->state;
        if (d->flip_at && f == d->flip_at)
            d->ram[(size_t)(f + 300) % sizeof d->ram] ^= 1;
        if (f % 2000 == 0)
            usleep((r >> 20) & 0xFFF);  /* a slow frame on one side or the other */
        d->done = f + 1;
    }
    d->oos = d->p->oos();
    if (!d->master) {                   /* the master's last repeats need an answer */
        uint16_t x;
        d->p->frame(0, NULL, 0, &x);
    }
    return NULL;
}

static void go(peer_t *m, peer_t *s)
{
    pthread_t a, b;
    attention = ready = m_busy = m_done = 0;
    m->master = 1;
    pthread_create(&a, NULL, run, m);
    pthread_create(&b, NULL, run, s);
    pthread_join(a, NULL);
    pthread_join(b, NULL);
}

int main(void)
{
    static peer_t m, s;
    int bad = 0;

    /* 1: a bad link, the state has to stay equal */
    m = (peer_t){ .p = &a_proto, .config = 42 };
    s = (peer_t){ .p = &b_proto, .config = 42 };
    frames = 200000;
    go(&m, &s);
    printf("bad link: %ld/%ld frames, states %08x %08x, failures m2s %ld s2m %ld slave end %ld, oos %d %d\n",
           m.done, s.done, m.state, s.state, fail_m2s, fail_s2m, fail_slave_end, m.oos, s.oos);
    if (m.done != frames || s.done != frames || m.state != s.state || m.oos || s.oos || m.lost || s.lost)
        bad = 1;

    /* 2: one bit of RAM differs: both have to notice */
    m = (peer_t){ .p = &a_proto, .config = 42, .flip_at = 1000 };
    s = (peer_t){ .p = &b_proto, .config = 42 };
    frames = 5000;
    go(&m, &s);
    printf("flipped bit: oos %d %d\n", m.oos, s.oos);
    if (!m.oos || !s.oos)
        bad = 1;

    /* 3: different games: both refuse */
    m = (peer_t){ .p = &a_proto, .config = 1 };
    s = (peer_t){ .p = &b_proto, .config = 2 };
    frames = 10;
    go(&m, &s);
    printf("other game: connect %d %d\n", m.connect, s.connect);
    if (m.connect != -1 || s.connect != -1)
        bad = 1;

    puts(bad ? "FAILED" : "ok");
    return bad;
}
