/* video.c -- see video.h */
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/sync.h"
#include "board.h"
#include "color565.h"
#include "display.h"
#include "audio_pwm.h"
#include "video.h"
#include "pce-go.h"

#define OUT_W     PBC_LCD_W
#define MAX_H     XBUF_HEIGHT
#define FB_BYTES  (16 + XBUF_WIDTH * XBUF_HEIGHT + 16)

static uint8_t __aligned(4) fb[2][FB_BYTES];
static int back;                        /* buffer pce-go renders into */

static uint16_t lut[256];               /* GGGRRRBB -> panel word */

/* Handshake for one frame. */
static volatile const uint8_t *frame_pixels;
static volatile int frame_w, frame_h;
static volatile bool core1_ready;

static char          status_text[32];
static volatile bool status_dirty;

static unsigned mode;                   /* display mode, set before core 1 runs */

/* Horizontal map for the current width (core 1 only). */
static int map_w = -1;
static int out_w, out_x;
static bool scaled;                     /* wider than 256: scaled in both directions */
static uint16_t xa[OUT_W], xb[OUT_W];
static uint16_t __aligned(4) rowbuf[2][OUT_W];

static void build_map(int w)
{
    map_w = w;
    bool scale, crop;
    if (mode == 1) {                    /* Crop 1:1: never scale */
        scale = false;
        crop = w > OUT_W;
    } else {                            /* crop up to 256, scale what is wider */
        scale = w > 256;
        crop = !scale && w > OUT_W;
    }
    scaled = scale;
    if (crop) {                         /* the middle 240 columns, 1:1 */
        int c = (w - OUT_W) / 2;
        out_w = OUT_W;
        out_x = 0;
        for (int x = 0; x < OUT_W; x++)
            xa[x] = xb[x] = (uint16_t)(x + c);
    } else if (scale) {                 /* scale to 240, average in between */
        out_w = OUT_W;
        out_x = 0;
        for (int x = 0; x < OUT_W; x++) {
            uint32_t p = (uint32_t)x * (uint32_t)w * 256u / OUT_W;
            int i0 = (int)(p >> 8), frac = (int)(p & 255);
            int i1 = i0 + 1 < w ? i0 + 1 : i0;
            if (frac < 64)       xa[x] = xb[x] = (uint16_t)i0;
            else if (frac > 192) xa[x] = xb[x] = (uint16_t)i1;
            else               { xa[x] = (uint16_t)i0; xb[x] = (uint16_t)i1; }
        }
    } else {                            /* narrow: centred, 1:1 */
        out_w = w;
        out_x = (OUT_W - w) / 2;
        for (int x = 0; x < w; x++)
            xa[x] = xb[x] = (uint16_t)x;
    }
}

void video_set_mode(unsigned m)
{
    mode = m < VIDEO_MODES ? m : 0;
}

uint8_t *video_back_buffer(void)
{
    return fb[back] + 16;
}

void video_wait_ready(void)
{
    while (!core1_ready)
        tight_loop_contents();
}

bool video_present(int w, int h)
{
    if (frame_pixels)
        return false;
    if (w > XBUF_WIDTH - 16) w = XBUF_WIDTH - 16;
    if (h > MAX_H) h = MAX_H;
    if (w < 8 || h < 8)
        return false;
    frame_w = w;
    frame_h = h;
    __dmb();
    frame_pixels = fb[back] + 16;
    back ^= 1;
    __sev();
    return true;
}

void video_set_status(const char *s)
{
    strncpy(status_text, s, sizeof status_text - 1);
    status_text[sizeof status_text - 1] = 0;
    status_dirty = true;
    __sev();
}

static void __not_in_flash_func(convert_row)(const uint8_t *src, uint16_t *out)
{
    for (int x = 0; x < out_w; x++)
        out[x] = pbc_avg565(lut[src[xa[x]]], lut[src[xb[x]]]);
}

/* An output line that falls between two source lines: both averaged. */
static void __not_in_flash_func(convert_row2)(const uint8_t *s0, const uint8_t *s1, uint16_t *out)
{
    for (int x = 0; x < out_w; x++)
        out[x] = pbc_avg565(pbc_avg565(lut[s0[xa[x]]], lut[s0[xb[x]]]),
                            pbc_avg565(lut[s1[xa[x]]], lut[s1[xb[x]]]));
}

void __not_in_flash_func(video_core1_main)(void)
{
    for (int i = 0; i < 256; i++) {
        int g3 = (i >> 5) & 7, r3 = (i >> 2) & 7, b2 = i & 3;
        lut[i] = PBC_RGB(r3 * 255 / 7, g3 * 255 / 7, b2 * 255 / 3);
    }

    __dmb();
    core1_ready = true;

    int last_x = -1, last_w = -1, last_h = -1;
    for (;;) {
        const uint8_t *px = (const uint8_t *)frame_pixels;
        if (!px) {
            if (status_dirty) {
                status_dirty = false;
                pbc_display_fill(0, PBC_LCD_H - 14, PBC_LCD_W, 8, PBC_BLACK);
                pbc_display_text_centered(PBC_LCD_H - 14, status_text, PBC_GREY, PBC_BLACK, 1);
            }
            __wfe();
            continue;
        }
        __dmb();
        int w = frame_w, h = frame_h;
        if (w != map_w)
            build_map(w);
        /* scaled lines keep the aspect ratio: the height by the same factor */
        int out_h = scaled ? h * OUT_W / w : h;
        int y0 = (PBC_LCD_H - out_h) / 2;

        /* Geometry changed: clear what the new picture will not cover. */
        if (out_x != last_x || out_w != last_w || out_h != last_h) {
            pbc_display_fill(0, 0, PBC_LCD_W, PBC_LCD_H, PBC_BLACK);
            status_dirty = status_text[0] != 0;
            last_x = out_x; last_w = out_w; last_h = out_h;
        }

        pbc_display_window(out_x, y0, out_w, out_h);
        pbc_display_begin();
        int b = 0;
        for (int y = 0; y < out_h; y++) {
            /* DMA sends the other buffer meanwhile */
            if (out_h == h) {
                convert_row(px + y * XBUF_WIDTH, rowbuf[b]);
            } else {
                uint32_t p = (uint32_t)y * (uint32_t)h * 256u / (uint32_t)out_h;
                int i0 = (int)(p >> 8), frac = (int)(p & 255);
                int i1 = i0 + 1 < h ? i0 + 1 : i0;
                if (frac < 64)
                    convert_row(px + i0 * XBUF_WIDTH, rowbuf[b]);
                else if (frac > 192)
                    convert_row(px + i1 * XBUF_WIDTH, rowbuf[b]);
                else
                    convert_row2(px + i0 * XBUF_WIDTH, px + i1 * XBUF_WIDTH, rowbuf[b]);
            }
            pbc_display_wait();
            pbc_display_send(rowbuf[b], out_w);
            b ^= 1;
        }
        pbc_display_end();
        __dmb();
        frame_pixels = NULL;
    }
}
