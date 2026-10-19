/* video.c -- palette, scaler and panel output (see video.h). */
#include <string.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/sync.h"
#include "board.h"
#include "color565.h"
#include "display.h"
#include "video.h"
#include "HDMI.h"

#define OUT_W PBC_LCD_W
#define OUT_H_MAX 240                      /* Crop 1:1, PAL */

/* The VDP writes rows with a pitch of the *current* screen width (256 or
 * 320), so this is really a flat buffer; the 2D type only fixes its size. */
uint8_t __aligned(4) video_screen[VIDEO_SRC_H][VIDEO_SRC_W];

uint16_t pbc_palette[256];
const uint8_t *hdmi_line_lut[HDMI_MAX_LINES];
uint8_t hdmi_identity_lut[256];

void graphics_set_palette(uint8_t i, uint32_t color888)
{
    pbc_palette[i] = pbc_rgb24(color888);
}

void hdmi_line_lut_reset(void)
{
    for (int i = 0; i < 256; i++)
        hdmi_identity_lut[i] = (uint8_t)(i & 0x3F);   /* top bits are priority flags */
    for (int i = 0; i < HDMI_MAX_LINES; i++)
        hdmi_line_lut[i] = hdmi_identity_lut;
}

/* ---- frame geometry, written by core 0 only while core 1 is idle --------- */
static int src_w = 0, src_h = 0;          /* 0: geometry not built yet */
static int out_h = 168, out_y = 56;
static int mode;                           /* VIDEO_FIT, VIDEO_CROP */
static int status_y = PBC_LCD_H - 22;
static uint16_t xa[OUT_W], xb[OUT_W];
static uint16_t ya[OUT_H_MAX], yb[OUT_H_MAX];
static volatile bool need_clear = true;

/* ---- handshake ----------------------------------------------------------- */
static volatile uint32_t frame_seq;        /* bumped by core 0 per drawn frame  */
static volatile int      lines_ready;      /* source lines 0..lines_ready-1 done */
static volatile bool     core1_busy;       /* core 1 has a frame open           */

/* ---- core 1 private ------------------------------------------------------ */
static uint32_t active_seq;
static int      frame_out_h, next_row;
static bool     pending;
static int      pbuf;
static uint16_t __aligned(4) rowbuf[2][OUT_W];

static char          status_text[32];
static volatile bool status_dirty;

/* Output index -> (first, second) source index for one axis.
 *   3/4  : groups of 4 -> 3:  a | (b+c)/2 | d
 *   15/16: groups of 16 -> 15: pixels 7 and 8 are merged */
static void build_map(uint16_t *a, uint16_t *b, int n_out, int n_src, bool three_quarters)
{
    for (int i = 0; i < n_out; i++) {
        int s0, s1;
        if (three_quarters) {
            int g = i / 3, j = i % 3;
            s0 = 4 * g + (j == 0 ? 0 : j == 1 ? 1 : 3);
            s1 = 4 * g + (j == 0 ? 0 : j == 1 ? 2 : 3);
        } else {
            int g = i / 15, j = i % 15;
            s0 = 16 * g + (j <= 7 ? j : j + 1);
            s1 = 16 * g + (j < 7 ? j : j + 1);
        }
        if (s0 >= n_src) s0 = n_src - 1;
        if (s1 >= n_src) s1 = n_src - 1;
        a[i] = (uint16_t)s0;
        b[i] = (uint16_t)s1;
    }
}

bool video_idle(void)
{
    /* Core 1 sets core1_busy before it publishes active_seq, so reading in
     * the opposite order can never see "caught up" while a frame is open. */
    uint32_t seen = active_seq;
    __dmb();
    return seen == frame_seq && !core1_busy;
}

void video_set_mode(int m)
{
    mode = m == VIDEO_CROP ? VIDEO_CROP : VIDEO_FIT;
    src_w = 0;                              /* geometry is rebuilt on the next frame */
}

/* 1:1, nothing averaged: both taps on the same source pixel/line. */
static void map_identity(uint16_t *a, uint16_t *b, int n, int first)
{
    for (int i = 0; i < n; i++)
        a[i] = b[i] = (uint16_t)(first + i);
}

static void build_geometry(int w, int h)
{
    bool h40 = (w == 320);
    switch (mode) {
    case VIDEO_CROP:                        /* the middle 240 pixels, unscaled */
        out_h = h;
        map_identity(xa, xb, OUT_W, (w - OUT_W) / 2);
        map_identity(ya, yb, out_h, 0);
        break;
    default:
        out_h = h40 ? h * 3 / 4 : h * 15 / 16;
        build_map(xa, xb, OUT_W, w, h40);
        build_map(ya, yb, out_h, h, h40);
        break;
    }
    out_y = (PBC_LCD_H - out_h) / 2;
    int free = PBC_LCD_H - out_y - out_h;
    status_y = free >= 30 ? PBC_LCD_H - 22 : PBC_LCD_H - free + (free - 8) / 2;
}

void video_frame_begin(int w, int h)
{
    /* The previous frame's remaining rows still read the source buffer. */
    while (!video_idle())
        tight_loop_contents();

    if (w != src_w || h != src_h) {
        src_w = w;
        src_h = h;
        build_geometry(w, h);
        need_clear = true;
    }
    lines_ready = 0;
    __dmb();
    frame_seq = frame_seq + 1;
}

void video_line_done(int line)
{
    __dmb();
    lines_ready = line + 1;
}

void video_set_status(const char *s)
{
    strncpy(status_text, s, sizeof status_text - 1);
    status_text[sizeof status_text - 1] = 0;
    status_dirty = true;
}

static void __not_in_flash_func(convert_row)(int r, uint16_t *out)
{
    const uint8_t *base = &video_screen[0][0];
    int y0 = ya[r], y1 = yb[r];
    const uint8_t *ra = base + y0 * src_w;
    const uint8_t *la = hdmi_line_lut[y0];
    const uint16_t *pal = pbc_palette;

    if (y0 == y1) {
        for (int x = 0; x < OUT_W; x++) {
            uint16_t p = pal[la[ra[xa[x]]]];
            uint16_t q = pal[la[ra[xb[x]]]];
            out[x] = pbc_avg565(p, q);
        }
    } else {
        const uint8_t *rb = base + y1 * src_w;
        const uint8_t *lb = hdmi_line_lut[y1];
        for (int x = 0; x < OUT_W; x++) {
            int i0 = xa[x], i1 = xb[x];
            uint16_t top = pbc_avg565(pal[la[ra[i0]]], pal[la[ra[i1]]]);
            uint16_t bot = pbc_avg565(pal[lb[rb[i0]]], pal[lb[rb[i1]]]);
            out[x] = pbc_avg565(top, bot);
        }
    }
}

void __not_in_flash_func(video_service)(void)
{
    if (!core1_busy) {
        if (active_seq == frame_seq) {
            if (status_dirty) {
                status_dirty = false;
                pbc_display_fill(0, status_y, PBC_LCD_W, 8, PBC_BLACK);
                pbc_display_text_centered(status_y, status_text, PBC_GREY, PBC_BLACK, 1);
            }
            return;
        }
        core1_busy = true;
        __dmb();
        active_seq = frame_seq;
        if (need_clear) {
            need_clear = false;
            pbc_display_fill(0, 0, PBC_LCD_W, PBC_LCD_H, PBC_BLACK);
            status_dirty = status_text[0] != 0;
        }
        frame_out_h = out_h;
        next_row = 0;
        pending = false;
        pbc_display_window(0, out_y, OUT_W, frame_out_h);
        pbc_display_begin();
    }

    for (;;) {
        if (pending) {
            if (pbc_display_busy())
                return;
            pbc_display_send(rowbuf[pbuf], OUT_W);
            pending = false;
            pbuf ^= 1;
            next_row++;
        }
        if (next_row >= frame_out_h) {
            if (pbc_display_busy())
                return;
            pbc_display_end();
            __dmb();
            core1_busy = false;
            return;
        }
        if (yb[next_row] >= lines_ready)
            return;
        convert_row(next_row, rowbuf[pbuf]);
        pending = true;
    }
}
