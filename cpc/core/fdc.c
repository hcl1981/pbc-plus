/* fdc.c -- see fdc.h */
#include <string.h>
#include "fdc.h"

/* ---- the image ---------------------------------------------------------------- */

static const uint8_t *dsk;
static uint32_t dsk_size;
static bool ext;
static int n_tracks, n_sides;
static uint32_t track_off[84][2];       /* 0: no such track */

static uint32_t rd16(const uint8_t *p) { return p[0] | p[1] << 8; }

bool fdc_insert(const uint8_t *d, uint32_t size)
{
    dsk = NULL;
    memset(track_off, 0, sizeof track_off);
    if (size < 256)
        return false;
    if (memcmp(d, "MV - CPC", 8) == 0)
        ext = false;
    else if (memcmp(d, "EXTENDED", 8) == 0)
        ext = true;
    else
        return false;
    n_tracks = d[0x30];
    n_sides = d[0x31];
    if (n_tracks > 84) n_tracks = 84;
    if (n_sides < 1) n_sides = 1;
    if (n_sides > 2) n_sides = 2;
    uint32_t off = 256;
    for (int t = 0; t < n_tracks; t++)
        for (int s = 0; s < n_sides; s++) {
            uint32_t len = ext ? (uint32_t)d[0x34 + t * n_sides + s] << 8 : rd16(d + 0x32);
            if (len && off + 256 <= size && memcmp(d + off, "Track-Info", 10) == 0)
                track_off[t][s] = off;
            off += len;
        }
    dsk = d;
    dsk_size = size;
    fdc_reset();
    return true;
}

/* sector `i` of a track: its ID (C H R N), status, data and length */
typedef struct { uint8_t c, h, r, n, st1, st2; const uint8_t *data; uint32_t len; } sector_t;

static int track_sectors(int t, int s)
{
    if (!dsk || t < 0 || t >= n_tracks || s >= n_sides || !track_off[t][s])
        return 0;
    return dsk[track_off[t][s] + 0x15];
}

static bool get_sector(int t, int s, int i, sector_t *out)
{
    int n = track_sectors(t, s);
    if (i < 0 || i >= n || i >= 29)
        return false;
    const uint8_t *tr = dsk + track_off[t][s];
    uint32_t data = track_off[t][s] + 0x100;
    for (int k = 0; k < i; k++) {
        const uint8_t *si = tr + 0x18 + 8 * k;
        data += ext ? rd16(si + 6) : (uint32_t)(0x80 << (tr[0x14] & 7));
    }
    const uint8_t *si = tr + 0x18 + 8 * i;
    out->c = si[0]; out->h = si[1]; out->r = si[2]; out->n = si[3];
    out->st1 = si[4]; out->st2 = si[5];
    out->len = ext ? rd16(si + 6) : (uint32_t)(0x80 << (tr[0x14] & 7));
    if (data + out->len > dsk_size)
        out->len = data < dsk_size ? dsk_size - data : 0;
    out->data = dsk + data;
    return true;
}

static int find_sector(int t, int s, uint8_t r, sector_t *out)
{
    int n = track_sectors(t, s);
    for (int i = 0; i < n; i++)
        if (get_sector(t, s, i, out) && out->r == r)
            return i;
    return -1;
}

/* ---- the controller ---------------------------------------------------------------- */

enum { P_CMD, P_READ, P_WRITE, P_RESULT };

static struct {
    int phase;
    uint8_t cmd[9];
    int cmd_n, cmd_len;
    uint8_t res[7];
    int res_n, res_i;
    int pcn;                            /* the head's cylinder */
    bool int_pending;
    uint8_t int_st0;
    int id_index;                       /* READ ID: the sector under the head */
    /* the transfer */
    const uint8_t *data;
    uint32_t left;
    uint8_t r, eot, n, st1, st2, head;
    bool fmt;
} f;

void fdc_reset(void)
{
    memset(&f, 0, sizeof f);
}

void fdc_motor(bool on) { (void)on; }

static int cmd_length(uint8_t c)
{
    switch (c & 0x1f) {
    case 0x02: case 0x05: case 0x06: case 0x09: case 0x0c:
    case 0x11: case 0x19: case 0x1d: return 9;
    case 0x03: case 0x0f: return 3;
    case 0x04: case 0x07: case 0x0a: return 2;
    case 0x08: return 1;
    case 0x0d: return 6;
    default: return 1;
    }
}

static void result(int n, uint8_t st0, uint8_t st1, uint8_t st2, uint8_t c, uint8_t h, uint8_t r, uint8_t nn)
{
    uint8_t v[7] = { st0, st1, st2, c, h, r, nn };
    memcpy(f.res, v, 7);
    f.res_n = n;
    f.res_i = 0;
    f.phase = P_RESULT;
}

static void result_rw(uint8_t st0, uint8_t st1, uint8_t st2)
{
    result(7, st0, st1, st2, f.cmd[2], f.cmd[3], f.r, f.cmd[5]);
}

/* the next sector of a read/write, or the end */
static void next_sector(bool first)
{
    uint8_t drv = f.cmd[1] & 3, st0b = (uint8_t)(drv | f.head << 2);
    if (!first) {
        if ((f.st1 | f.st2) & 0x20) {   /* a data error ends it */
            result_rw((uint8_t)(0x40 | st0b), f.st1, f.st2);
            return;
        }
        if (f.r == f.eot) {             /* no TC on the CPC: "end of cylinder" */
            result_rw((uint8_t)(0x40 | st0b), (uint8_t)(0x80 | f.st1), f.st2);
            return;
        }
        f.r++;
    }
    sector_t s;
    if (drv != 0 || find_sector(f.pcn, f.head, f.r, &s) < 0) {
        result_rw((uint8_t)(0x40 | st0b), drv ? 0x00 : 0x04, 0x00);
        if (drv) f.res[0] = (uint8_t)(0x48 | st0b);     /* not ready */
        return;
    }
    f.st1 = s.st1 & 0x25;
    f.st2 = s.st2 & 0x61;
    if ((f.cmd[0] & 0x1f) == 0x05 || (f.cmd[0] & 0x1f) == 0x09) {
        f.phase = P_WRITE;
        f.left = 0x80u << (f.n & 7);
        f.data = NULL;
    } else {
        f.phase = P_READ;
        f.data = s.data;
        f.left = s.len;
        if (!f.left) next_sector(false);
    }
}

static void execute(void)
{
    uint8_t c = f.cmd[0] & 0x1f, drv = f.cmd[1] & 3;
    switch (c) {
    case 0x03:                          /* specify */
        f.phase = P_CMD;
        break;
    case 0x04: {                        /* sense drive status */
        uint8_t st3 = (uint8_t)(f.cmd[1] & 7);
        if (drv == 0 && dsk) st3 |= 0x20 | (f.pcn == 0 ? 0x10 : 0) | (n_sides > 1 ? 0x08 : 0);
        result(1, st3, 0, 0, 0, 0, 0, 0);
        break;
    }
    case 0x07:                          /* recalibrate */
        f.pcn = 0;
        f.int_pending = true;
        f.int_st0 = (uint8_t)(0x20 | drv | (drv ? 0x48 : 0));
        f.phase = P_CMD;
        break;
    case 0x0f:                          /* seek */
        if (drv == 0) f.pcn = f.cmd[2];
        f.int_pending = true;
        f.int_st0 = (uint8_t)(0x20 | drv | (drv ? 0x48 : 0));
        f.phase = P_CMD;
        break;
    case 0x08:                          /* sense interrupt status */
        if (f.int_pending) {
            f.int_pending = false;
            result(2, f.int_st0, (uint8_t)f.pcn, 0, 0, 0, 0, 0);
        } else {
            result(1, 0x80, 0, 0, 0, 0, 0, 0);
        }
        break;
    case 0x0a: {                        /* read ID */
        int head = (f.cmd[1] >> 2) & 1, n = drv ? 0 : track_sectors(f.pcn, head);
        sector_t s;
        if (n == 0 || !get_sector(f.pcn, head, f.id_index % n, &s)) {
            result(7, (uint8_t)(0x40 | drv | head << 2 | (drv ? 0x08 : 0)), 0x01, 0, (uint8_t)f.pcn, 0, 0, 0);
        } else {
            f.id_index++;
            result(7, (uint8_t)(drv | head << 2), 0, 0, s.c, s.h, s.r, s.n);
        }
        break;
    }
    case 0x05: case 0x06: case 0x09: case 0x0c:   /* write / read (deleted) data */
        f.head = (f.cmd[1] >> 2) & 1;
        f.r = f.cmd[4];
        f.n = f.cmd[5];
        f.eot = f.cmd[6];
        f.st1 = f.st2 = 0;
        next_sector(true);
        break;
    case 0x0d:                          /* format: the IDs come in and are dropped */
        f.phase = P_WRITE;
        f.fmt = true;
        f.left = 4u * f.cmd[3];
        if (!f.left) result(7, drv, 0, 0, 0, 0, 0, f.cmd[2]);
        break;
    default:                            /* invalid */
        result(1, 0x80, 0, 0, 0, 0, 0, 0);
        break;
    }
}

uint8_t fdc_status(void)
{
    switch (f.phase) {
    case P_READ:   return 0xf0;
    case P_WRITE:  return 0xb0;
    case P_RESULT: return 0xd0;
    default:       return f.cmd_n ? 0x90 : 0x80;
    }
}

uint8_t fdc_read(void)
{
    if (f.phase == P_READ) {
        uint8_t v = f.left ? *f.data++ : 0xff;
        if (f.left && --f.left == 0)
            next_sector(false);
        return v;
    }
    if (f.phase == P_RESULT) {
        uint8_t v = f.res[f.res_i++];
        if (f.res_i >= f.res_n) {
            f.phase = P_CMD;
            f.cmd_n = 0;
        }
        return v;
    }
    return 0xff;
}

void fdc_write(uint8_t v)
{
    if (f.phase == P_WRITE) {
        if (f.left && --f.left == 0) {
            if (f.fmt) {
                f.fmt = false;
                result(7, f.cmd[1] & 7, 0, 0, 0, 0, 0, f.cmd[2]);
            } else {
                next_sector(false);
            }
        }
        return;
    }
    if (f.phase != P_CMD)
        return;
    if (f.cmd_n == 0)
        f.cmd_len = cmd_length(v);
    f.cmd[f.cmd_n++] = v;
    if (f.cmd_n >= f.cmd_len) {
        f.cmd_n = 0;
        execute();
    }
}

/* ---- the first program ------------------------------------------------------------- */

const char *fdc_first_program(void)
{
    static char name[13];
    sector_t s;
    int track = 0;
    uint8_t first;
    bool system = false;
    if (find_sector(0, 0, 0xc1, &s) >= 0) {
        first = 0xc1;                   /* data format: directory on track 0 */
    } else if (find_sector(0, 0, 0x41, &s) >= 0) {
        first = 0x41;                   /* system (vendor) format: track 2 */
        track = 2;
        system = true;
    } else {
        return NULL;
    }
    for (int k = 0; k < 4; k++) {
        if (find_sector(track, 0, (uint8_t)(first + k), &s) < 0)
            continue;
        for (uint32_t e = 0; e + 32 <= s.len; e += 32) {
            const uint8_t *d = s.data + e;
            if (d[0] != 0 || d[12] != 0)
                continue;               /* deleted, another user, a later extent */
            char ext3[4];
            for (int i = 0; i < 3; i++) ext3[i] = (char)(d[9 + i] & 0x7f);
            ext3[3] = 0;
            if (strcmp(ext3, "BAS") && strcmp(ext3, "BIN") && strcmp(ext3, "   "))
                continue;
            int o = 0;
            for (int i = 0; i < 8 && (d[1 + i] & 0x7f) != ' '; i++)
                name[o++] = (char)(d[1 + i] & 0x7f);
            if (!o)
                continue;
            if (ext3[0] != ' ') {
                name[o++] = '.';
                for (int i = 0; i < 3; i++) name[o++] = ext3[i];
            }
            name[o] = 0;
            return name;
        }
    }
    return system ? "" : NULL;
}
