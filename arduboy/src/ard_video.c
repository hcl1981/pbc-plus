/* ard_video.c -- see ard_video.h */
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/sync.h"
#include "board.h"
#include "color565.h"
#include "display.h"
#include "pbc_scanline.h"
#include "pbc_battwarn.h"
#define FONT8X8_EXTERN
#include "font8x8.h"
#include "ard_video.h"
#include "arduboy.h"

/* the frame from core 0: one waiting at most */
static uint8_t __aligned(4) frame[2][1024];
static bool frame_off[2], frame_inv[2];
static volatile int frame_ready = -1;   /* the buffer core 1 is to show, or -1 */
static int frame_fill;                  /* the one core 0 writes next */

static int out_w, out_h, out_x, out_y;
/* columns and rows: the source pixel(s) of each shown one */
static uint8_t sx_a[PBC_LCD_W], sx_b[PBC_LCD_W], sy_a[PBC_LCD_H], sy_b[PBC_LCD_H];

/* the OLED's colours: lit, and dark */
#define LIT  PBC_RGB(236, 244, 255)
#define DARK PBC_BLACK

static void build_map(uint8_t *a, uint8_t *b, int n, int m, bool blend)
{
    for (int i = 0; i < n; i++) {
        if (!blend) {                   /* nearest: 1.5x gives 1, 2, 1, 2 ... */
            a[i] = b[i] = (uint8_t)(i * m / n);
            continue;
        }
        int f = ((2 * i + 1) * m * 256) / (2 * n) - 128;
        if (f < 0) f = 0;
        int p = f >> 8, frac = f & 255;
        int q = p + 1 < m ? p + 1 : p;
        if (frac < 64)       { a[i] = b[i] = (uint8_t)p; }
        else if (frac > 192) { a[i] = b[i] = (uint8_t)q; }
        else                 { a[i] = (uint8_t)p; b[i] = (uint8_t)q; }
    }
}

void ard_video_init(unsigned mode)
{
    bool sharp = mode == ARD_MODE_SHARP;
    out_w = sharp ? 192 : 240;
    out_h = sharp ? 96 : 120;
    out_x = (PBC_LCD_W - out_w) / 2;
    out_y = (PBC_LCD_H - out_h) / 2;
    build_map(sx_a, sx_b, out_w, ARD_W, !sharp);
    build_map(sy_a, sy_b, out_h, ARD_H, !sharp);
}

bool __not_in_flash_func(ard_video_frame)(const uint8_t *screen, bool invert)
{
    if (frame_ready >= 0)
        return false;                   /* core 1 is still busy with the last */
    int f = frame_fill;
    frame_off[f] = !screen;
    frame_inv[f] = invert;
    if (screen) {
        const uint32_t *s = (const uint32_t *)screen;
        uint32_t *d = (uint32_t *)frame[f];
        for (int i = 0; i < 256; i++)
            d[i] = s[i];
    }
    __dmb();
    frame_ready = f;
    frame_fill = f ^ 1;
    __sev();
    return true;
}

/* ---- status ---------------------------------------------------------------- */

static char status_text[32];
static volatile bool status_dirty;

void ard_video_set_status(const char *s)
{
    strncpy(status_text, s, sizeof status_text - 1);
    status_text[sizeof status_text - 1] = 0;
    __dmb();
    status_dirty = true;
    __sev();
}

/* ---- core 1 ------------------------------------------------------------------ */

static inline bool px(const uint8_t *f, int x, int y)
{
    return (f[(y >> 3) * 128 + x] >> (y & 7)) & 1;
}

static void __not_in_flash_func(row_of)(uint16_t *dst, const uint8_t *f, int y, bool inv)
{
    int ya = sy_a[y], yb = sy_b[y];
    uint16_t on = inv ? DARK : LIT, off = inv ? LIT : DARK;
    uint16_t half = pbc_avg565(on, off);
    for (int x = 0; x < out_w; x++) {
        int xa = sx_a[x], xb = sx_b[x];
        int n = px(f, xa, ya) + px(f, xb, ya) + px(f, xa, yb) + px(f, xb, yb);
        dst[x] = n == 4 ? on : n == 0 ? off : n == 2 ? half : n == 3 ? pbc_avg565(on, half)
                                                                     : pbc_avg565(off, half);
    }
}

void __not_in_flash_func(ard_video_core1_main)(void)
{
    multicore_lockout_victim_init();    /* flash writes (saves) stop core 1 */
    static uint16_t __aligned(4) row[2][PBC_LCD_W];
    int cur = 0;
    for (;;) {
        if (status_dirty) {
            status_dirty = false;
            int y = out_y + out_h + 24;
            pbc_display_fill(0, y, PBC_LCD_W, 8, PBC_BLACK);
            pbc_display_text_centered(y, status_text, PBC_GREY, PBC_BLACK, 1);
        }
        int f = frame_ready;
        if (f < 0) {
            __wfe();
            continue;
        }
        __dmb();
        pbc_battwarn_frame(out_x, out_y, out_w, out_h);
        pbc_display_window(out_x, out_y, out_w, out_h);
        pbc_display_begin();
        static int8_t scan = -1;
        if (scan < 0)
            scan = pbc_scanlines_on();
        for (int y = 0; y < out_h; y++) {
            if (frame_off[f])
                for (int x = 0; x < out_w; x++)
                    row[cur][x] = DARK;
            else
                row_of(row[cur], frame[f], y, frame_inv[f]);
            if (scan && (y & 1))
                pbc_scanline_dim(row[cur], out_w);
            pbc_battwarn_row(row[cur], out_y + y, out_x, out_w);
            pbc_display_wait();
            pbc_display_send(row[cur], out_w);
            cur ^= 1;
        }
        pbc_display_end();
        frame_ready = -1;               /* core 0 may hand over the next */
        __sev();
    }
}
