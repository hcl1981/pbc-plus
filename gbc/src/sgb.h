/* sgb.h -- Super Game Boy colours for the Game Boy emulator (included once,
 * by main.c, after peanut_gb.h; PEANUT_GB_SGB_HOOK points at sgb_joyp). */
#ifndef PBC_SGB_H
#define PBC_SGB_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "color565.h"

/* ---- Super Game Boy colours -------------------------------------------------
 *
 * A game made for the Super Game Boy (header byte 0146h = 03h) talks to it
 * through the joypad register: 16-byte packets, bit by bit (P14 low: 0, P15
 * low: 1, both low: start of a packet), and asks for two joypads to see
 * whether one is there (MLT_REQ, then the joypad ID it reads changes). When
 * the game's palette is "Auto" the emulator answers like a Super Game Boy
 * and takes over its colours: four palettes of four colours, given directly
 * (PAL01 .. PAL12) or picked from 512 the game uploaded (PAL_TRN, PAL_SET),
 * and which palette each 8x8 cell of the screen uses (ATTR_BLK, ATTR_LIN,
 * ATTR_DIV, or one of 45 uploaded maps: ATTR_TRN, ATTR_SET). MASK_EN keeps
 * the picture still while the game uploads. Not done: borders, sound,
 * ATTR_CHR. Uploads read the 4 KB the game shows on the screen for it: the
 * tiles of the first 256 cells of its background map. */
static bool sgb_on;                     /* answer as a Super Game Boy */
static bool sgb_colour;                 /* the game has sent colours */
static uint8_t sgb_mask;                /* MASK_EN: 1 freeze, 2 black, 3 colour 0 */
static uint8_t sgb_players = 1, sgb_id;
static uint8_t sgb_pkt[7 * 16];         /* a command of up to 7 packets */
static int sgb_bits, sgb_packets;
static bool sgb_in, sgb_latched;
static uint8_t sgb_joyp_last = 0x30;
static uint16_t sgb_pal[4][4];          /* 15-bit BGR, as the Super Game Boy has them */
static uint16_t sgb_lut[4][4];          /* ... for the panel */
static uint8_t sgb_attr[18][20];        /* palette of each cell */
static uint16_t sgb_sys[512][4];        /* PAL_TRN */
static uint8_t sgb_files[45][90];       /* ATTR_TRN: 2 bits per cell */

static void sgb_lut_update(void)
{
    for (int p = 0; p < 4; p++)
        for (int c = 0; c < 4; c++) {
            uint16_t v = sgb_pal[c ? p : 0][c];     /* colour 0 is palette 0's for all */
            unsigned r = v & 31, g = (v >> 5) & 31, b = (v >> 10) & 31;
            sgb_lut[p][c] = PBC_RGB(r << 3 | r >> 2, g << 3 | g >> 2, b << 3 | b >> 2);
        }
    sgb_colour = true;
}

/* the 4 KB the game shows for an upload */
static void sgb_vram(struct gb_s *g, uint8_t *out)
{
    uint8_t lcdc = g->hram_io[IO_LCDC];
    unsigned map = (lcdc & LCDC_BG_MAP) ? 0x1C00 : 0x1800;
    for (int i = 0; i < 256; i++) {
        uint8_t t = g->vram[map + (i / 20) * 32 + i % 20];
        unsigned a = (lcdc & LCDC_TILE_SELECT) ? t * 16u : (unsigned)(0x1000 + (int8_t)t * 16);
        memcpy(out + i * 16, &g->vram[a], 16);
    }
}

static void sgb_attr_file(int f)
{
    if (f >= 45)
        return;
    for (int i = 0; i < 360; i++)
        sgb_attr[i / 20][i % 20] = (sgb_files[f][i / 4] >> (6 - 2 * (i % 4))) & 3;
}

static void sgb_pal_pair(int a, int b, const uint8_t *d)
{
    for (int c = 0; c < 4; c++)
        sgb_pal[a][c] = (uint16_t)(d[c * 2] | d[c * 2 + 1] << 8);
    for (int c = 1; c < 4; c++)
        sgb_pal[b][c] = (uint16_t)(d[8 + (c - 1) * 2] | d[9 + (c - 1) * 2] << 8);
    sgb_pal[b][0] = sgb_pal[a][0];
}

static void sgb_command(struct gb_s *g)
{
    const uint8_t *d = sgb_pkt;
    switch (d[0] >> 3) {
    case 0x00: sgb_pal_pair(0, 1, d + 1); sgb_lut_update(); break;     /* PAL01 */
    case 0x01: sgb_pal_pair(2, 3, d + 1); sgb_lut_update(); break;     /* PAL23 */
    case 0x02: sgb_pal_pair(0, 3, d + 1); sgb_lut_update(); break;     /* PAL03 */
    case 0x03: sgb_pal_pair(1, 2, d + 1); sgb_lut_update(); break;     /* PAL12 */
    case 0x04: {                                                       /* ATTR_BLK */
        int n = d[1] > 18 ? 18 : d[1];
        for (int s = 0; s < n; s++) {
            const uint8_t *b = d + 2 + s * 6;
            int ctl = b[0] & 7, pin = b[1] & 3, pbo = (b[1] >> 2) & 3, pout = (b[1] >> 4) & 3;
            int x1 = b[2] & 31, y1 = b[3] & 31, x2 = b[4] & 31, y2 = b[5] & 31;
            if (ctl == 1) pbo = pin;            /* only inside: the border with it */
            if (ctl == 4) pbo = pout;           /* only outside: likewise */
            for (int y = 0; y < 18; y++)
                for (int x = 0; x < 20; x++) {
                    bool in = x > x1 && x < x2 && y > y1 && y < y2;
                    bool on = !in && x >= x1 && x <= x2 && y >= y1 && y <= y2;
                    if (in && (ctl & 1)) sgb_attr[y][x] = (uint8_t)pin;
                    else if (on && (ctl & 3)) sgb_attr[y][x] = (uint8_t)pbo;
                    else if (!in && !on && (ctl & 4)) sgb_attr[y][x] = (uint8_t)pout;
                }
        }
        break;
    }
    case 0x05: {                                                       /* ATTR_LIN */
        int n = d[1] > 110 ? 110 : d[1];
        for (int s = 0; s < n; s++) {
            uint8_t b = d[2 + s];
            int line = b & 31, pal = (b >> 5) & 3;
            if (b & 0x80) {                     /* a row */
                if (line < 18) for (int x = 0; x < 20; x++) sgb_attr[line][x] = (uint8_t)pal;
            } else if (line < 20) {             /* a column */
                for (int y = 0; y < 18; y++) sgb_attr[y][line] = (uint8_t)pal;
            }
        }
        break;
    }
    case 0x06: {                                                       /* ATTR_DIV */
        int on = (d[1] >> 4) & 3, lo = (d[1] >> 2) & 3, hi = d[1] & 3, at = d[2] & 31;
        bool rows = d[1] & 0x40;
        for (int y = 0; y < 18; y++)
            for (int x = 0; x < 20; x++) {
                int v = rows ? y : x;
                sgb_attr[y][x] = (uint8_t)(v < at ? lo : v == at ? on : hi);
            }
        break;
    }
    case 0x0A: {                                                       /* PAL_SET */
        for (int p = 0; p < 4; p++) {
            int i = (d[1 + p * 2] | d[2 + p * 2] << 8) & 511;
            memcpy(sgb_pal[p], sgb_sys[i], sizeof sgb_pal[p]);
        }
        if (d[9] & 0x80)
            sgb_attr_file(d[9] & 0x3f);
        if (d[9] & 0x40)
            sgb_mask = 0;
        sgb_lut_update();
        break;
    }
    case 0x0B: sgb_vram(g, (uint8_t *)sgb_sys); break;                 /* PAL_TRN */
    case 0x11:                                                         /* MLT_REQ */
        sgb_players = (d[1] & 3) == 3 ? 4 : (d[1] & 3) + 1;
        sgb_id = 0;
        break;
    case 0x15: {                                                       /* ATTR_TRN */
        static uint8_t t[4096];
        sgb_vram(g, t);
        memcpy(sgb_files, t, sizeof sgb_files);
        break;
    }
    case 0x16:                                                         /* ATTR_SET */
        sgb_attr_file(d[1] & 0x3f);
        if (d[1] & 0x40)
            sgb_mask = 0;
        break;
    case 0x17: sgb_mask = d[1] & 3; break;                             /* MASK_EN */
    }
}

static int sgb_joyp(struct gb_s *g, uint8_t val)
{
    if (!sgb_on)
        return -1;
    uint8_t v = val & 0x30, last = sgb_joyp_last;
    sgb_joyp_last = v;
    if (v == 0x00) {                    /* start of a packet */
        sgb_in = true;
        sgb_bits = 0;
        sgb_latched = true;
        return -1;
    }
    if (v == 0x30) {
        sgb_latched = false;
        if (sgb_players > 1 && !(last & 0x20))      /* P15 back up: the next joypad */
            sgb_id = (uint8_t)((sgb_id + 1) % sgb_players);
        return sgb_players > 1 ? 0x0F - sgb_id : -1;
    }
    if (sgb_in && !sgb_latched) {       /* one bit: P14 low 0, P15 low 1 */
        sgb_latched = true;
        if (sgb_bits == 128) {          /* the stop bit */
            sgb_in = false;
            sgb_packets++;
            int need = sgb_pkt[0] & 7;
            if (need == 0) need = 1;
            if (sgb_packets >= need) {
                sgb_command(g);
                sgb_packets = 0;
            }
            return -1;
        }
        int byte = sgb_packets * 16 + sgb_bits / 8;
        if (sgb_bits % 8 == 0)
            sgb_pkt[byte] = 0;
        if (v == 0x10)
            sgb_pkt[byte] |= (uint8_t)(1 << (sgb_bits % 8));
        sgb_bits++;
    }
    return -1;
}

#endif
