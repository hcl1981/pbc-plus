/* tms9918.c -- see tms9918.h
 *
 * After Texas Instruments' TMS9918A data manual: registers, VRAM access
 * through the two ports (address latch, read-ahead buffer), the four
 * screen modes and the sprite engine (4 per line with the 5th-sprite flag,
 * collision of any two sprite pixels, early clock, magnification).
 */
#include <string.h>
#include "tms9918.h"

#ifdef PBC_ON_DEVICE
#define TMS_RAM __attribute__((section(".time_critical.tms")))
#else
#define TMS_RAM
#endif

tms9918_t tms;

const uint32_t tms_palette[16] = {
    0x000000, 0x000000, 0x21c842, 0x5edc78, 0x5455ed, 0x7d76fc, 0xd4524d, 0x42ebf5,
    0xfc5554, 0xff7978, 0xd4c154, 0xe6ce80, 0x21b03b, 0xc95bba, 0xcccccc, 0xffffff,
};

void tms_reset(void)
{
    memset(&tms, 0, sizeof tms);
}

uint8_t tms_read_data(void)
{
    uint8_t v = tms.readahead;
    tms.readahead = tms.vram[tms.addr];
    tms.addr = (tms.addr + 1) & 0x3fff;
    tms.latched = 0;
    return v;
}

uint8_t tms_read_status(void)
{
    uint8_t v = tms.status;
    tms.status &= 0x1f;
    tms.latched = 0;
    return v;
}

void tms_write_data(uint8_t v)
{
    tms.vram[tms.addr] = v;
    tms.readahead = v;
    tms.addr = (tms.addr + 1) & 0x3fff;
    tms.latched = 0;
}

void tms_write_ctrl(uint8_t v)
{
    if (!tms.latched) {
        tms.latch = v;
        tms.latched = 1;
        tms.addr = (tms.addr & 0x3f00) | v;     /* the low byte takes effect at once */
        return;
    }
    tms.latched = 0;
    if (v & 0x80) {                             /* register write */
        tms.reg[v & 7] = tms.latch;
    } else {
        tms.addr = (uint16_t)(((v & 0x3f) << 8) | tms.latch);
        if (!(v & 0x40)) {                      /* read: fill the buffer */
            tms.readahead = tms.vram[tms.addr];
            tms.addr = (tms.addr + 1) & 0x3fff;
        }
    }
}

void tms_vblank(void)
{
    tms.status |= 0x80;
}

/* ---- drawing ------------------------------------------------------------- */

static TMS_RAM void sprites(int y, uint8_t *out)
{
    const uint8_t *sat = tms.vram + ((tms.reg[5] & 0x7f) << 7);
    const uint8_t *spg = tms.vram + ((tms.reg[6] & 7) << 11);
    int size = (tms.reg[1] & 2) ? 16 : 8;
    int mag = tms.reg[1] & 1;
    int h = size << mag;
    int found[4], n = 0;

    for (int i = 0; i < 32; i++) {
        int sy = sat[i * 4];
        if (sy == 0xd0)
            break;
        int line = (y - (sy + 1)) & 0xff;
        if (line >= h)
            continue;
        if (n == 4) {                           /* a fifth sprite on this line */
            if (!(tms.status & 0x40))
                tms.status = (uint8_t)((tms.status & 0xa0) | 0x40 | i);
            break;
        }
        found[n++] = i;
    }

    uint8_t cover[TMS_WIDTH + 32];
    memset(cover, 0, sizeof cover);
    /* highest priority (lowest number) last: it ends up on top */
    for (int k = n - 1; k >= 0; k--) {
        const uint8_t *a = sat + found[k] * 4;
        int line = ((y - (a[0] + 1)) & 0xff) >> mag;
        int x = a[1] - ((a[3] & 0x80) ? 32 : 0);
        int color = a[3] & 15;
        int pat = size == 16 ? (a[2] & 0xfc) : a[2];
        const uint8_t *p = spg + pat * 8 + line;
        uint32_t bits = (uint32_t)p[0] << 8;
        if (size == 16)
            bits |= p[16];
        int w = size;
        for (int i = 0; i < w; i++) {
            if (!(bits & (0x8000u >> i)))
                continue;
            for (int m = 0; m <= mag; m++) {
                int px = x + (i << mag) + m;
                if (px < 0 || px >= TMS_WIDTH)
                    continue;
                if (cover[px])
                    tms.status |= 0x20;         /* collision */
                cover[px] = 1;
                if (color)
                    out[px] = (uint8_t)color;
            }
        }
    }
}

TMS_RAM void tms_line(int y, uint8_t *out)
{
    const uint8_t *v = tms.vram;
    int back = tms.reg[7] & 15;
    if (!(tms.reg[1] & 0x40)) {                 /* display off */
        memset(out, back, TMS_WIDTH);
        return;
    }
    int m1 = tms.reg[1] & 0x10, m2 = tms.reg[1] & 0x08, m3 = tms.reg[0] & 0x02;
    const uint8_t *nt = v + ((tms.reg[2] & 0x0f) << 10);

    if (m1) {                                   /* text: 40 x 6 pixels */
        const uint8_t *pg = v + ((tms.reg[4] & 7) << 11);
        int fg = tms.reg[7] >> 4;
        if (!fg) fg = back;
        memset(out, back, TMS_WIDTH);
        const uint8_t *row = nt + (y >> 3) * 40;
        uint8_t *o = out + 8;
        for (int c = 0; c < 40; c++) {
            uint8_t bits = pg[row[c] * 8 + (y & 7)];
            for (int i = 0; i < 6; i++)
                *o++ = (uint8_t)((bits & (0x80 >> i)) ? fg : back);
        }
        return;
    }

    const uint8_t *row = nt + (y >> 3) * 32;
    if (m2) {                                   /* multicolour: 4 x 4 blocks */
        const uint8_t *pg = v + ((tms.reg[4] & 7) << 11);
        int sub = ((y >> 3) & 3) * 2 + ((y >> 2) & 1);
        for (int c = 0; c < 32; c++) {
            uint8_t col = pg[row[c] * 8 + sub];
            int a = col >> 4, b = col & 15;
            if (!a) a = back;
            if (!b) b = back;
            uint8_t *o = out + c * 8;
            o[0] = o[1] = o[2] = o[3] = (uint8_t)a;
            o[4] = o[5] = o[6] = o[7] = (uint8_t)b;
        }
    } else {
        int pbase, cbase, pmask, cmask, section;
        if (m3) {                               /* graphics II: three tables */
            pbase = (tms.reg[4] & 4) << 11;
            pmask = ((tms.reg[4] & 3) << 8) | 0xff;
            cbase = (tms.reg[3] & 0x80) << 6;
            cmask = ((tms.reg[3] & 0x7f) << 3) | 7;
            section = (y >> 6) << 8;
        } else {                                /* graphics I */
            pbase = (tms.reg[4] & 7) << 11;
            pmask = 0xff;
            cbase = tms.reg[3] << 6;
            cmask = 0;
            section = 0;
        }
        int line = y & 7;
        for (int c = 0; c < 32; c++) {
            int t = row[c];
            uint8_t bits, col;
            if (m3) {
                int tile = t + section;
                bits = v[pbase + ((tile & pmask) << 3) + line];
                col = v[cbase + ((tile & cmask) << 3) + line];
            } else {
                bits = v[pbase + (t << 3) + line];
                col = v[cbase + (t >> 3)];
            }
            int fg = col >> 4, bg = col & 15;
            if (!fg) fg = back;
            if (!bg) bg = back;
            uint8_t *o = out + c * 8;
            for (int i = 0; i < 8; i++)
                o[i] = (uint8_t)((bits & (0x80 >> i)) ? fg : bg);
        }
    }
    sprites(y, out);
}
