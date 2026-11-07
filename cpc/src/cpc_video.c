/* cpc_video.c -- see cpc_video.h */
#include <string.h>
#include "pbc_scanline.h"
#include "pbc_battwarn.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/sync.h"
#include "board.h"
#include "color565.h"
#include "display.h"
#define FONT8X8_EXTERN
#include "font8x8.h"
#include "cpc_video.h"
#include "cpc.h"

static uint16_t pal565[32];
/* the colours packed for averaging: red in bits 0-10, green 11-21, blue
 * halved in 22-31, so eight weights' worth adds up without carries. The
 * CPC's levels are 0, 128 and 255: halving blue gives the same five bits
 * after averaging as the full value (checked for every four colours). */
static uint32_t pal_w[32];

/* ---- ring of finished lines: core 0 -> core 1 --------------------------------- */

#define RING_N 16

static uint8_t __aligned(8) ring[RING_N][CPC_WIDTH];
static int16_t ring_y[RING_N];
static volatile uint32_t ring_head, ring_tail;
static bool lines_this_frame;

static volatile bool core1_running, pause_req, pause_ack;

static bool crop;                       /* Crop 1:1, else Fit width */

void cpc_video_init(unsigned mode)
{
    if (mode >= CPC_MODES)
        mode = CPC_MODE_FIT;
    crop = mode == CPC_MODE_CROP;
    for (int i = 0; i < 32; i++) {
        uint32_t c = cpc_palette(i), r = (c >> 16) & 0xff, g = (c >> 8) & 0xff, b = c & 0xff;
        pal565[i] = PBC_RGB(r, g, b);
        pal_w[i] = r | g << 11 | (b >> 1) << 22;
    }
}

/* the core draws each line straight into its slot: no copy */
uint8_t *__not_in_flash_func(cpc_video_line_begin)(int y)
{
    if (y >= CPC_SHOWN || !core1_running || pause_req)
        return NULL;
    while (ring_head - ring_tail >= RING_N)
        __wfe();                        /* core 1 frees a slot and signals */
    return ring[ring_head % RING_N];
}

void __not_in_flash_func(cpc_video_line_commit)(int y)
{
    ring_y[ring_head % RING_N] = (int16_t)y;
    __dmb();
    ring_head++;
    __sev();
    lines_this_frame = true;
}

bool cpc_video_frame_drawn(void)
{
    bool d = lines_this_frame;
    lines_this_frame = false;
    return d;
}

void cpc_video_pause(void)
{
    if (!core1_running)
        return;
    pause_req = true;
    __sev();
    while (!pause_ack)
        tight_loop_contents();
}

void cpc_video_resume(void)
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

#define STATUS_Y (CPC_SHOWN - 14)        /* in the lower border */

static char status_text[32];
static volatile int status_len;

void cpc_video_set_status(const char *s)
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

/* 640 -> 240: Fit averages the 8/3 pixels under each panel pixel (three
 * at a time, the middle one in full, the outer ones in part); Crop takes
 * the middle 480 two by two (a mode 1 pixel each) */
static void __not_in_flash_func(convert)(uint16_t *dst, const uint8_t *px)
{
    if (crop) {
        const uint8_t *s = px + 80;
        for (int i = 0; i < PBC_LCD_W; i++, s += 2)
            dst[i] = s[0] == s[1] ? pal565[s[0]] : pbc_avg565(pal565[s[0]], pal565[s[1]]);
        return;
    }
    /* three panel pixels cover eight source pixels: weights 3 3 2 | 1 3 3 1 | 2 3 3,
     * summed in pal_w's fields; >> 6 (green >> 5) is PBC_RGB(sum >> 3) */
    const uint32_t *pw = pal_w;
    for (int g = 0; g < PBC_LCD_W / 3; g++) {
        const uint8_t *s = px + g * 8;
        uint16_t *d = dst + g * 3;
        uint32_t lo, hi;                /* eight equal pixels: no averaging */
        memcpy(&lo, s, 4);
        memcpy(&hi, s + 4, 4);
        if (lo == hi && lo == s[0] * 0x01010101u) {
            d[0] = d[1] = d[2] = pal565[s[0]];
            continue;
        }
        uint32_t w2 = pw[s[2]], w5 = pw[s[5]];
        uint32_t a[3] = {
            3 * (pw[s[0]] + pw[s[1]]) + 2 * w2,
            w2 + 3 * (pw[s[3]] + pw[s[4]]) + w5,
            2 * w5 + 3 * (pw[s[6]] + pw[s[7]]),
        };
        for (int k = 0; k < 3; k++)
            d[k] = (uint16_t)((a[k] >> 27) << 11 | ((a[k] >> 16) & 0x3f) << 5 | ((a[k] >> 6) & 0x1f));
    }
}

void __not_in_flash_func(cpc_video_core1_main)(void)
{
    multicore_lockout_victim_init();    /* flash writes stop core 1 */
    core1_running = true;

    bool open = false;
    int next_y = 0;
    static uint16_t __aligned(4) lbuf[2][PBC_LCD_W];
    int cur = 0;
    const int h = CPC_SHOWN, y0 = (PBC_LCD_H - CPC_SHOWN) / 2;

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
            if (y == 0)
                pbc_battwarn_frame(0, y0, PBC_LCD_W, h);
            pbc_display_window(0, y0 + y, PBC_LCD_W, h - y);
            pbc_display_begin();
            open = true;
            next_y = y;
        }
        static int8_t scan = -1;
        if (scan < 0)
            scan = pbc_scanlines_on();
        if (scan && (y & 1))
            pbc_scanline_dim(lbuf[cur], PBC_LCD_W);
        pbc_battwarn_row(lbuf[cur], y0 + y, 0, PBC_LCD_W);
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
