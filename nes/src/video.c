/* video.c -- see video.h */
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/sync.h"
#include "board.h"
#include "color565.h"
#include "display.h"
#include "video.h"
#include "quicknes.h"

#define CROP_X 8
#define OUT_Y  ((PBC_LCD_H - VIDEO_H) / 2)

/* Handshake. Core 0 fills one of two palette tables, then publishes the
 * frame; core 1 clears `frame_pixels` when the last row has gone out. */
static uint16_t lut[2][256];
static volatile const uint8_t *frame_pixels;
static volatile long frame_pitch;
static volatile int  frame_lut;
static int next_lut;

static char          status_text[32];
static volatile bool status_dirty;

static uint16_t __aligned(4) rowbuf[2][VIDEO_W];

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
    strncpy(status_text, s, sizeof status_text - 1);
    status_text[sizeof status_text - 1] = 0;
    status_dirty = true;
    __sev();
}

static void __not_in_flash_func(convert_row)(const uint8_t *src, const uint16_t *l, uint16_t *out)
{
    src += CROP_X;
    for (int x = 0; x < VIDEO_W; x += 4) {
        out[x]     = l[src[x]];
        out[x + 1] = l[src[x + 1]];
        out[x + 2] = l[src[x + 2]];
        out[x + 3] = l[src[x + 3]];
    }
}

void __not_in_flash_func(video_core1_main)(void)
{
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
        long pitch = frame_pitch;
        const uint16_t *l = lut[frame_lut];

        pbc_display_window(0, OUT_Y, VIDEO_W, VIDEO_H);
        pbc_display_begin();
        int b = 0;
        for (int y = 0; y < VIDEO_H; y++) {
            /* The DMA reads the other buffer meanwhile. */
            convert_row(px + y * pitch, l, rowbuf[b]);
            pbc_display_wait();
            pbc_display_send(rowbuf[b], VIDEO_W);
            b ^= 1;
        }
        pbc_display_end();
        __dmb();
        frame_pixels = NULL;
    }
}
