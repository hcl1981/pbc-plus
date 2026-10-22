/* c64_video.c -- see c64_video.h */
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/sync.h"
#include "pico/multicore.h"
#include "board.h"
#include "color565.h"
#include "display.h"
#include "c64_glue.h"
#include "c64_video.h"
#include "audio_pwm.h"

#define OUT_W  240

/* The window of the 384x272 VIC buffer that is shown, and how big it gets
 * on the panel; set from the display mode before core 1 starts. */
static int src_x = 32, src_y = 16, src_w = 320, src_h = 240;
static int out_h = 180, out_y = (PBC_LCD_H - 180) / 2;

/* C64 "Pepto" palette, as in frank-c64 */
static const uint32_t pepto[16] = {
    0x000000, 0xFFFFFF, 0x68372B, 0x70A4B2, 0x6F3D86, 0x588D43, 0x352879, 0xB8C76F,
    0x6F4F25, 0x433900, 0x9A6759, 0x444444, 0x6C6C6C, 0x9AD284, 0x6C5EB5, 0x959595,
};
static uint16_t pal[256];              /* indexed by the raw byte, masked to 16 colours */

static volatile bool frame_ready;
static volatile bool core1_ready;
static char          status_text[32];
static volatile bool status_dirty;
static uint16_t __aligned(4) rowbuf[2][OUT_W];

/* Output index -> (first, second) source index, groups of 4 -> 3:
 * a | (b+c)/2 | d  (the same mapping megadrive uses for H40). */
static uint16_t xa[OUT_W], xb[OUT_W], ya[180], yb[180];   /* out_h is at most 180 */

static void build_map(uint16_t *a, uint16_t *b, int n)
{
    for (int i = 0; i < n; i++) {
        int g = i / 3, j = i % 3;
        a[i] = (uint16_t)(4 * g + (j == 0 ? 0 : j == 1 ? 1 : 3));
        b[i] = (uint16_t)(4 * g + (j == 0 ? 0 : j == 1 ? 2 : 3));
    }
}

/* Any other ratio (smaller): each output pixel averages the first and the
 * last source pixel it covers. */
static void build_map_any(uint16_t *a, uint16_t *b, int n, int src)
{
    for (int i = 0; i < n; i++) {
        int first = i * src / n, last = ((i + 1) * src - 1) / n;
        if (last >= src) last = src - 1;
        a[i] = (uint16_t)first;
        b[i] = (uint16_t)last;
    }
}

static unsigned mode;

void c64_video_set_mode(unsigned m)
{
    mode = m < C64_VIDEO_MODES ? m : 0;
}

void c64_video_wait_ready(void)
{
    while (!core1_ready)
        tight_loop_contents();
}

bool c64_video_present(void)
{
    if (frame_ready)
        return false;
    __dmb();
    frame_ready = true;
    __sev();
    return true;
}

void c64_video_wait_idle(void)
{
    while (frame_ready || status_dirty)
        tight_loop_contents();
}

void c64_video_set_status(const char *s)
{
    strncpy(status_text, s, sizeof status_text - 1);
    status_text[sizeof status_text - 1] = 0;
    status_dirty = true;
    __sev();
}

static void __not_in_flash_func(convert_row)(int r, uint16_t *out)
{
    const uint8_t *ra = g_pixels + (src_y + ya[r]) * C64_FB_W + src_x;
    const uint8_t *rb = g_pixels + (src_y + yb[r]) * C64_FB_W + src_x;
    for (int x = 0; x < OUT_W; x++) {
        int i0 = xa[x], i1 = xb[x];
        uint16_t top = pbc_avg565(pal[ra[i0]], pal[ra[i1]]);
        uint16_t bot = pbc_avg565(pal[rb[i0]], pal[rb[i1]]);
        out[x] = pbc_avg565(top, bot);
    }
}

void __not_in_flash_func(c64_video_core1_main)(void)
{
    for (int i = 0; i < 256; i++)
        pal[i] = pbc_rgb24(pepto[i & 15]);
    switch (mode) {
    case C64_VIDEO_FULL_BORDER:     /* the whole buffer, 5/8 */
        src_x = 0; src_y = 0; src_w = C64_FB_W; src_h = C64_FB_H; out_h = 170;
        build_map_any(xa, xb, OUT_W, src_w);
        build_map_any(ya, yb, out_h, src_h);
        break;
    case C64_VIDEO_NO_BORDER:       /* the 320x200 screen only, 3/4 */
        src_x = 32; src_y = 35; src_w = 320; src_h = 200; out_h = 150;
        build_map(xa, xb, OUT_W);
        build_map(ya, yb, out_h);
        break;
    default:                        /* standard: 320x240, 3/4 */
        build_map(xa, xb, OUT_W);
        build_map(ya, yb, out_h);
        break;
    }
    out_y = (PBC_LCD_H - out_h) / 2;
    pbc_display_fill(0, 0, PBC_LCD_W, PBC_LCD_H, PBC_BLACK);

    /* Disk writes park this core while a flash sector is rewritten. */
    multicore_lockout_victim_init();
    __dmb();
    core1_ready = true;

    for (;;) {
        if (!frame_ready) {
            if (status_dirty) {
                status_dirty = false;
                pbc_display_fill(0, PBC_LCD_H - 22, PBC_LCD_W, 8, PBC_BLACK);
                pbc_display_text_centered(PBC_LCD_H - 22, status_text, PBC_GREY, PBC_BLACK, 1);
            }
            __wfe();
            continue;
        }
        __dmb();
        pbc_display_window(0, out_y, OUT_W, out_h);
        pbc_display_begin();
        int b = 0;
        for (int y = 0; y < out_h; y++) {
            convert_row(y, rowbuf[b]);  /* the DMA sends the other buffer meanwhile */
            pbc_display_wait();
            pbc_display_send(rowbuf[b], OUT_W);
            b ^= 1;
        }
        pbc_display_end();
        __dmb();
        frame_ready = false;
    }
}
