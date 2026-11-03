/* msx_video.c -- see msx_video.h */
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/sync.h"
#include "board.h"
#include "color565.h"
#include "display.h"
#include "msx_video.h"
#include "tms9918.h"

static uint16_t pal565[16];             /* the TMS9918 colours */

/* ---- ring of finished lines: core 0 -> core 1 ---------------------------------
 *
 * The lines go over as the chip made them (MSX1: 256 colour indices, MSX2:
 * 256 RGB565 pixels); core 1 turns them into the 240 panel pixels while the
 * line before goes out, so core 0 only copies. */

#define RING_N 16
#define SRC_W  256

static uint16_t __aligned(4) ring[RING_N][SRC_W];
static int16_t ring_y[RING_N];
static uint8_t ring_h[RING_N];
static bool ring_565[RING_N];
static volatile uint32_t ring_head, ring_tail;

static const int frame_h = TMS_LINES;
static bool lines_this_frame;

static volatile bool core1_running, pause_req, pause_ack;

/* columns: source pixel(s) of each of the 240 stored ones */
static uint8_t sx_a[PBC_LCD_W], sx_b[PBC_LCD_W];
static bool sx_plain;                   /* plain copy from column 8 on */

/* Output index i of n -> source pixels a/b of m (centre sampling). Close to
 * a source pixel: that one alone; near the middle between two: both, which
 * the caller averages. Sharp, without uneven stair steps. */
static void build_map(uint8_t *a, uint8_t *b, int n, int m)
{
    for (int i = 0; i < n; i++) {
        int f = ((2 * i + 1) * m * 256) / (2 * n) - 128;    /* 8.8 fixed point */
        if (f < 0) f = 0;
        int p = f >> 8, frac = f & 255;
        int q = p + 1 < m ? p + 1 : p;
        if (frac < 64)       { a[i] = b[i] = (uint8_t)p; }
        else if (frac > 192) { a[i] = b[i] = (uint8_t)q; }
        else                 { a[i] = (uint8_t)p; b[i] = (uint8_t)q; }
    }
}

void msx_video_init(unsigned mode)
{
    if (mode >= MSX_MODES)
        mode = MSX_MODE_FIT;
    if (mode == MSX_MODE_CROP) {
        sx_plain = true;
    } else {
        build_map(sx_a, sx_b, PBC_LCD_W, 256);
        sx_plain = false;
    }
    for (int i = 0; i < 16; i++) {
        uint32_t c = tms_palette[i];
        pal565[i] = PBC_RGB((c >> 16) & 0xff, (c >> 8) & 0xff, c & 0xff);
    }
}

static void __not_in_flash_func(ring_put)(int y, int lines, const void *px, bool is565)
{
    if (y >= lines || !core1_running || pause_req)
        return;
    while (ring_head - ring_tail >= RING_N)
        __wfe();                        /* core 1 frees a slot and signals */
    int slot = (int)(ring_head % RING_N);
    memcpy(ring[slot], px, is565 ? SRC_W * 2 : SRC_W);
    ring_565[slot] = is565;
    ring_y[slot] = (int16_t)y;
    ring_h[slot] = (uint8_t)lines;
    __dmb();
    ring_head++;
    __sev();
    lines_this_frame = true;
}

void __not_in_flash_func(msx_video_line)(int y, const uint8_t *px, int backdrop)
{
    (void)backdrop;
    ring_put(y, frame_h, px, false);
}

/* MSX2: the V9938's lines, 256 RGB565 pixels, 192 or 212 a frame */
void __not_in_flash_func(msx_video_line565)(int y, int lines, const uint16_t *px)
{
    ring_put(y, lines, px, true);
}

/* core 1: a ring line to the 240 panel pixels */
static void __not_in_flash_func(convert)(uint16_t *dst, const uint16_t *src, bool is565)
{
    if (is565) {
        if (sx_plain) {
            memcpy(dst, src + 8, PBC_LCD_W * sizeof dst[0]);
            return;
        }
        for (int i = 0; i < PBC_LCD_W; i++) {
            uint16_t a = src[sx_a[i]];
            dst[i] = sx_a[i] == sx_b[i] ? a : pbc_avg565(a, src[sx_b[i]]);
        }
    } else {
        const uint8_t *px = (const uint8_t *)src;
        if (sx_plain) {
            for (int i = 0; i < PBC_LCD_W; i++)
                dst[i] = pal565[px[i + 8]];
            return;
        }
        for (int i = 0; i < PBC_LCD_W; i++) {
            uint16_t a = pal565[px[sx_a[i]]];
            dst[i] = sx_a[i] == sx_b[i] ? a : pbc_avg565(a, pal565[px[sx_b[i]]]);
        }
    }
}

bool msx_video_frame_drawn(void)
{
    bool d = lines_this_frame;
    lines_this_frame = false;
    return d;
}

void msx_video_pause(void)
{
    if (!core1_running)
        return;
    pause_req = true;
    __sev();
    while (!pause_ack)
        tight_loop_contents();
}

void msx_video_resume(void)
{
    if (!core1_running)
        return;
    ring_tail = ring_head;              /* what was queued is stale */
    __dmb();
    pause_req = false;
    __sev();
    while (pause_ack)
        tight_loop_contents();
}

/* ---- status line --------------------------------------------------------------- */

static char          status_text[32];
static volatile bool status_dirty;

void msx_video_set_status(const char *s)
{
    strncpy(status_text, s, sizeof status_text - 1);
    status_text[sizeof status_text - 1] = 0;
    status_dirty = true;
    __sev();
}

/* ---- core 1 --------------------------------------------------------------------- */

#define STATUS_Y (PBC_LCD_H - 12)

static void draw_status(void)
{
    status_dirty = false;
    pbc_display_fill(0, STATUS_Y, PBC_LCD_W, 8, PBC_BLACK);
    if (status_text[0])
        pbc_display_text_centered(STATUS_Y, status_text, PBC_GREY, PBC_BLACK, 1);
}

void __not_in_flash_func(msx_video_core1_main)(void)
{
    multicore_lockout_victim_init();    /* disk writes stop core 1 (flash_fs.c) */
    core1_running = true;

    bool open = false;                  /* a window is being written */
    int  next_y = 0, h = 0, y0 = 0;
    int  shown_h = -1;                  /* height the panel is laid out for */
    static uint16_t __aligned(4) lbuf[2][PBC_LCD_W];   /* one sent, one filled */
    int  cur = 0;

    for (;;) {
        if (pause_req) {
            if (open) {
                pbc_display_end();
                open = false;
            }
            pause_ack = true;
            while (pause_req)
                __wfe();
            pause_ack = false;
            shown_h = -1;               /* a menu was drawn over everything */
            continue;
        }
        if (ring_tail == ring_head) {
            if (!open && status_dirty && shown_h > 0)
                draw_status();
            __wfe();
            continue;
        }
        __dmb();
        int slot = (int)(ring_tail % RING_N);
        int y = ring_y[slot];
        int slot_h = ring_h[slot];
        convert(lbuf[cur], ring[slot], ring_565[slot]);   /* while the last line goes out */
        __dmb();
        ring_tail++;
        __sev();
        if (y == 0 || !open || y != next_y) {
            if (open)
                pbc_display_end();
            if (y == 0 || !open) {
                h = slot_h;
                if (h != shown_h) {
                    pbc_display_fill(0, 0, PBC_LCD_W, PBC_LCD_H, PBC_BLACK);
                    shown_h = h;
                    status_dirty = status_text[0] != 0;
                }
                if (status_dirty)
                    draw_status();
                y0 = (PBC_LCD_H - h) / 2;
            }
            pbc_display_window(0, y0 + y, PBC_LCD_W, h - y);
            pbc_display_begin();
            open = true;
            next_y = y;
        }
        pbc_display_wait();
        pbc_display_send(lbuf[cur], PBC_LCD_W);
        cur ^= 1;
        next_y++;
        if (next_y >= h) {
            pbc_display_end();
            open = false;
        }
    }
}
