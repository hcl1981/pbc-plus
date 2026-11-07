/* sms_video.c -- see sms_video.h */
#include <string.h>
#include "pbc_scanline.h"
#include "pbc_battwarn.h"
#include "pico/stdlib.h"
#include "hardware/sync.h"
#include "pico/multicore.h"
#include "board.h"
#include "color565.h"
#include "display.h"
#define FONT8X8_EXTERN
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

/* PBC: the frames as the core's 8-bit palette indices, one 256-byte row
 * per line (Game Gear: its 160 pixels), and the palette each line was drawn
 * with. Core 1 converts them to panel colours while it sends; core 0 used
 * to do that for every pixel (about 5 % of its time), and the two buffers
 * of panel words took 70 KB more. A palette is stored again only when it
 * changed since the last line: pals[] holds at most one per line. */
#define IX_W 256
static uint8_t  __aligned(4) ix[2][IX_W * SMS_HMAX];
static uint8_t  line_pal[2][SMS_HMAX];    /* index into pals[], or LINE_BLACK */
#define LINE_BLACK 0xFF
static uint16_t __aligned(4) pals[2][SMS_HMAX][32];
static int      npal;               /* palettes stored in pals[back] */
static bool     pal_changed;        /* pal[] differs from the last stored one */
static int back;                    /* buffer core 0 draws into */
static bool is_gg;
static unsigned mode;               /* SMS_MODE_* or GG_MODE_* */
static int  lines_drawn;            /* Master System: height of this frame */

static uint16_t pal[32];

/* Master System, core 0: source column(s) of each of the 240 stored ones */
static uint8_t sx_a[SMS_W], sx_b[SMS_W];
static bool sx_plain;               /* plain 1:1 copy from sx_a[0] on */

/* handshake with core 1: the buffer index + 1, 0 = none */
static volatile int  frame;
static volatile int  frame_h;
static volatile bool core1_ready;
static int last_h = -1, last_w = -1; /* core 1: geometry of the last frame */

static char          status_text[32];
static volatile bool status_dirty;

/* core 1: output -> source maps, rebuilt when the geometry changes */
static uint16_t __aligned(4) rowbuf[2][PBC_LCD_W];
/* core 1: the last two source rows converted to panel colours */
static uint16_t __aligned(4) srcrow[2][SMS_W];
static int srcrow_y[2], srcrow_last;
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

void __not_in_flash_func(sms_palette_sync)(int index)
{
    /* CRAM: --BBGGRR, two bits per channel */
    int c = vdp.cram[index];
    pal[index] = PBC_RGB((c & 3) * 85, ((c >> 2) & 3) * 85, ((c >> 4) & 3) * 85);
    pal_changed = true;
}

void __not_in_flash_func(sms_palette_syncGG)(int index)
{
    /* CRAM: GGGGRRRR ----BBBB, four bits per channel */
    int lo = vdp.cram[(index << 1) | 0], hi = vdp.cram[(index << 1) | 1];
    pal[index] = PBC_RGB((lo & 15) * 17, (lo >> 4) * 17, (hi & 15) * 17);
    pal_changed = true;
}

void __not_in_flash_func(sms_palette_syncSG)(int index)
{
    const uint8 *rgb = tms_palette_rgb[index & 15];
    pal[index] = PBC_RGB(rgb[0], rgb[1], rgb[2]);
    pal_changed = true;
}

/* ---- line callback from the core (render.c), core 0 ---------------------- */

/* n bytes (a multiple of 16) in words, here in RAM: memcpy() is in flash.
 * The empty asm keeps GCC from turning the loop back into a memcpy call. */
static inline void copy16(void *dst, const void *src, int n)
{
    uint32_t *d = dst;
    const uint8_t *s = src;
    for (int i = 0; i < n; i += 16, d += 4) {
        uint32_t a, b, c, e;
        memcpy(&a, s + i, 4);
        memcpy(&b, s + i + 4, 4);
        memcpy(&c, s + i + 8, 4);
        memcpy(&e, s + i + 12, 4);
        __asm__ ("" : "+r" (a), "+r" (b), "+r" (c), "+r" (e));
        d[0] = a; d[1] = b; d[2] = c; d[3] = e;
    }
}

/* Keep the line's indices and note its palette: stored again if it changed. */
static inline void __not_in_flash_func(keep_line)(int y, const uint8_t *src, int n)
{
    if (pal_changed || !npal) {
        copy16(pals[back][npal], pal, sizeof pal);
        npal++;
        pal_changed = false;
    }
    line_pal[back][y] = (uint8_t)(npal - 1);
    copy16(ix[back] + y * IX_W, src, n);
}

void __not_in_flash_func(sms_render_line)(int line, const uint8_t *buffer)
{
    if (is_gg) {
        int y = line - GG_Y;
        if (y < 0 || y >= GG_H)
            return;
        if (!buffer) {
            line_pal[back][y] = LINE_BLACK;
            return;
        }
        keep_line(y, buffer + GG_X, GG_W);
    } else {
        if (line < 0 || line >= SMS_HMAX)
            return;
        if (!buffer) {
            line_pal[back][line] = LINE_BLACK;
            return;
        }
        keep_line(line, buffer, IX_W);
        if (line + 1 > lines_drawn)
            lines_drawn = line + 1;
    }
}

/* Core 1: source row y of buffer f in panel colours, as core 0 converted
 * it before; Master System columns scaled here too. */
static void __not_in_flash_func(convert_row)(int f, int y, uint16_t *dst)
{
    int w = is_gg ? GG_W : SMS_W;
    int p = line_pal[f][y];
    if (p == LINE_BLACK) {
        memset(dst, 0, w * sizeof(uint16_t));
        return;
    }
    const uint16_t *pl = pals[f][p];
    const uint8_t *buffer = ix[f] + y * IX_W;
    if (is_gg) {
        for (int i = 0; i < GG_W; i++)
            dst[i] = pl[buffer[i] & 31];
    } else if (sx_plain) {
        const uint8_t *src = buffer + sx_a[0];
        for (int i = 0; i < SMS_W; i++)
            dst[i] = pl[src[i] & 31];
    } else {
        for (int i = 0; i < SMS_W; i++) {
            uint16_t a = pl[buffer[sx_a[i]] & 31];
            dst[i] = sx_a[i] == sx_b[i] ? a : pbc_avg565(a, pl[buffer[sx_b[i]] & 31]);
        }
    }
}

/* Core 1: source row y converted, from the two kept ones if there; `keep`
 * (a row the caller still uses) is not replaced. */
static const uint16_t *__not_in_flash_func(source_row)(int f, int y, const uint16_t *keep)
{
    for (int i = 0; i < 2; i++)
        if (srcrow_y[i] == y) {
            srcrow_last = i;
            return srcrow[i];
        }
    /* replace the one used longer ago, or the one that is not `keep` */
    int i = keep ? (srcrow[0] == keep ? 1 : 0) : 1 - srcrow_last;
    convert_row(f, y, srcrow[i]);
    srcrow_y[i] = y;
    srcrow_last = i;
    return srcrow[i];
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
    npal = 0;                       /* the next frame stores its own palettes */
    if (frame)
        return false;               /* core 1 still busy: draw over it again */
    frame_h = h;
    __dmb();
    frame = back + 1;
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
    /* Save files are written while a game runs: park this core per sector. */
    multicore_lockout_victim_init();
    __dmb();
    core1_ready = true;

    int src_h = 0, out_w = 0, out_h = 0;
    bool over = false;                      /* status goes over the picture */

    for (;;) {
        int f = frame - 1;
        if (f < 0) {
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
        bool cols = is_gg && out_w != GG_W;     /* SMS columns: in convert_row() */
        bool text = over && status_text[0];

        pbc_battwarn_frame(x0, y0, out_w, out_h);
        pbc_display_window(x0, y0, out_w, out_h);
        pbc_display_begin();
        static int8_t scan = -1;
        if (scan < 0)
            scan = pbc_scanlines_on();
        bool batt = pbc_battwarn_rows();
        /* Row by row, converted from the indices into rowbuf[b] while the
         * DMA sends the other one. Rows used again (vertical scaling)
         * come from the two kept source rows. */
        srcrow_y[0] = srcrow_y[1] = -1;
        int b = 0;
        for (int r = 0; r < out_h; r++) {
            bool ov = text && r >= OV_Y && r < OV_Y + OV_H;
            if (!cols && out_h == h) {
                convert_row(f, r, rowbuf[b]);   /* 1:1: straight into it */
            } else {
                const uint16_t *ra = source_row(f, gy_a[r], NULL);
                const uint16_t *rb = gy_b[r] == gy_a[r] ? ra : source_row(f, gy_b[r], ra);
                if (cols || ra != rb)
                    scale_row(ra, rb, out_w, cols, rowbuf[b]);
                else                            /* Master System: 240 wide */
                    copy16(rowbuf[b], ra, out_w * sizeof(uint16_t));
            }
            if (ov)
                overlay_row(rowbuf[b], r, out_w);
            if (scan && (r & 1))
                pbc_scanline_dim(rowbuf[b], out_w);
            if (batt && y0 + r >= 10 && y0 + r < 28)
                pbc_battwarn_row(rowbuf[b], y0 + r, x0, out_w);
            pbc_display_wait();             /* the DMA still sends the other one */
            pbc_display_send(rowbuf[b], out_w);
            b ^= 1;
        }
        pbc_display_wait();
        pbc_display_end();
        __dmb();
        frame = 0;
    }
}
