/* cpc_video.c -- see cpc_video.h */
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/sync.h"
#include "board.h"
#include "color565.h"
#include "display.h"
#include "font8x8.h"
#include "cpc_video.h"
#include "cpc.h"

static uint16_t pal565[32];
/* the colours split up, for averaging three of them */
static uint8_t pal_r[32], pal_g[32], pal_b[32];

/* ---- ring of finished lines: core 0 -> core 1 --------------------------------- */

#define RING_N 16

static uint8_t __aligned(4) ring[RING_N][CPC_WIDTH];
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
        uint32_t c = cpc_palette(i);
        pal_r[i] = (uint8_t)(c >> 16);
        pal_g[i] = (uint8_t)(c >> 8);
        pal_b[i] = (uint8_t)c;
        pal565[i] = PBC_RGB(pal_r[i], pal_g[i], pal_b[i]);
    }
}

void __not_in_flash_func(cpc_video_line)(int y, const uint8_t *px)
{
    if (y >= CPC_SHOWN || !core1_running || pause_req)
        return;
    while (ring_head - ring_tail >= RING_N)
        __wfe();                        /* core 1 frees a slot and signals */
    int slot = (int)(ring_head % RING_N);
    memcpy(ring[slot], px, CPC_WIDTH);
    ring_y[slot] = (int16_t)y;
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
    /* three panel pixels cover eight source pixels: weights 3 3 2 | 1 3 3 1 | 2 3 3 */
    static const uint8_t w[3][4] = { { 3, 3, 2, 0 }, { 1, 3, 3, 1 }, { 2, 3, 3, 0 } };
    static const uint8_t first[3] = { 0, 2, 5 };
    for (int g = 0; g < PBC_LCD_W / 3; g++) {
        const uint8_t *s = px + g * 8;
        uint32_t lo, hi;                /* eight equal pixels: no averaging */
        memcpy(&lo, s, 4);
        memcpy(&hi, s + 4, 4);
        if (lo == hi && lo == s[0] * 0x01010101u) {
            dst[g * 3] = dst[g * 3 + 1] = dst[g * 3 + 2] = pal565[s[0]];
            continue;
        }
        for (int k = 0; k < 3; k++) {
            int r = 0, gr = 0, b = 0;
            for (int j = 0; j < 4; j++) {
                int c = s[first[k] + j < 8 ? first[k] + j : 7], wt = w[k][j];
                r += pal_r[c] * wt; gr += pal_g[c] * wt; b += pal_b[c] * wt;
            }
            dst[g * 3 + k] = PBC_RGB(r >> 3, gr >> 3, b >> 3);
        }
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
