/* a26_video.c -- see a26_video.h */
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/sync.h"
#include "board.h"
#include "color565.h"
#include "display.h"
#include "a26_video.h"
#include "tia.h"

static uint16_t pal565[128];            /* the TIA colours */

#define WIN_H  240
static int win_y0 = 26;                 /* first line shown: NTSC 26, PAL 42 */

/* ---- ring of finished lines: core 0 -> core 1 ---------------------------------
 *
 * The lines go over as the TIA's colour indices; core 1 turns them into
 * panel pixels. The emulation makes a line faster than the panel takes it,
 * so the ring holds half a frame and core 0 never waits for the panel. */

#define RING_N 128
#define LINE_MAX 320                    /* the 7800's lines */

static uint8_t __aligned(4) ring[RING_N][LINE_MAX];
static int16_t ring_y[RING_N];
static uint8_t ring_h[RING_N];
static bool ring_blank[RING_N];
static bool ring_wide[RING_N];          /* 320 pixels (7800), else 160 */
static volatile uint32_t ring_head, ring_tail;

static const int frame_h = WIN_H;
static bool lines_this_frame;

static volatile bool core1_running, pause_req, pause_ack;

static void make_pal565(void)
{
    for (int i = 0; i < 128; i++) {
        uint32_t c = tia_palette[i];
        pal565[i] = PBC_RGB((c >> 16) & 0xff, (c >> 8) & 0xff, c & 0xff);
    }
}

void a26_video_init(unsigned mode)
{
    (void)mode;                         /* one mode: 160 -> 240 */
    tia_make_palette(false);
    make_pal565();
}

void a26_video_set_pal(bool pal)
{
    tia_make_palette(pal);
    make_pal565();
    win_y0 = pal ? 42 : 26;
}

void __not_in_flash_func(a26_video_line)(int y, const uint8_t *px, bool blank)
{
    y -= win_y0;
    if (y < 0 || y >= frame_h || !core1_running || pause_req)
        return;
    while (ring_head - ring_tail >= RING_N)
        __wfe();                        /* core 1 frees a slot and signals */
    int slot = (int)(ring_head % RING_N);
    ring_blank[slot] = blank;
    ring_wide[slot] = false;
    if (!blank)
        memcpy(ring[slot], px, TIA_WIDTH);
    ring_y[slot] = (int16_t)y;
    ring_h[slot] = (uint8_t)frame_h;
    __dmb();
    ring_head++;
    __sev();
    lines_this_frame = true;
}

/* Core 0: a 7800 line, 320 pixels, y 0..239 */
void __not_in_flash_func(a26_video_line320)(int y, const uint8_t *px)
{
    if (y < 0 || y >= frame_h || !core1_running || pause_req)
        return;
    while (ring_head - ring_tail >= RING_N)
        __wfe();
    int slot = (int)(ring_head % RING_N);
    ring_blank[slot] = false;
    ring_wide[slot] = true;
    memcpy(ring[slot], px, LINE_MAX);
    ring_y[slot] = (int16_t)y;
    ring_h[slot] = (uint8_t)frame_h;
    __dmb();
    ring_head++;
    __sev();
    lines_this_frame = true;
}

bool a26_video_frame_drawn(void)
{
    bool d = lines_this_frame;
    lines_this_frame = false;
    return d;
}

void a26_video_pause(void)
{
    if (!core1_running)
        return;
    pause_req = true;
    __sev();
    while (!pause_ack)
        tight_loop_contents();
}

void a26_video_resume(void)
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

void a26_video_set_status(const char *s)
{
    strncpy(status_text, s, sizeof status_text - 1);
    status_text[sizeof status_text - 1] = 0;
    status_dirty = true;
    __sev();
}

/* ---- core 1 --------------------------------------------------------------------- */

#define STATUS_Y (PBC_LCD_H - 12)

/* two pixels become three: a, avg(a, b), b; a 7800 line's four become
 * three: a, avg(b, c), d */
static void __not_in_flash_func(convert)(uint16_t *dst, const uint8_t *px, bool blank, bool wide)
{
    if (blank) {
        memset(dst, 0, PBC_LCD_W * sizeof dst[0]);
        return;
    }
    if (wide) {
        for (int i = 0; i < LINE_MAX; i += 4, dst += 3) {
            uint16_t b = pal565[px[i + 1]], c = pal565[px[i + 2]];
            dst[0] = pal565[px[i]];
            dst[1] = b == c ? b : pbc_avg565(b, c);
            dst[2] = pal565[px[i + 3]];
        }
        return;
    }
    for (int i = 0; i < TIA_WIDTH; i += 2, dst += 3) {
        uint16_t a = pal565[px[i]], b = pal565[px[i + 1]];
        dst[0] = a;
        dst[1] = a == b ? a : pbc_avg565(a, b);
        dst[2] = b;
    }
}

static void draw_status(void)
{
    status_dirty = false;
    pbc_display_fill(0, STATUS_Y, PBC_LCD_W, 8, PBC_BLACK);
    if (status_text[0])
        pbc_display_text_centered(STATUS_Y, status_text, PBC_GREY, PBC_BLACK, 1);
}

void __not_in_flash_func(a26_video_core1_main)(void)
{
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
        convert(lbuf[cur], ring[slot], ring_blank[slot], ring_wide[slot]);  /* while the last line goes out */
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
