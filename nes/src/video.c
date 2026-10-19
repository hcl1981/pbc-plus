/* video.c -- see video.h */
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/sync.h"
#include "board.h"
#include "color565.h"
#include "display.h"
#include "video.h"
#include "quicknes.h"

#define SRC_W   256
#define SRC_H   240
#define OUT_W   PBC_LCD_W               /* every mode is 240 wide */
#define STATUS_Y (PBC_LCD_H - 14)       /* status line under the picture */

/* Geometry of the mode in use. */
static int mode = VIDEO_MODE_CROP;
static int out_h = SRC_H, out_y = (PBC_LCD_H - SRC_H) / 2;   /* rows on the panel, first row */
static int src_y0;                      /* first source row (NO_OVERSCAN) */

/* Handshake. Core 0 fills one of two palette tables, then publishes the
 * frame; core 1 clears `frame_pixels` when the last row has gone out. */
static uint16_t lut[2][256];
static volatile const uint8_t *frame_pixels;
static volatile long frame_pitch;
static volatile int  frame_lut;
static int next_lut;

static char          status_text[32];
static volatile bool status_dirty;

static uint16_t __aligned(4) rowbuf[2][OUT_W];
static uint16_t __aligned(4) tmprow[OUT_W];     /* second row of an averaged pair */

void video_set_mode(unsigned m)
{
    mode = m < VIDEO_MODE_COUNT ? (int)m : VIDEO_MODE_CROP;
    switch (mode) {
    case VIDEO_MODE_FIT:         out_h = SRC_H * 15 / 16; src_y0 = 0; break;   /* 225 */
    case VIDEO_MODE_NO_OVERSCAN: out_h = SRC_H - 16;      src_y0 = 8; break;   /* 224 */
    default:                     out_h = SRC_H;           src_y0 = 0; break;   /* 240 */
    }
    out_y = (PBC_LCD_H - out_h) / 2;
}

bool video_busy_with(const uint8_t *pixels)
{
    return frame_pixels == pixels;
}

bool video_present(const uint8_t *pixels, long pitch, const int16_t *palette, int count)
{
    if (frame_pixels)
        return false;

    const qnes_rgb_t *colors = qnes_get_color_table();
    uint16_t *l = lut[next_lut];
    if (count > 256) count = 256;
    for (int i = 0; i < count; i++) {
        const qnes_rgb_t c = colors[palette[i] & 511];
        l[i] = PBC_RGB(c.r, c.g, c.b);
    }
    frame_lut = next_lut;
    next_lut ^= 1;
    frame_pitch = pitch;
    __dmb();
    frame_pixels = pixels;
    __sev();                            /* wake core 1 from __wfe() */
    return true;
}

void video_set_status(const char *s)
{
    strncpy(status_text, s ? s : "", sizeof status_text - 1);
    status_text[sizeof status_text - 1] = 0;
    status_dirty = true;
    __sev();
}

/* ---- row converters --------------------------------------------------------- */

/* 1:1, eight columns cut on each side. */
static void __not_in_flash_func(row_crop)(const uint8_t *src, const uint16_t *l, uint16_t *out)
{
    src += 8;
    for (int x = 0; x < OUT_W; x += 4) {
        out[x]     = l[src[x]];
        out[x + 1] = l[src[x + 1]];
        out[x + 2] = l[src[x + 2]];
        out[x + 3] = l[src[x + 3]];
    }
}

/* 256 -> 240: of every 16 source pixels the middle two share one output
 * pixel, averaged. */
static void __not_in_flash_func(row_fit)(const uint8_t *src, const uint16_t *l, uint16_t *out)
{
    for (int g = 0; g < SRC_W / 16; g++, src += 16, out += 15) {
        for (int k = 0; k < 7; k++)
            out[k] = l[src[k]];
        out[7] = pbc_avg565(l[src[7]], l[src[8]]);
        for (int k = 8; k < 15; k++)
            out[k] = l[src[k + 1]];
    }
}

static void __not_in_flash_func(avg_rows)(uint16_t *a, const uint16_t *b)
{
    for (int x = 0; x < OUT_W; x++)
        a[x] = pbc_avg565(a[x], b[x]);
}

/* Builds output row y of the current mode into `out`. */
static void __not_in_flash_func(make_row)(const uint8_t *px, long pitch, const uint16_t *l,
                                          int y, uint16_t *out)
{
    switch (mode) {
    case VIDEO_MODE_FIT: {
        /* 240 -> 225 rows: same pattern as across, rows 7 and 8 of every 16 */
        int g = y / 15, k = y % 15;
        int sy = g * 16 + (k < 8 ? k : k + 1);
        row_fit(px + sy * pitch, l, out);
        if (k == 7) {
            row_fit(px + (sy + 1) * pitch, l, tmprow);
            avg_rows(out, tmprow);
        }
        break;
    }
    default:
        row_crop(px + (src_y0 + y) * pitch, l, out);
        break;
    }
}

void __not_in_flash_func(video_core1_main)(void)
{
    for (;;) {
        const uint8_t *px = (const uint8_t *)frame_pixels;
        if (!px) {
            if (status_dirty) {
                status_dirty = false;
                pbc_display_fill(0, STATUS_Y, PBC_LCD_W, 8, PBC_BLACK);
                if (status_text[0])
                    pbc_display_text_centered(STATUS_Y, status_text, PBC_GREY, PBC_BLACK, 1);
            }
            __wfe();
            continue;
        }
        __dmb();
        long pitch = frame_pitch;
        const uint16_t *l = lut[frame_lut];

        pbc_display_window(0, out_y, OUT_W, out_h);
        pbc_display_begin();
        int b = 0;
        for (int y = 0; y < out_h; y++) {
            /* The DMA reads the other buffer meanwhile. */
            make_row(px, pitch, l, y, rowbuf[b]);
            pbc_display_wait();
            pbc_display_send(rowbuf[b], OUT_W);
            b ^= 1;
        }
        pbc_display_end();
        __dmb();
        frame_pixels = NULL;
    }
}
