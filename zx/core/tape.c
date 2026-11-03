/* tape.c -- see tape.h
 *
 * The signal: a pulse starts with an edge (the EAR level flips) and lasts
 * its length in T-states. A block ends with one more edge and its pause
 * (the level drops to low after 1 ms); a pause of 0 joins it to the next
 * block without that edge.
 */
#include <string.h>
#include "tape.h"

bool tape_is_48k;

enum { S_IDLE, S_PULSES, S_DIRECT, S_PAUSE1, S_PAUSE2 };
enum { P_PILOT, P_SYNC1, P_SYNC2, P_DATA, P_SEQ, P_DONE };

static struct {
    const uint8_t *d;
    uint32_t size;
    bool tzx;
    uint32_t pos, next;                 /* this block, the one after it */
    bool playing, at_end;
    int level;
    uint32_t remain;                    /* T-states to the next event */
    int state, phase;
    /* the block being played */
    uint32_t pilot_len, pilot_left, sync1, sync2, zero, one, pause_ms;
    const uint8_t *bytes;
    uint32_t nbytes, byte_i;
    int last_bits, bit_i, half;
    const uint8_t *seq;                 /* 13h: pulse lengths */
    uint32_t seq_n, seq_i;
    uint32_t direct_t;                  /* 15h: T-states per sample */
    uint32_t loop_start;
    int loop_left;
} t;

static uint32_t rd16(const uint8_t *p) { return p[0] | p[1] << 8; }
static uint32_t rd24(const uint8_t *p) { return p[0] | p[1] << 8 | (uint32_t)p[2] << 16; }
static uint32_t rd32(const uint8_t *p) { return rd24(p) | (uint32_t)p[3] << 24; }

#define MS_T 3500u                      /* T-states per millisecond */

/* the offset after the block at `o` (0: broken) */
static uint32_t block_end(uint32_t o)
{
    const uint8_t *b = t.d + o;
    uint32_t left = t.size - o, n;
    if (!t.tzx) {
        if (left < 2) return 0;
        n = 2 + rd16(b);
    } else {
        if (left < 1) return 0;
        const uint8_t *p = b + 1;
        left--;
#define NEED(k) do { if (left < (k)) return 0; } while (0)
        switch (b[0]) {
        case 0x10: NEED(4);    n = 4 + rd16(p + 2); break;
        case 0x11: NEED(0x12); n = 0x12 + rd24(p + 0x0f); break;
        case 0x12: n = 4; break;
        case 0x13: NEED(1);    n = 1 + 2u * p[0]; break;
        case 0x14: NEED(10);   n = 10 + rd24(p + 7); break;
        case 0x15: NEED(8);    n = 8 + rd24(p + 5); break;
        case 0x20: case 0x23: case 0x24: n = 2; break;
        case 0x21: case 0x30: NEED(1); n = 1 + p[0]; break;
        case 0x22: case 0x25: case 0x27: n = 0; break;
        case 0x26: NEED(2);    n = 2 + 2 * rd16(p); break;
        case 0x28: case 0x32: NEED(2); n = 2 + rd16(p); break;
        case 0x2a: n = 4; break;
        case 0x2b: n = 5; break;
        case 0x31: NEED(2);    n = 2 + p[1]; break;
        case 0x33: NEED(1);    n = 1 + 3u * p[0]; break;
        case 0x35: NEED(0x14); n = 0x14 + rd32(p + 0x10); break;
        case 0x5a: n = 9; break;
        default:   NEED(4);    n = 4 + rd32(p); break;     /* 18h, 19h, newer ones */
        }
#undef NEED
        n += 1;
    }
    return n <= t.size - o ? o + n : 0;
}

bool tape_insert(const uint8_t *data, uint32_t size, bool tzx)
{
    memset(&t, 0, sizeof t);
    t.d = data;
    t.size = size;
    t.tzx = tzx;
    if (tzx) {
        if (size < 10 || memcmp(data, "ZXTape!\x1a", 8) != 0)
            return false;
        t.pos = 10;
    }
    return true;
}

void tape_rewind(void)
{
    t.pos = t.tzx ? 10 : 0;
    t.playing = t.at_end = false;
    t.state = S_IDLE;
    t.level = 0;
}

bool tape_present(void) { return t.d != NULL; }
bool tape_playing(void) { return t.playing; }
bool tape_at_end(void) { return t.at_end; }
int tape_level(void) { return t.level; }

static bool info_block(uint8_t id)
{
    switch (id) {
    case 0x21: case 0x22: case 0x30: case 0x31: case 0x32: case 0x33: case 0x35: case 0x5a:
    case 0x2b:
        return true;
    default:
        return false;
    }
}

bool tape_next_standard(const uint8_t **bytes, uint32_t *len)
{
    uint32_t o = t.pos;
    while (o < t.size) {
        uint32_t e = block_end(o);
        if (!e)
            return false;
        if (!t.tzx) {
            *bytes = t.d + o + 2;
            *len = e - o - 2;
            t.pos = e;
            return true;
        }
        uint8_t id = t.d[o];
        if (id == 0x10) {
            *bytes = t.d + o + 5;
            *len = e - o - 5;
            t.pos = e;
            return true;
        }
        if (!info_block(id))
            return false;
        o = e;
    }
    t.pos = o;
    return false;
}

bool tape_peek_standard(void)
{
    uint32_t keep = t.pos;
    const uint8_t *b;
    uint32_t n;
    bool ok = tape_next_standard(&b, &n);
    t.pos = keep;
    return ok;
}

/* ---- real time ---------------------------------------------------------- */

static void stop(void)
{
    t.playing = false;
    t.state = S_IDLE;
}

/* the next pulse of the block, false at its end */
static bool next_pulse(uint32_t *len)
{
    for (;;) {
        switch (t.phase) {
        case P_PILOT:
            if (t.pilot_left) { t.pilot_left--; *len = t.pilot_len; return true; }
            t.phase = P_SYNC1;
            break;
        case P_SYNC1:
            t.phase = P_SYNC2;
            if (t.sync1) { *len = t.sync1; return true; }
            break;
        case P_SYNC2:
            t.phase = P_DATA;
            if (t.sync2) { *len = t.sync2; return true; }
            break;
        case P_DATA: {
            if (t.byte_i >= t.nbytes) { t.phase = P_DONE; break; }
            int bits = t.byte_i == t.nbytes - 1 ? t.last_bits : 8;
            int bit = (t.bytes[t.byte_i] >> (7 - t.bit_i)) & 1;
            *len = bit ? t.one : t.zero;
            if (++t.half == 2) {
                t.half = 0;
                if (++t.bit_i >= bits) { t.bit_i = 0; t.byte_i++; }
            }
            return true;
        }
        case P_SEQ:
            if (t.seq_i < t.seq_n) { *len = rd16(t.seq + 2 * t.seq_i++); return true; }
            t.phase = P_DONE;
            break;
        default:
            return false;
        }
    }
}

static void begin_block(void);

/* the block's pulses are done: its pause, or straight on */
static void end_block(void)
{
    t.pos = t.next;
    if (t.pause_ms == 0) {
        begin_block();
        return;
    }
    t.level ^= 1;                       /* the last pulse ends */
    t.state = S_PAUSE1;
    t.remain = MS_T;
}

static void start_pulses(void)
{
    uint32_t len;
    t.state = S_PULSES;
    if (next_pulse(&len)) {
        t.level ^= 1;
        t.remain = len;
    } else {
        end_block();
    }
}

/* the block at t.pos: set it up and start it (passing over the silent ones) */
static void begin_block(void)
{
    for (int guard = 0; guard < 4096; guard++) {
        if (t.pos >= t.size) {
            t.at_end = true;
            stop();
            return;
        }
        uint32_t e = block_end(t.pos);
        if (!e) {
            t.at_end = true;
            stop();
            return;
        }
        t.next = e;
        const uint8_t *b = t.d + t.pos;
        t.phase = P_PILOT;
        t.pilot_left = 0;
        t.sync1 = t.sync2 = 0;
        t.byte_i = 0;
        t.bit_i = t.half = 0;
        t.last_bits = 8;
        if (!t.tzx || b[0] == 0x10) {           /* standard speed */
            const uint8_t *p = t.tzx ? b + 5 : b + 2;
            t.bytes = p;
            t.nbytes = t.tzx ? e - t.pos - 5 : e - t.pos - 2;
            t.pause_ms = t.tzx ? rd16(b + 1) : 1000;
            t.pilot_len = 2168;
            t.pilot_left = t.nbytes && p[0] < 0x80 ? 8063 : 3223;
            t.sync1 = 667;
            t.sync2 = 735;
            t.zero = 855;
            t.one = 1710;
            start_pulses();
            return;
        }
        const uint8_t *p = b + 1;
        switch (b[0]) {
        case 0x11:                              /* turbo speed */
            t.pilot_len = rd16(p);
            t.sync1 = rd16(p + 2);
            t.sync2 = rd16(p + 4);
            t.zero = rd16(p + 6);
            t.one = rd16(p + 8);
            t.pilot_left = rd16(p + 10);
            t.last_bits = p[12] ? p[12] : 8;
            t.pause_ms = rd16(p + 13);
            t.bytes = p + 0x12;
            t.nbytes = rd24(p + 0x0f);
            start_pulses();
            return;
        case 0x12:                              /* pure tone */
            t.pilot_len = rd16(p);
            t.pilot_left = rd16(p + 2);
            t.phase = P_PILOT;
            t.nbytes = 0;
            t.pause_ms = 0;
            start_pulses();
            return;
        case 0x13:                              /* pulse sequence */
            t.seq = p + 1;
            t.seq_n = p[0];
            t.seq_i = 0;
            t.phase = P_SEQ;
            t.pause_ms = 0;
            start_pulses();
            return;
        case 0x14:                              /* pure data */
            t.zero = rd16(p);
            t.one = rd16(p + 2);
            t.last_bits = p[4] ? p[4] : 8;
            t.pause_ms = rd16(p + 5);
            t.bytes = p + 10;
            t.nbytes = rd24(p + 7);
            t.phase = P_DATA;
            start_pulses();
            return;
        case 0x15:                              /* direct recording */
            t.direct_t = rd16(p) ? rd16(p) : 1;
            t.pause_ms = rd16(p + 2);
            t.last_bits = p[4] ? p[4] : 8;
            t.bytes = p + 8;
            t.nbytes = rd24(p + 5);
            t.state = S_DIRECT;
            if (!t.nbytes) { end_block(); return; }
            t.level = t.bytes[0] >> 7;
            t.bit_i = 1;
            t.remain = t.direct_t;
            return;
        case 0x20:                              /* pause, 0: stop the tape */
            t.pos = e;
            if (rd16(p) == 0) { stop(); return; }
            t.level = 0;
            t.state = S_PAUSE2;
            t.remain = rd16(p) * MS_T;
            return;
        case 0x24:                              /* loop start */
            t.loop_left = (int)rd16(p);
            t.loop_start = e;
            t.pos = e;
            continue;
        case 0x25:                              /* loop end */
            t.pos = (t.loop_left > 1) ? t.loop_start : e;
            if (t.loop_left > 0) t.loop_left--;
            continue;
        case 0x2a:                              /* stop the tape if in 48K mode */
            t.pos = e;
            if (tape_is_48k) { stop(); return; }
            continue;
        case 0x2b:                              /* signal level */
            t.level = p[4] & 1;
            t.pos = e;
            continue;
        default:                                /* information, CSW, generalized data... */
            t.pos = e;
            continue;
        }
    }
    stop();
}

void tape_play(bool on)
{
    if (on == t.playing || !t.d)
        return;
    if (!on) {
        stop();
        return;
    }
    if (t.pos >= t.size) {
        t.at_end = true;
        return;
    }
    t.playing = true;
    t.at_end = false;
    begin_block();
}

/* the event at the end of t.remain */
static void step(void)
{
    switch (t.state) {
    case S_PULSES: {
        uint32_t len;
        if (next_pulse(&len)) {
            t.level ^= 1;
            t.remain = len;
        } else {
            end_block();
        }
        break;
    }
    case S_DIRECT: {
        uint32_t bits_total = (t.nbytes - 1) * 8 + (uint32_t)t.last_bits;
        uint32_t i = t.byte_i * 8 + (uint32_t)t.bit_i;
        if (i >= bits_total) {
            t.pos = t.next;
            if (t.pause_ms) { t.state = S_PAUSE1; t.remain = MS_T; }
            else begin_block();
            break;
        }
        t.level = (t.bytes[i >> 3] >> (7 - (i & 7))) & 1;
        if (++t.bit_i == 8) { t.bit_i = 0; t.byte_i++; }
        t.remain = t.direct_t;
        break;
    }
    case S_PAUSE1:
        t.level = 0;
        t.state = S_PAUSE2;
        t.remain = t.pause_ms > 1 ? (t.pause_ms - 1) * MS_T : 1;
        break;
    case S_PAUSE2:
        begin_block();
        break;
    default:
        stop();
        break;
    }
}

void tape_advance(uint32_t n)
{
    while (t.playing && n) {
        if (t.remain > n) {
            t.remain -= n;
            return;
        }
        n -= t.remain;
        t.remain = 0;
        step();
        if (t.playing && t.remain == 0)
            t.remain = 1;               /* never stand still */
    }
}
