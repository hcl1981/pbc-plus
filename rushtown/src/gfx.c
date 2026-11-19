/* gfx.c -- drawing into the frame; everything clips to the screen. */
#include <string.h>
#include <math.h>
#include "rt.h"
#include "font8x8.h"

px_t *g_fb;

#define DARK(p) ((px_t)(((p) >> 1) & 0x7befu))

static inline bool clip(int *x, int *y, int *w, int *h)
{
    if (*x < 0) { *w += *x; *x = 0; }
    if (*y < 0) { *h += *y; *y = 0; }
    if (*x + *w > SCR_W) *w = SCR_W - *x;
    if (*y + *h > SCR_H) *h = SCR_H - *y;
    return *w > 0 && *h > 0;
}

void g_clear(px_t c)
{
    for (int i = 0; i < SCR_W * SCR_H; i++)
        g_fb[i] = c;
}

void g_rect(int x, int y, int w, int h, px_t c)
{
    if (!clip(&x, &y, &w, &h))
        return;
    uint32_t cc = (uint32_t)c << 16 | c;
    for (int j = 0; j < h; j++) {
        px_t *p = g_fb + (y + j) * SCR_W + x;
        int n = w;
        if (((uintptr_t)p & 2) && n) {          /* two pixels at a time from here */
            *p++ = c;
            n--;
        }
        uint32_t *q = (uint32_t *)(void *)p;
        for (; n >= 2; n -= 2)
            *q++ = cc;
        if (n)
            *(px_t *)(void *)q = c;
    }
}

void g_dark(int x, int y, int w, int h)
{
    if (!clip(&x, &y, &w, &h))
        return;
    for (int j = 0; j < h; j++) {
        px_t *p = g_fb + (y + j) * SCR_W + x;
        int n = w;
        if (((uintptr_t)p & 2) && n) {
            *p = DARK(*p);
            p++;
            n--;
        }
        uint32_t *q = (uint32_t *)(void *)p;
        for (; n >= 2; n -= 2, q++)
            *q = (*q >> 1) & 0x7bef7befu;
        if (n)
            *(px_t *)(void *)q = DARK(*(px_t *)(void *)q);
    }
}

void g_blend(int x, int y, int w, int h, px_t c)
{
    if (!clip(&x, &y, &w, &h))
        return;
    px_t half = DARK(c);
    for (int j = 0; j < h; j++) {
        px_t *p = g_fb + (y + j) * SCR_W + x;
        for (int i = 0; i < w; i++)
            p[i] = (px_t)(DARK(p[i]) + half);
    }
}

void g_frame(int x, int y, int w, int h, px_t c)
{
    g_rect(x, y, w, 1, c);
    g_rect(x, y + h - 1, w, 1, c);
    g_rect(x, y, 1, h, c);
    g_rect(x + w - 1, y, 1, h, c);
}

void g_pix(int x, int y, px_t c)
{
    if ((unsigned)x < SCR_W && (unsigned)y < SCR_H)
        g_fb[y * SCR_W + x] = c;
}

void g_line(int x0, int y0, int x1, int y1, px_t c)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (int n = 0; n < 600; n++) {
        g_pix(x0, y0, c);
        if (x0 == x1 && y0 == y1)
            break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void g_disc(int cx, int cy, int r, px_t c)
{
    for (int y = -r; y <= r; y++) {
        int w = (int)sqrtf((float)(r * r - y * y) + 0.5f);
        g_rect(cx - w, cy + y, 2 * w + 1, 1, c);
    }
}

void g_ring(int cx, int cy, int r, px_t c)
{
    int x = r, y = 0, e = 1 - r;
    while (x >= y) {
        g_pix(cx + x, cy + y, c); g_pix(cx - x, cy + y, c);
        g_pix(cx + x, cy - y, c); g_pix(cx - x, cy - y, c);
        g_pix(cx + y, cy + x, c); g_pix(cx - y, cy + x, c);
        g_pix(cx + y, cy - x, c); g_pix(cx - y, cy - x, c);
        y++;
        if (e < 0)
            e += 2 * y + 1;
        else {
            x--;
            e += 2 * (y - x) + 1;
        }
    }
}

void g_tile(const px_t *t, int x, int y)
{
    if (x >= 0 && y >= 0 && x <= SCR_W - TILE && y <= SCR_H - TILE) {
        px_t *p = g_fb + y * SCR_W + x;
        for (int j = 0; j < TILE; j++, p += SCR_W, t += TILE)
            memcpy(p, t, TILE * sizeof(px_t));
        return;
    }
    int x0 = x < 0 ? -x : 0, x1 = x + TILE > SCR_W ? SCR_W - x : TILE;
    int y0 = y < 0 ? -y : 0, y1 = y + TILE > SCR_H ? SCR_H - y : TILE;
    if (x1 <= x0)
        return;
    for (int j = y0; j < y1; j++)
        memcpy(g_fb + (y + j) * SCR_W + x + x0, t + j * TILE + x0, (size_t)(x1 - x0) * sizeof(px_t));
}

void g_sprite(const uint8_t *pix, int w, int h, const px_t *pal, int x, int y)
{
    for (int j = 0; j < h; j++) {
        int yy = y + j;
        if ((unsigned)yy >= SCR_H)
            continue;
        for (int i = 0; i < w; i++) {
            int c = pix[j * w + i], xx = x + i;
            if (c && (unsigned)xx < SCR_W)
                g_fb[yy * SCR_W + xx] = pal[c];
        }
    }
}

/* What a hit car's pixel looks like. The colour numbers are those of the
 * car drawings in art.c: 2 body, 3 its shade, 4 its highlight, 5 and 6
 * glass, 7 head light, 8 tail light, 1 outline, 9 black. An end that was
 * hit hard is shorter by a ragged row or two, its lights are out, its
 * metal is blotched; glass next to it is cracked. */
static inline int dented(int col, int su, int sv, int w, int h, const uint8_t *lv)
{
    unsigned r = ((unsigned)(su * 7 + sv * 13) * 2654435761u) >> 28;    /* 0..15, fixed per pixel */
    for (int end = 0; end < 2; end++) {
        int l = lv[end], d = end ? h - 1 - sv : sv;         /* rows from that end */
        if (!l)
            continue;
        if (d < l - 1 || (d == l - 1 && (r & 1) && l > 1))
            return 0;
        if (d < 3 + l) {
            if (col == 7 || col == 8)
                return 9;
            if ((col == 2 || col == 4) && (int)r < l * 4)
                return r & 2 ? 1 : 3;
        } else if (l == 3 && (col == 5 || col == 6) && d < 10 && r < 5)
            return 4;
    }
    for (int side = 0; side < 2; side++) {
        int l = lv[2 + side], d = side ? w - 1 - su : su;
        if (!l)
            continue;
        if (d < 2 + l) {
            if ((col == 2 || col == 4 || col == 3) && (int)r < l * 4)
                return r & 2 ? 1 : 9;
            if (l >= 2 && (col == 5 || col == 6) && r < 6)
                return 4;
        }
    }
    return col;
}

void g_sprite_rot(const uint8_t *pix, int w, int h, const px_t *pal,
                  float cx, float cy, float a, bool shadow)
{
    g_car(pix, w, h, pal, cx, cy, a, shadow, NULL);
}

void g_car(const uint8_t *pix, int w, int h, const px_t *pal,
           float cx, float cy, float a, bool shadow, const uint8_t *level)
{
    float s = sinf(a), c = cosf(a);
    int r = (int)(sqrtf((float)(w * w + h * h)) * 0.5f) + 1;
    int icx = (int)floorf(cx), icy = (int)floorf(cy);
    int x0 = icx - r, x1 = icx + r, y0 = icy - r, y1 = icy + r;
    if (x1 < 0 || y1 < 0 || x0 >= SCR_W || y0 >= SCR_H)
        return;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= SCR_W) x1 = SCR_W - 1;
    if (y1 >= SCR_H) y1 = SCR_H - 1;
    /* screen offset (dx, dy) -> picture: right = dx c + dy s, forward = dx s - dy c */
    int32_t ic = (int32_t)(c * 65536.0f), is = (int32_t)(s * 65536.0f);
    int32_t hw = w * 32768, hh = h * 32768;
    for (int y = y0; y <= y1; y++) {
        float dy = (float)y + 0.5f - cy, dx = (float)x0 + 0.5f - cx;
        int32_t u = (int32_t)((dx * c + dy * s) * 65536.0f) + hw;
        int32_t v = hh - (int32_t)((dx * s - dy * c) * 65536.0f);
        px_t *p = g_fb + y * SCR_W + x0;
        for (int x = x0; x <= x1; x++, p++, u += ic, v -= is) {
            int su = u >> 16, sv = v >> 16;
            if ((unsigned)su >= (unsigned)w || (unsigned)sv >= (unsigned)h)
                continue;
            int col = pix[sv * w + su];
            if (col && level)
                col = dented(col, su, sv, w, h, level);
            if (!col)
                continue;
            *p = shadow ? DARK(*p) : pal[col];
        }
    }
}

/* The font is ASCII. The seven letters German needs on top are made from
 * it: two dots over A O U a o u, and a drawn sharp s. In the texts they are
 * UTF-8, which for these is 0xC3 and one more byte. */
static const uint8_t *glyph(const char **s)
{
    static uint8_t extra[7][8];
    static bool made;
    static const char base[7] = "AOUaou";
    static const uint8_t second[7] = { 0x84, 0x96, 0x9c, 0xa4, 0xb6, 0xbc, 0x9f };
    unsigned ch = (unsigned char)**s;
    if (!made) {
        made = true;
        for (int i = 0; i < 6; i++) {
            const uint8_t *b = font8x8_basic[base[i] - 0x20];
            bool cap = i < 3;
            extra[i][0] = cap ? 0x33 : 0x12;
            for (int r = 1; r < 8; r++)         /* capitals move down a row to make room */
                extra[i][r] = b[cap ? r - 1 : r];
        }
        {   /* the capital U is drawn: its dots would sit on its own strokes, so a row stays free */
            static const uint8_t ue[8] = { 0x33, 0x00, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3f };
            memcpy(extra[2], ue, 8);
        }
        static const uint8_t sz[8] = { 0x1e, 0x33, 0x33, 0x1b, 0x33, 0x33, 0x1b, 0x03 };
        memcpy(extra[6], sz, 8);
    }
    if (ch == 0xc3 && (*s)[1]) {
        (*s)++;
        for (int i = 0; i < 7; i++)
            if ((unsigned char)**s == second[i])
                return extra[i];
        return font8x8_basic['?' - 0x20];
    }
    if (ch < 0x20 || ch > 0x7e)
        ch = '?';
    return font8x8_basic[ch - 0x20];
}

int g_text_w(const char *s, int scale)
{
    int n = 0;
    for (; *s; s++)
        if (((unsigned char)*s & 0xc0) != 0x80)     /* not the second byte of a letter */
            n++;
    return n * 8 * scale;
}

#if RT_ART
extern const uint8_t pics_blob[];
bool g_picture(int id, int x, int y)
{
    const uint32_t *head = (const uint32_t *)(const void *)pics_blob;
    if ((unsigned)id >= head[0])
        return false;
    const uint16_t *size = (const uint16_t *)(const void *)(pics_blob + 4 + 8 * id);
    const px_t *pal = (const px_t *)(const void *)(pics_blob + head[2 + 2 * id]);
    const uint8_t *pix = (const uint8_t *)(pal + 256);
    int w = size[0], h = size[1];
    for (int j = 0; j < h; j++) {
        int yy = y + j;
        if ((unsigned)yy >= SCR_H)
            continue;
        px_t *out = g_fb + yy * SCR_W;
        const uint8_t *row = pix + j * w;
        for (int i = 0; i < w; i++)
            if ((unsigned)(x + i) < SCR_W)
                out[x + i] = pal[row[i]];
    }
    return true;
}
#else
bool g_picture(int id, int x, int y) { (void)id; (void)x; (void)y; return false; }
#endif

int g_text(int x, int y, const char *s, px_t c, int scale)
{
    int x0 = x;
    for (; *s; s++, x += 8 * scale) {
        const uint8_t *g = glyph(&s);
        for (int j = 0; j < 8; j++)
            for (int i = 0; i < 8; i++)
                if (g[j] & (1u << i)) {
                    if (scale == 1)
                        g_pix(x + i, y + j, c);
                    else
                        g_rect(x + i * scale, y + j * scale, scale, scale, c);
                }
    }
    return x - x0;
}

void g_text_s(int x, int y, const char *s, px_t c, int scale)
{
    g_text(x + scale, y + scale, s, RGB(0, 0, 0), scale);
    g_text(x, y, s, c, scale);
}

void g_text_o(int x, int y, const char *s, px_t c, int scale)
{
    for (int j = -1; j <= 1; j++)
        for (int i = -1; i <= 1; i++)
            if (i || j)
                g_text(x + i * scale, y + j * scale, s, RGB(0, 0, 0), scale);
    g_text(x, y, s, c, scale);
}

void g_text_c(int cx, int y, const char *s, px_t c, int scale)
{
    g_text_s(cx - g_text_w(s, scale) / 2, y, s, c, scale);
}
