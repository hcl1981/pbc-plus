/* zx_video.c -- see zx_video.h */
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/sync.h"
#include "board.h"
#include "color565.h"
#include "display.h"
#include "font8x8.h"
#include "zx_video.h"
#include "zx.h"

static uint16_t pal565[16];

/* ---- ring of finished lines: core 0 -> core 1 --------------------------------- */

#define RING_N 16

static uint8_t __aligned(4) ring[RING_N][ZX_WIDTH];
static int16_t ring_y[RING_N];
static volatile uint32_t ring_head, ring_tail;
static bool lines_this_frame;

static volatile bool core1_running, pause_req, pause_ack;

/* columns: source pixel(s) of each of the 240 shown */
static uint8_t sx_a[PBC_LCD_W], sx_b[PBC_LCD_W];
static bool sx_plain;                   /* plain copy from column 8 on */

/* as msx_video.c: close to a source pixel that one, near the middle
 * between two both, averaged */
static void build_map(uint8_t *a, uint8_t *b, int n, int m)
{
    for (int i = 0; i < n; i++) {
        int f = ((2 * i + 1) * m * 256) / (2 * n) - 128;
        if (f < 0) f = 0;
        int p = f >> 8, frac = f & 255;
        int q = p + 1 < m ? p + 1 : p;
        if (frac < 64)       { a[i] = b[i] = (uint8_t)p; }
        else if (frac > 192) { a[i] = b[i] = (uint8_t)q; }
        else                 { a[i] = (uint8_t)p; b[i] = (uint8_t)q; }
    }
}

void zx_video_init(unsigned mode)
{
    if (mode >= ZX_MODES)
        mode = ZX_MODE_FIT;
    sx_plain = mode == ZX_MODE_CROP;
    if (!sx_plain)
        build_map(sx_a, sx_b, PBC_LCD_W, ZX_WIDTH);
    for (int i = 0; i < 16; i++) {
        uint32_t c = zx_palette(i);
        pal565[i] = PBC_RGB((c >> 16) & 0xff, (c >> 8) & 0xff, c & 0xff);
    }
}

void __not_in_flash_func(zx_video_line)(int y, const uint8_t *px)
{
    if (y >= ZX_SHOWN || !core1_running || pause_req)
        return;
    while (ring_head - ring_tail >= RING_N)
        __wfe();                        /* core 1 frees a slot and signals */
    int slot = (int)(ring_head % RING_N);
    memcpy(ring[slot], px, ZX_WIDTH);
    ring_y[slot] = (int16_t)y;
    __dmb();
    ring_head++;
    __sev();
    lines_this_frame = true;
}

bool zx_video_frame_drawn(void)
{
    bool d = lines_this_frame;
    lines_this_frame = false;
    return d;
}

void zx_video_pause(void)
{
    if (!core1_running)
        return;
    pause_req = true;
    __sev();
    while (!pause_ack)
        tight_loop_contents();
}

void zx_video_resume(void)
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

/* ---- status text ------------------------------------------------------------------ */

#define STATUS_Y (ZX_SHOWN - 14)        /* in the lower border */

static char status_text[32];
static volatile int status_len;

void zx_video_set_status(const char *s)
{
    char t[32];
    strncpy(t, s, sizeof t - 1);
    t[sizeof t - 1] = 0;
    status_len = 0;
    __dmb();
    memcpy(status_text, t, sizeof t);
    __dmb();
    status_len = (int)strlen(t);
}

/* core 1: the text's row `row` (0..7) over a panel line, grey on black */
static void __not_in_flash_func(overlay)(uint16_t *dst, int row)
{
    int n = status_len;
    if (!n)
        return;
    int x0 = (PBC_LCD_W - n * 8) / 2;
    for (int c = 0; c < n; c++) {
        unsigned ch = (unsigned char)status_text[c];
        if (ch < 0x20 || ch > 0x7e) ch = '?';
        uint8_t bits = font8x8_basic[ch - 0x20][row];
        uint16_t *o = dst + x0 + c * 8;
        for (int b = 0; b < 8; b++)
            o[b] = (bits & (1u << b)) ? PBC_GREY : PBC_BLACK;
    }
}

/* ---- core 1 --------------------------------------------------------------------- */

static void __not_in_flash_func(convert)(uint16_t *dst, const uint8_t *px)
{
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

void __not_in_flash_func(zx_video_core1_main)(void)
{
    multicore_lockout_victim_init();    /* flash writes stop core 1 */
    core1_running = true;

    bool open = false;
    int next_y = 0;
    static uint16_t __aligned(4) lbuf[2][PBC_LCD_W];
    int cur = 0;
    const int h = ZX_SHOWN, y0 = (PBC_LCD_H - ZX_SHOWN) / 2;

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
            continue;
        }
        if (ring_tail == ring_head) {
            __wfe();
            continue;
        }
        __dmb();
        int slot = (int)(ring_tail % RING_N);
        int y = ring_y[slot];
        convert(lbuf[cur], ring[slot]);
        if (y >= STATUS_Y && y < STATUS_Y + 8)
            overlay(lbuf[cur], y - STATUS_Y);
        __dmb();
        ring_tail++;
        __sev();
        if (y == 0 || !open || y != next_y) {
            if (open)
                pbc_display_end();
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
