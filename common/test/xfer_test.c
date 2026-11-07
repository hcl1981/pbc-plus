/* xfer_test.c -- common/pbc_xfer.c on a PC; ./run.sh
 *
 * Two threads, sender and receiver, over the failing link of fake_link.h:
 * files of several sizes arrive whole; a cancel on either side, a full
 * drive, a wrong hash and a partner that goes away end both sides properly.
 */
#include "fake_link.h"
#include "pbc_xfer.h"

typedef struct {
    const uint8_t *src; uint8_t *dst; uint32_t size, got;
    int cancel_at;                      /* bytes; -1 never */
    int full_at;                        /* receiver: write fails past this; -1 never */
    int pull_at;                        /* sender: the cable is pulled at this many bytes; -1 never */
    int result;
} side_t;

static int rd(void *u, uint32_t off, void *buf, int len)
{
    side_t *s = u;
    memcpy(buf, s->src + off, (size_t)len);
    return len;
}
static int wr(void *u, const void *buf, int len)
{
    side_t *s = u;
    if (s->full_at >= 0 && s->got + (uint32_t)len > (uint32_t)s->full_at)
        return -1;
    memcpy(s->dst + s->got, buf, (size_t)len);
    s->got += (uint32_t)len;
    usleep(3000);                       /* writing the flash takes a while */
    return len;
}
static bool tk(void *u, uint32_t done)
{
    side_t *s = u;
    if (s->pull_at >= 0 && done >= (uint32_t)s->pull_at)
        fail_rate = 100;                /* nothing gets through from now on */
    return s->cancel_at < 0 || done < (uint32_t)s->cancel_at;
}

static xfer_t xs, xr;
static side_t ss, sr;
static void *t_send(void *a) { (void)a; ss.result = xfer_send(&xs); return NULL; }
static void *t_recv(void *a) { (void)a; sr.result = xfer_recv(&xr); return NULL; }

static int bad;
static void check(const char *what, int ok)
{
    printf("%-44s %s\n", what, ok ? "ok" : "FAILED");
    if (!ok) bad = 1;
}

static void run(uint32_t size, int send_cancel, int recv_cancel, int full, uint32_t hash_xor, int vanish)
{
    static uint8_t src[300000], dst[300000];
    for (uint32_t i = 0; i < size; i++) src[i] = (uint8_t)(i * 7 + (i >> 8));
    memset(dst, 0, sizeof dst);
    attention = ready = m_busy = m_done = 0;
    uint32_t h = xfer_hash(XFER_HASH_START, src, size);
    ss = (side_t){ .src = src, .size = size, .cancel_at = send_cancel, .full_at = -1,
                   .pull_at = vanish ? (int)size / 3 : -1 };
    sr = (side_t){ .dst = dst, .size = size, .cancel_at = recv_cancel, .full_at = full, .pull_at = -1 };
    xs = (xfer_t){ .size = size, .hash = h, .read = rd, .tick = tk, .user = &ss };
    xr = (xfer_t){ .size = size, .hash = h ^ hash_xor, .write = wr, .tick = tk, .user = &sr };
    pthread_t a, b;
    pthread_create(&b, NULL, t_recv, NULL);
    pthread_create(&a, NULL, t_send, NULL);
    pthread_join(a, NULL);
    pthread_join(b, NULL);
    fail_rate = 10;
}

int main(void)
{
    fail_rate = 10;
    uint32_t sizes[] = { 0, 1, 127, 128, 129, 16384, 16385, 50000, 250000 };
    for (unsigned i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        char what[64];
        run(sizes[i], -1, -1, -1, 0, 0);
        snprintf(what, sizeof what, "%u bytes: both OK", sizes[i]);
        check(what, ss.result == XFER_OK && sr.result == XFER_OK);
        snprintf(what, sizeof what, "%u bytes: arrived whole", sizes[i]);
        check(what, sr.got == sizes[i] && !memcmp(ss.src, sr.dst, sizes[i]));
    }
    run(100000, 30000, -1, -1, 0, 0);
    check("sender cancels: both stop", ss.result == XFER_CANCEL && sr.result == XFER_CANCEL);
    run(100000, -1, 30000, -1, 0, 0);
    check("receiver cancels: both stop", ss.result == XFER_CANCEL && sr.result == XFER_CANCEL);
    run(100000, -1, -1, 40000, 0, 0);
    check("drive full: both know", ss.result == XFER_FULL && sr.result == XFER_FULL);
    run(20000, -1, -1, -1, 1, 0);
    check("wrong hash: both know", ss.result == XFER_BAD && sr.result == XFER_BAD);
    run(200000, -1, -1, -1, 0, 1);
    check("cable pulled: both give up", ss.result == XFER_GONE && sr.result == XFER_GONE);
    printf("link failures: %ld before, %ld after, %ld at the slave\n", fail_m2s, fail_s2m, fail_slave_end);
    puts(bad ? "FAILED" : "ok");
    return bad;
}
