/* gfx.c -- see gfx.h */
#include <string.h>
#include "gfx.h"
#include "color565.h"
#ifndef PBC_HOST
#include "display.h"
#endif

static int cx0, cy0, cx1 = GFX_W, cy1 = GFX_H;

#ifdef GFX_STRIP
/* In the emulators (common/pbc_loading.c) there is no room for a frame
 * buffer: the caller points gfx_fb at a few rows and clips to them. */
uint16_t *gfx_fb;
#else
static uint16_t fbs[2][GFX_W * GFX_H];
static int back;                    /* index of the buffer being drawn */
static bool sending;
uint16_t *gfx_fb = fbs[0];

void gfx_init(void)
{
    back = 0;
    gfx_fb = fbs[0];
    gfx_noclip();
}
#endif

#ifdef GFX_STRIP
#elif defined(PBC_HOST)
/* host preview: the preview program takes each frame from gfx_fb */
void host_frame(void);
void gfx_present(void) { host_frame(); }
void gfx_finish(void) {}
#else
void gfx_finish(void)
{
    if (sending) {
        pbc_display_wait();
        pbc_display_end();
        sending = false;
    }
}

void gfx_present(void)
{
    gfx_finish();
    pbc_display_window(0, 0, GFX_W, GFX_H);
    pbc_display_begin();
    pbc_display_send(fbs[back], GFX_W * GFX_H);
    sending = true;
    back ^= 1;
    gfx_fb = fbs[back];
}
#endif

void gfx_clip(int x0, int y0, int x1, int y1)
{
    cx0 = x0 < 0 ? 0 : x0;
    cy0 = y0 < 0 ? 0 : y0;
    cx1 = x1 > GFX_W ? GFX_W : x1;
    cy1 = y1 > GFX_H ? GFX_H : y1;
}

void gfx_noclip(void)
{
    gfx_clip(0, 0, GFX_W, GFX_H);
}

/* ---- colour ---------------------------------------------------------------- */

static inline uint16_t panel(uint32_t rgb)
{
    return pbc_rgb24(rgb);
}

/* Blend panel word c over d with alpha 0..256. Works on the 5/6/5 fields
 * whatever their order. */
static inline uint16_t blend(uint16_t d, uint16_t c, unsigned a)
{
    uint32_t dd = (d | ((uint32_t)d << 16)) & 0x07E0F81Fu;
    uint32_t cc = (c | ((uint32_t)c << 16)) & 0x07E0F81Fu;
    a >>= 3;                                    /* 0..32 */
    uint32_t r = (dd + (((cc - dd) * a) >> 5)) & 0x07E0F81Fu;
    return (uint16_t)(r | (r >> 16));
}

uint32_t gfx_mix(uint32_t a, uint32_t b, int t)
{
    uint32_t r = 0;
    for (int s = 0; s < 24; s += 8) {
        int ca = (a >> s) & 0xff, cb = (b >> s) & 0xff;
        r |= (uint32_t)(ca + (cb - ca) * t / 255) << s;
    }
    return r;
}

static inline void put(int x, int y, uint16_t c, unsigned a)
{
    if (x < cx0 || x >= cx1 || y < cy0 || y >= cy1 || a == 0)
        return;
    uint16_t *p = &gfx_fb[y * GFX_W + x];
    *p = a >= 256 ? c : blend(*p, c, a);
}

/* ---- shapes ------------------------------------------------------------------ */

void gfx_background(uint32_t top, uint32_t bottom)
{
    for (int y = cy0; y < cy1; y++) {
        uint16_t c = panel(gfx_mix(top, bottom, y * 255 / (GFX_H - 1)));
        uint16_t *p = &gfx_fb[y * GFX_W];
        for (int x = cx0; x < cx1; x++)
            p[x] = c;
    }
}

void gfx_rect(int x, int y, int w, int h, uint32_t rgb, int alpha)
{
    int x0 = x < cx0 ? cx0 : x, x1 = x + w > cx1 ? cx1 : x + w;
    int y0 = y < cy0 ? cy0 : y, y1 = y + h > cy1 ? cy1 : y + h;
    uint16_t c = panel(rgb);
    unsigned a = alpha >= 255 ? 256 : (unsigned)alpha;
    for (int yy = y0; yy < y1; yy++) {
        uint16_t *p = &gfx_fb[yy * GFX_W];
        for (int xx = x0; xx < x1; xx++)
            p[xx] = a >= 256 ? c : blend(p[xx], c, a);
    }
}

/* Coverage (0..256) of pixel (px, py) by a disc around (cx, cy), all in 1/16
 * pixel units -- 4x4 samples are plenty for these sizes. */
static unsigned disc_cover(int px, int py, int cx16, int cy16, int r16)
{
    unsigned n = 0;
    int r2 = r16 * r16;
    for (int sy = 0; sy < 4; sy++)
        for (int sx = 0; sx < 4; sx++) {
            int dx = px * 16 + sx * 4 + 2 - cx16, dy = py * 16 + sy * 4 + 2 - cy16;
            if (dx * dx + dy * dy <= r2)
                n++;
        }
    return n * 16;
}

void gfx_rrect(int x, int y, int w, int h, int r, uint32_t rgb, int alpha)
{
    if (r * 2 > h) r = h / 2;
    if (r * 2 > w) r = w / 2;
    if (r <= 0) {
        gfx_rect(x, y, w, h, rgb, alpha);
        return;
    }
    gfx_rect(x, y + r, w, h - 2 * r, rgb, alpha);
    gfx_rect(x + r, y, w - 2 * r, r, rgb, alpha);
    gfx_rect(x + r, y + h - r, w - 2 * r, r, rgb, alpha);
    uint16_t c = panel(rgb);
    unsigned a = alpha >= 255 ? 256 : (unsigned)alpha;
    /* corners: quarter discs centred r pixels in from each corner */
    int cxl = (x + r) * 16, cxr = (x + w - r) * 16, cyt = (y + r) * 16, cyb = (y + h - r) * 16;
    for (int yy = 0; yy < r; yy++)
        for (int xx = 0; xx < r; xx++) {
            unsigned cov;
            cov = disc_cover(x + xx, y + yy, cxl, cyt, r * 16);
            put(x + xx, y + yy, c, cov * a >> 8);
            cov = disc_cover(x + w - 1 - xx, y + yy, cxr, cyt, r * 16);
            put(x + w - 1 - xx, y + yy, c, cov * a >> 8);
            cov = disc_cover(x + xx, y + h - 1 - yy, cxl, cyb, r * 16);
            put(x + xx, y + h - 1 - yy, c, cov * a >> 8);
            cov = disc_cover(x + w - 1 - xx, y + h - 1 - yy, cxr, cyb, r * 16);
            put(x + w - 1 - xx, y + h - 1 - yy, c, cov * a >> 8);
        }
}

void gfx_circle(int cx, int cy, int r, uint32_t rgb, int alpha)
{
    uint16_t c = panel(rgb);
    unsigned a = alpha >= 255 ? 256 : (unsigned)alpha;
    for (int y = cy - r - 1; y <= cy + r; y++)
        for (int x = cx - r - 1; x <= cx + r; x++)
            put(x, y, c, disc_cover(x, y, cx * 16, cy * 16, r * 16) * a >> 8);
}

void gfx_glow(int cx, int cy, int rx, int ry, uint32_t rgb, int alpha)
{
    uint16_t c = panel(rgb);
    int y0 = cy - ry < cy0 ? cy0 : cy - ry, y1 = cy + ry > cy1 ? cy1 : cy + ry;
    int x0 = cx - rx < cx0 ? cx0 : cx - rx, x1 = cx + rx > cx1 ? cx1 : cx + rx;
    for (int y = y0; y < y1; y++) {
        int dy = (y - cy) * 256 / ry;
        uint16_t *p = &gfx_fb[y * GFX_W];
        for (int x = x0; x < x1; x++) {
            int dx = (x - cx) * 256 / rx;
            int d2 = (dx * dx + dy * dy) >> 8;          /* 0..256 at the rim */
            if (d2 >= 256)
                continue;
            int f = 256 - d2;                           /* smooth: f^2 */
            unsigned a = (unsigned)(f * f >> 8) * (unsigned)alpha >> 8;
            p[x] = blend(p[x], c, a);
        }
    }
}

void gfx_chevron(int cx, int cy, int size, int dir, uint32_t rgb, int alpha)
{
    /* two thick strokes meeting at the tip, drawn as discs along each line */
    int tip = cx + dir * size / 2, back_x = cx - dir * size / 2;
    for (int i = 0; i <= 16; i++) {
        int x = back_x + (tip - back_x) * i / 16;
        int y1 = cy - size + size * i / 16, y2 = cy + size - size * i / 16;
        for (int yy = y1 - 2; yy <= y1 + 2; yy++)
            for (int xx = x - 2; xx <= x + 2; xx++)
                put(xx, yy, panel(rgb), disc_cover(xx, yy, x * 16, y1 * 16, 32) * (unsigned)alpha >> 8);
        for (int yy = y2 - 2; yy <= y2 + 2; yy++)
            for (int xx = x - 2; xx <= x + 2; xx++)
                put(xx, yy, panel(rgb), disc_cover(xx, yy, x * 16, y2 * 16, 32) * (unsigned)alpha >> 8);
    }
}

/* ---- pictures ------------------------------------------------------------------ */

void gfx_picture(const pbc_picture_t *p, int x, int y, int alpha)
{
    const uint8_t *s = p->rle;
    int n = p->w * p->h, i = 0;
    unsigned am = alpha >= 255 ? 256 : (unsigned)alpha;
    while (i < n) {
        int c = *s++, count;
        bool run = c >= 128;
        count = run ? c - 126 : c + 1;
        for (int k = 0; k < count; k++, i++) {
            int idx = run ? *s : s[k];
            unsigned a = p->alpha[idx];
            if (a) {
                a = (a * 17 + (a == 15)) * am >> 8;     /* 4 bit -> 0..256 */
                put(x + i % p->w, y + i / p->w, p->pal[idx], a);
            }
        }
        s += run ? 1 : count;
    }
}

/* ---- text ------------------------------------------------------------------------ */

static const pbc_glyph_t *glyph(const pbc_font_t *f, unsigned char ch)
{
    if (ch < 32)
        ch = '?';
    return &f->glyphs[ch - 32];
}

int gfx_text(const pbc_font_t *f, int x, int y, const char *s, uint32_t rgb, int alpha)
{
    uint16_t c = panel(rgb);
    unsigned am = alpha >= 255 ? 256 : (unsigned)alpha;
    int x0 = x;
    for (; *s; s++) {
        const pbc_glyph_t *g = glyph(f, (unsigned char)*s);
        const uint8_t *b = f->bits + g->off;
        int stride = (g->w + 1) / 2;
        for (int gy = 0; gy < g->h; gy++)
            for (int gx = 0; gx < g->w; gx++) {
                unsigned v = b[gy * stride + gx / 2];
                v = (gx & 1) ? (v & 15) : (v >> 4);
                if (v)
                    put(x + g->x + gx, y + g->y + gy, c, (v * 17 + (v == 15)) * am >> 8);
            }
        x += g->adv;
    }
    return x - x0;
}

int gfx_text_w(const pbc_font_t *f, const char *s)
{
    int w = 0;
    for (; *s; s++)
        w += glyph(f, (unsigned char)*s)->adv;
    return w;
}

void gfx_text_centered(const pbc_font_t *f, int cx, int y, const char *s, uint32_t rgb, int alpha)
{
    gfx_text(f, cx - gfx_text_w(f, s) / 2, y, s, rgb, alpha);
}

void gfx_text_fit(const pbc_font_t *f, const char *s, int max_w, char *out)
{
    strcpy(out, s);
    if (gfx_text_w(f, out) <= max_w)
        return;
    int dots = gfx_text_w(f, "...");
    size_t n = strlen(out);
    while (n > 0) {
        out[--n] = 0;
        if (gfx_text_w(f, out) + dots <= max_w)
            break;
    }
    while (n > 0 && out[n - 1] == ' ')
        out[--n] = 0;
    strcpy(out + n, "...");
}
