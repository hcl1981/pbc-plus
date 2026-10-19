/* sms_video.c -- see sms_video.h */
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/sync.h"
#include "board.h"
#include "color565.h"
#include "display.h"
#include "font8x8.h"
#include "audio_pwm.h"
#include "sms_video.h"
#include "shared.h"                 /* SMS Plus: vdp, tms_palette_rgb */

#define SMS_SRC_W 256
#define SMS_W   240                 /* every Master System mode stores 240 columns */
#define SMS_HMAX 224
#define SMS_CROP 8
#define GG_W    160
#define GG_H    144
#define GG_X    48                  /* Game Gear window inside the 256-px line */
#define GG_Y    24
#define OUT_HMAX PBC_LCD_H

static uint16_t __aligned(4) fb[2][SMS_W * SMS_HMAX];
static int back;                    /* buffer core 0 draws into */
static bool is_gg;
static unsigned mode;               /* SMS_MODE_* or GG_MODE_* */
static int  lines_drawn;            /* Master System: height of this frame */

static uint16_t pal[32];

/* Master System, core 0: source column(s) of each of the 240 stored ones */
static uint8_t sx_a[SMS_W], sx_b[SMS_W];
static bool sx_plain;               /* plain 1:1 copy from sx_a[0] on */

/* handshake with core 1 */
static volatile const uint16_t *frame;
static volatile int  frame_h;
static volatile bool core1_ready;
static int last_h = -1, last_w = -1; /* core 1: geometry of the last frame */

static char          status_text[32];
static volatile bool status_dirty;

/* core 1: output -> source maps, rebuilt when the geometry changes */
static uint16_t __aligned(4) rowbuf[2][PBC_LCD_W];
static uint8_t gx_a[PBC_LCD_W], gx_b[PBC_LCD_W];
static uint8_t gy_a[OUT_HMAX], gy_b[OUT_HMAX];

/* ---- scaling maps ---------------------------------------------------------- */

/* Output index i of n -> source pixels a/b of m (centre sampling), offset by
 * `first`. Close to a source pixel: that one alone; near the middle between
 * two: both, which the caller averages. Sharp, without uneven stair steps. */
static void build_map(uint8_t *a, uint8_t *b, int n, int m, int first)
{
    for (int i = 0; i < n; i++) {
        int f = ((2 * i + 1) * m * 256) / (2 * n) - 128;    /* 8.8 fixed point */
        if (f < 0) f = 0;
        int p = f >> 8, frac = f & 255;
        int q = p + 1 < m ? p + 1 : p;
        if (frac < 64)       { a[i] = b[i] = (uint8_t)(first + p); }
        else if (frac > 192) { a[i] = b[i] = (uint8_t)(first + q); }
        else                 { a[i] = (uint8_t)(first + p); b[i] = (uint8_t)(first + q); }
    }
}

/* ---- palette callbacks from the core (render.c) -------------------------- */

void sms_palette_sync(int index)
{
    /* CRAM: --BBGGRR, two bits per channel */
    int c = vdp.cram[index];
    pal[index] = PBC_RGB((c & 3) * 85, ((c >> 2) & 3) * 85, ((c >> 4) & 3) * 85);
}

void sms_palette_syncGG(int index)
{
    /* CRAM: GGGGRRRR ----BBBB, four bits per channel */
    int lo = vdp.cram[(index << 1) | 0], hi = vdp.cram[(index << 1) | 1];
    pal[index] = PBC_RGB((lo & 15) * 17, (lo >> 4) * 17, (hi & 15) * 17);
}

void sms_palette_syncSG(int index)
{
    const uint8 *rgb = tms_palette_rgb[index & 15];
    pal[index] = PBC_RGB(rgb[0], rgb[1], rgb[2]);
}

/* ---- line callback from the core (render.c), core 0 ---------------------- */

void sms_render_line(int line, const uint8_t *buffer)
{
    uint16_t *dst;
    if (is_gg) {
        int y = line - GG_Y;
        if (y < 0 || y >= GG_H)
            return;
        dst = fb[back] + y * GG_W;
        if (!buffer) {
            memset(dst, 0, GG_W * sizeof(uint16_t));
            return;
        }
        const uint8_t *src = buffer + GG_X;
        for (int i = 0; i < GG_W; i++)
            dst[i] = pal[src[i] & 31];
    } else {
        if (line < 0 || line >= SMS_HMAX)
            return;
        dst = fb[back] + line * SMS_W;
        if (!buffer) {
            memset(dst, 0, SMS_W * sizeof(uint16_t));
            return;
        }
        if (sx_plain) {
            const uint8_t *src = buffer + sx_a[0];
            for (int i = 0; i < SMS_W; i++)
                dst[i] = pal[src[i] & 31];
        } else {
            /* horizontal scaling happens here, on the 256 indices */
            for (int i = 0; i < SMS_W; i++) {
                uint16_t a = pal[buffer[sx_a[i]] & 31];
                dst[i] = sx_a[i] == sx_b[i] ? a : pbc_avg565(a, pal[buffer[sx_b[i]] & 31]);
            }
        }
        if (line + 1 > lines_drawn)
            lines_drawn = line + 1;
    }
}

/* ---- core 0 -------------------------------------------------------------- */

void sms_video_set_mode(bool gg, unsigned m)
{
    is_gg = gg;
    mode = gg ? (m < GG_MODES ? m : 0) : (m < SMS_MODES ? m : 0);
    if (gg)
        return;
    switch (mode) {
    case SMS_MODE_FIT:                      /* 256 -> 240 */
        build_map(sx_a, sx_b, SMS_W, SMS_SRC_W, 0);
        sx_plain = false;
        break;
    case SMS_MODE_ZOOM:                     /* middle 192 -> 240 */
        build_map(sx_a, sx_b, SMS_W, 192, 32);
        sx_plain = false;
        break;
    default:                                /* 8 columns off each side */
        sx_a[0] = SMS_CROP;
        sx_plain = true;
        break;
    }
}

void sms_video_wait_ready(void)
{
    while (!core1_ready)
        tight_loop_contents();
}

bool sms_video_present(void)
{
    int h = is_gg ? GG_H : (lines_drawn > 192 ? lines_drawn : 192);
    lines_drawn = 0;
    if (frame)
        return false;               /* core 1 still busy: draw over it again */
    frame_h = h;
    __dmb();
    frame = fb[back];
    back ^= 1;                      /* core 1 now reads the other one */
    __sev();
    return true;
}

void sms_video_set_status(const char *s)
{
    strncpy(status_text, s, sizeof status_text - 1);
    status_text[sizeof status_text - 1] = 0;
    status_dirty = true;
    __sev();
}

/* ---- core 1 -------------------------------------------------------------- */

/* Output size for a source frame of height h. */
static void out_size(int h, int *w, int *oh)
{
    if (is_gg) {
        switch (mode) {
        case GG_MODE_ORIGINAL: *w = GG_W; *oh = GG_H; break;
        default:               *w = 240; *oh = 216; break;
        }
        return;
    }
    *w = SMS_W;
    switch (mode) {
    case SMS_MODE_FIT:  *oh = (h * 15 + 8) / 16; break;     /* 192 -> 180 */
    case SMS_MODE_ZOOM: *oh = h * 5 / 4; break;              /* 224 lines -> 280 */
    default:            *oh = h; break;
    }
}

/* Status text over the picture (no room around it -- Zoom 1.25x with 224
 * lines fills all 280 rows): rows 2..13 at the top left, on a dark box. */
#define OV_Y 2
#define OV_H 12
static void __not_in_flash_func(overlay_row)(uint16_t *row, int r, int w)
{
    int len = (int)strlen(status_text);
    int box = len * 8 + 6;
    if (box > w) box = w;
    for (int x = 0; x < box; x++)
        row[x] = 0;
    int gy = r - OV_Y - 2;
    if (gy < 0 || gy >= 8)
        return;
    for (int c = 0; c < len && c * 8 + 11 <= w; c++) {
        unsigned ch = (unsigned char)status_text[c];
        uint8_t bits = (ch >= 0x20 && ch <= 0x7E) ? font8x8_basic[ch - 0x20][gy] : 0;
        for (int b = 0; b < 8; b++)
            if (bits & (1u << b))           /* LSB is the left pixel */
                row[3 + c * 8 + b] = PBC_GREY;
    }
}

/* One output row from two source rows and the column maps. */
static void __not_in_flash_func(scale_row)(const uint16_t *ra, const uint16_t *rb, int w,
                                           bool cols, uint16_t *out)
{
    if (!cols) {                            /* columns are 1:1 already */
        for (int x = 0; x < w; x++)
            out[x] = pbc_avg565(ra[x], rb[x]);
    } else if (ra == rb) {
        for (int x = 0; x < w; x++)
            out[x] = gx_a[x] == gx_b[x] ? ra[gx_a[x]] : pbc_avg565(ra[gx_a[x]], ra[gx_b[x]]);
    } else {
        for (int x = 0; x < w; x++)
            out[x] = pbc_avg565(pbc_avg565(ra[gx_a[x]], ra[gx_b[x]]),
                                pbc_avg565(rb[gx_a[x]], rb[gx_b[x]]));
    }
}

void __not_in_flash_func(sms_video_core1_main)(void)
{
    /* The 22 kHz sample timer runs on this core, as in c64: core 0 stays
     * free of audio interrupts. */
    audio_pwm_set_pool(alarm_pool_create_with_unused_hardware_alarm(4));
    __dmb();
    core1_ready = true;

    int src_w = 0, src_h = 0, out_w = 0, out_h = 0;
    bool over = false;                      /* status goes over the picture */

    for (;;) {
        const uint16_t *src = (const uint16_t *)frame;
        if (!src) {
            if (status_dirty && !over) {
                status_dirty = false;
                pbc_display_fill(0, PBC_LCD_H - 12, PBC_LCD_W, 8, PBC_BLACK);
                pbc_display_text_centered(PBC_LCD_H - 12, status_text, PBC_GREY, PBC_BLACK, 1);
            }
            __wfe();
            continue;
        }
        __dmb();
        int h = frame_h;
        out_size(h, &out_w, &out_h);
        if (out_h != last_h || out_w != last_w || h != src_h) {
            /* geometry changed: maps, borders, where the status goes */
            src_w = is_gg ? GG_W : SMS_W;
            src_h = h;
            if (is_gg)
                build_map(gx_a, gx_b, out_w, GG_W, 0);
            build_map(gy_a, gy_b, out_h, h, 0);
            pbc_display_fill(0, 0, PBC_LCD_W, PBC_LCD_H, PBC_BLACK);
            last_h = out_h;
            last_w = out_w;
            int band = PBC_LCD_H - (PBC_LCD_H - out_h) / 2 - out_h;
            over = band < 12;
            status_dirty = status_text[0] != 0;
        }
        int x0 = (PBC_LCD_W - out_w) / 2, y0 = (PBC_LCD_H - out_h) / 2;
        bool cols = is_gg && out_w != GG_W;     /* SMS columns are done on core 0 */
        bool text = over && status_text[0];

        pbc_display_window(x0, y0, out_w, out_h);
        pbc_display_begin();
        if (!cols && out_h == h && !text) {
            pbc_display_send(src, src_w * h);   /* 1:1, one piece */
        } else {
            int b = 0;
            for (int r = 0; r < out_h; r++) {
                const uint16_t *ra = src + gy_a[r] * src_w, *rb = src + gy_b[r] * src_w;
                const uint16_t *line;
                bool ov = text && r >= OV_Y && r < OV_Y + OV_H;
                if (cols || ra != rb) {
                    scale_row(ra, rb, out_w, cols, rowbuf[b]);
                    line = rowbuf[b];
                } else if (ov) {
                    memcpy(rowbuf[b], ra, out_w * sizeof(uint16_t));
                    line = rowbuf[b];
                } else {
                    line = ra;                  /* send the stored row itself */
                }
                if (ov)
                    overlay_row(rowbuf[b], r, out_w);
                pbc_display_wait();             /* the DMA still sends the other one */
                pbc_display_send(line, out_w);
                b ^= 1;
            }
            pbc_display_wait();
        }
        pbc_display_end();
        __dmb();
        frame = NULL;
    }
}
