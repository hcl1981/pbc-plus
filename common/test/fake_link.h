/* fake_link.h -- a usblink that fails like the real one, for the tests here:
 * an exchange can die before the slave has the packet, with the answer on its
 * way, or at the slave's end alone. */
#ifndef FAKE_LINK_H
#define FAKE_LINK_H

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "usblink.h"

static pthread_mutex_t mx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
static int attention, ready;            /* CLK, DATA */
static const void *m_tx; static int m_txlen; static void *m_rx; static int m_rxmax;
static int m_result, m_done, m_busy;
static unsigned rnd_state = 12345;
static long fail_m2s, fail_s2m, fail_slave_end;
static int fail_rate = 10;              /* in 100 */

static int chance(int pct)
{
    rnd_state = rnd_state * 1103515245u + 12345u;
    return (int)((rnd_state >> 16) % 100) < pct;
}

uint32_t ls_host_us(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint32_t)(t.tv_sec * 1000000u + t.tv_nsec / 1000);
}

uint32_t ls_host_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint32_t)(t.tv_sec * 1000u + t.tv_nsec / 1000000);
}

void usblink_init(void) {}
void usblink_deinit(void) {}
bool usblink_master_begin_attention(void)
{
    pthread_mutex_lock(&mx);
    int free_line = !attention;         /* as the real one: CLK already high */
    if (free_line)
        attention = 1;
    pthread_mutex_unlock(&mx);
    return free_line;
}
bool usblink_master_slave_ready(void)
{
    pthread_mutex_lock(&mx);
    /* the slave answers CLK; the master then runs the transfer */
    int r = ready;
    pthread_mutex_unlock(&mx);
    return r;
}
void usblink_master_abort(void)
{
    pthread_mutex_lock(&mx); attention = 0; pthread_mutex_unlock(&mx);
}
int usblink_master_finish_exchange(const void *tx, int txlen, void *rx, int rxmax)
{
    pthread_mutex_lock(&mx);
    m_tx = tx; m_txlen = txlen; m_rx = rx; m_rxmax = rxmax;
    m_done = 0; m_busy = 1;
    pthread_cond_broadcast(&cv);
    struct timespec until;
    clock_gettime(CLOCK_REALTIME, &until);
    until.tv_sec += 1;
    while (!m_done)
        if (pthread_cond_timedwait(&cv, &mx, &until)) {
            m_busy = 0;                 /* the slave had already given up */
            attention = 0;
            pthread_mutex_unlock(&mx);
            return -1;
        }
    int r = m_result;
    pthread_mutex_unlock(&mx);
    return r;
}
bool usblink_slave_request_pending(void)
{
    pthread_mutex_lock(&mx); int a = attention; pthread_mutex_unlock(&mx);
    return a;
}
int usblink_slave_exchange_cb(void *rx, int rxmax, void *tx, int txlen,
                              usblink_reply_fn make_reply, void *user)
{
    pthread_mutex_lock(&mx);
    if (!attention) { pthread_mutex_unlock(&mx); return -4; }
    ready = 1;
    struct timespec until;
    clock_gettime(CLOCK_REALTIME, &until);
    until.tv_nsec += 2000000;
    if (until.tv_nsec >= 1000000000) { until.tv_sec++; until.tv_nsec -= 1000000000; }
    while (!m_busy) {
        if (pthread_cond_timedwait(&cv, &mx, &until)) {
            ready = 0;
            pthread_mutex_unlock(&mx);
            return -1;                  /* the master gave up */
        }
    }
    ready = 0; attention = 0; m_busy = 0;
    int rc;
    if (chance(fail_rate)) {            /* the slave never gets the packet */
        fail_m2s++;
        m_result = -1;
        rc = -2;
    } else {
        memcpy(rx, m_tx, (size_t)(m_txlen < rxmax ? m_txlen : rxmax));
        int n = make_reply(m_txlen, user);
        if (chance(fail_rate)) {        /* the answer is damaged on the way */
            fail_s2m++;
            m_result = -2;
            rc = (int)m_txlen;
        } else if (chance(fail_rate)) { /* the slave's end times out */
            fail_slave_end++;
            memcpy(m_rx, tx, (size_t)(n < m_rxmax ? n : m_rxmax));
            m_result = n;
            rc = -1;
        } else {
            memcpy(m_rx, tx, (size_t)(n < m_rxmax ? n : m_rxmax));
            m_result = n;
            rc = (int)m_txlen;
        }
    }
    m_done = 1;
    pthread_cond_broadcast(&cv);
    pthread_mutex_unlock(&mx);
    (void)txlen;
    return rc;
}

#endif
