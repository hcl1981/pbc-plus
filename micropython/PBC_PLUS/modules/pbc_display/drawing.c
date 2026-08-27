// Drawing primitives for the PBC+ display module.
// See drawing.h for design notes.

#include "drawing.h"
#include "st7789.h"
#include "font12.h"
#include "font18.h"
#include "font24.h"

#include "extmod/font_petme128_8x8.h"  // 8x8, shared with framebuf

#include <stdlib.h>
#include <string.h>

// ---- Helpers ------------------------------------------------------------

static inline int clamp_lo(int v, int lo) { return v < lo ? lo : v; }
static inline int clamp_hi(int v, int hi) { return v > hi ? hi : v; }

static void hline_fast(int x, int y, int w, uint16_t color) {
    if (y < 0 || y >= ST7789_HEIGHT) return;
    if (x < 0) { w += x; x = 0; }
    if (x + w > ST7789_WIDTH) w = ST7789_WIDTH - x;
    if (w <= 0) return;
    st7789_set_window(x, y, x + w - 1, y);
    st7789_fill_color(color, (uint32_t)w);
}

static void vline_fast(int x, int y, int h, uint16_t color) {
    if (x < 0 || x >= ST7789_WIDTH) return;
    if (y < 0) { h += y; y = 0; }
    if (y + h > ST7789_HEIGHT) h = ST7789_HEIGHT - y;
    if (h <= 0) return;
    st7789_set_window(x, y, x, y + h - 1);
    st7789_fill_color(color, (uint32_t)h);
}

// ---- Line ---------------------------------------------------------------

void draw_line(int x0, int y0, int x1, int y1, uint16_t color) {
    if (y0 == y1) {
        int xa = x0 < x1 ? x0 : x1;
        int xb = x0 < x1 ? x1 : x0;
        hline_fast(xa, y0, xb - xa + 1, color);
        return;
    }
    if (x0 == x1) {
        int ya = y0 < y1 ? y0 : y1;
        int yb = y0 < y1 ? y1 : y0;
        vline_fast(x0, ya, yb - ya + 1, color);
        return;
    }

    int dx =  abs(x1 - x0);
    int dy = -abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        st7789_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = err << 1;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// ---- Rectangle outline --------------------------------------------------

void draw_rect_outline(int x, int y, int w, int h, uint16_t color) {
    if (w <= 0 || h <= 0) return;
    if (h == 1) { hline_fast(x, y, w, color); return; }
    if (w == 1) { vline_fast(x, y, h, color); return; }

    hline_fast(x,         y,         w, color);
    hline_fast(x,         y + h - 1, w, color);
    vline_fast(x,         y + 1, h - 2, color);
    vline_fast(x + w - 1, y + 1, h - 2, color);
}

// ---- Ellipse ------------------------------------------------------------

#define MASK_NE 0x01
#define MASK_NW 0x02
#define MASK_SE 0x04
#define MASK_SW 0x08

static inline void plot4(int cx, int cy, int x, int y,
                         uint16_t color, uint8_t mask) {
    if (mask & MASK_NE) st7789_pixel(cx + x, cy - y, color);
    if (mask & MASK_NW) st7789_pixel(cx - x, cy - y, color);
    if (mask & MASK_SE) st7789_pixel(cx + x, cy + y, color);
    if (mask & MASK_SW) st7789_pixel(cx - x, cy + y, color);
}

static inline void fill4(int cx, int cy, int x, int y,
                         uint16_t color, uint8_t mask) {
    bool ne = mask & MASK_NE, nw = mask & MASK_NW;
    bool se = mask & MASK_SE, sw = mask & MASK_SW;

    if (ne || nw) {
        int xl = nw ? (cx - x) : cx;
        int xr = ne ? (cx + x) : cx;
        if (xl <= xr) hline_fast(xl, cy - y, xr - xl + 1, color);
    }
    if ((se || sw) && y != 0) {
        int xl = sw ? (cx - x) : cx;
        int xr = se ? (cx + x) : cx;
        if (xl <= xr) hline_fast(xl, cy + y, xr - xl + 1, color);
    }
}

void draw_ellipse(int cx, int cy, int xr, int yr, uint16_t color,
                  bool fill, uint8_t mask) {
    if (xr <= 0 || yr <= 0) {
        if (xr == 0 && yr == 0) st7789_pixel(cx, cy, color);
        return;
    }

    long xr2 = (long)xr * xr;
    long yr2 = (long)yr * yr;
    int x = 0, y = yr;
    long p = yr2 - xr2 * yr + xr2 / 4;

    while (yr2 * x < xr2 * y) {
        if (fill) fill4(cx, cy, x, y, color, mask);
        else      plot4(cx, cy, x, y, color, mask);
        x++;
        if (p < 0) {
            p += yr2 * (2 * x + 1);
        } else {
            y--;
            p += yr2 * (2 * x + 1) - 2 * xr2 * y;
        }
    }

    p = yr2 * (2 * x + 1) * (2 * x + 1) / 4
      + xr2 * (y - 1) * (y - 1)
      - xr2 * yr2;
    while (y >= 0) {
        if (fill) fill4(cx, cy, x, y, color, mask);
        else      plot4(cx, cy, x, y, color, mask);
        y--;
        if (p > 0) {
            p -= xr2 * (2 * y + 1);
        } else {
            x++;
            p += yr2 * (2 * x + 1) - xr2 * (2 * y + 1);
        }
    }
}

// ---- Polygon ------------------------------------------------------------

void draw_poly_outline(int ox, int oy, const int16_t *coords,
                       int n_points, uint16_t color) {
    if (n_points < 2) return;
    for (int i = 0; i < n_points; ++i) {
        int j = (i + 1) % n_points;
        draw_line(
            ox + coords[2 * i],     oy + coords[2 * i + 1],
            ox + coords[2 * j],     oy + coords[2 * j + 1],
            color);
    }
}

#define MAX_POLY_VERTICES 64

void draw_poly_fill(int ox, int oy, const int16_t *coords,
                    int n_points, uint16_t color) {
    if (n_points < 3) return;
    if (n_points > MAX_POLY_VERTICES) n_points = MAX_POLY_VERTICES;

    int y_min = oy + coords[1];
    int y_max = y_min;
    for (int i = 1; i < n_points; ++i) {
        int yi = oy + coords[2 * i + 1];
        if (yi < y_min) y_min = yi;
        if (yi > y_max) y_max = yi;
    }
    y_min = clamp_lo(y_min, 0);
    y_max = clamp_hi(y_max, ST7789_HEIGHT - 1);

    int xs[MAX_POLY_VERTICES];

    for (int sy = y_min; sy <= y_max; ++sy) {
        int n_inter = 0;
        for (int i = 0; i < n_points; ++i) {
            int j = (i + 1) % n_points;
            int y0 = oy + coords[2 * i + 1];
            int y1 = oy + coords[2 * j + 1];
            int x0 = ox + coords[2 * i];
            int x1 = ox + coords[2 * j];

            if ((y0 <= sy && y1 > sy) || (y1 <= sy && y0 > sy)) {
                int xi = x0 + (sy - y0) * (x1 - x0) / (y1 - y0);
                xs[n_inter++] = xi;
            }
        }

        for (int i = 1; i < n_inter; ++i) {
            int v = xs[i];
            int k = i - 1;
            while (k >= 0 && xs[k] > v) {
                xs[k + 1] = xs[k];
                --k;
            }
            xs[k + 1] = v;
        }

        for (int i = 0; i + 1 < n_inter; i += 2) {
            int xa = xs[i];
            int xb = xs[i + 1];
            if (xa > xb) { int t = xa; xa = xb; xb = t; }
            hline_fast(xa, sy, xb - xa + 1, color);
        }
    }
}

// ---- 8x8 petme text -----------------------------------------------------

static void draw_glyph_opaque(uint8_t c, int x, int y,
                              uint16_t fg, uint16_t bg) {
    if (c < 32 || c > 127) c = '?';
    const uint8_t *g = &font_petme128_8x8[(c - 32) * 8];

    if (x + 8 <= 0 || x >= ST7789_WIDTH ||
        y + 8 <= 0 || y >= ST7789_HEIGHT) {
        return;
    }

    if (x >= 0 && y >= 0 &&
        x + 8 <= ST7789_WIDTH && y + 8 <= ST7789_HEIGHT) {
        uint16_t pix[64];
        for (int col = 0; col < 8; ++col) {
            uint8_t bits = g[col];
            for (int row = 0; row < 8; ++row) {
                pix[row * 8 + col] = (bits & (1 << row)) ? fg : bg;
            }
        }
        st7789_set_window(x, y, x + 7, y + 7);
        st7789_blit_pixels(pix, 64);
        return;
    }

    for (int col = 0; col < 8; ++col) {
        uint8_t bits = g[col];
        for (int row = 0; row < 8; ++row) {
            int px = x + col, py = y + row;
            if (px < 0 || px >= ST7789_WIDTH ||
                py < 0 || py >= ST7789_HEIGHT) continue;
            st7789_pixel(px, py, (bits & (1 << row)) ? fg : bg);
        }
    }
}

static void draw_glyph_transparent(uint8_t c, int x, int y, uint16_t fg) {
    if (c < 32 || c > 127) c = '?';
    const uint8_t *g = &font_petme128_8x8[(c - 32) * 8];

    if (x + 8 <= 0 || x >= ST7789_WIDTH ||
        y + 8 <= 0 || y >= ST7789_HEIGHT) {
        return;
    }

    for (int col = 0; col < 8; ++col) {
        uint8_t bits = g[col];
        if (bits == 0) continue;
        int px = x + col;
        if (px < 0 || px >= ST7789_WIDTH) continue;
        for (int row = 0; row < 8; ++row) {
            if (!(bits & (1 << row))) continue;
            int py = y + row;
            if (py < 0 || py >= ST7789_HEIGHT) continue;
            st7789_pixel(px, py, fg);
        }
    }
}

void draw_text(const char *s, int x, int y, uint16_t fg,
               uint16_t bg, bool transparent) {
    while (*s) {
        if (transparent) draw_glyph_transparent((uint8_t)*s, x, y, fg);
        else             draw_glyph_opaque((uint8_t)*s, x, y, fg, bg);
        x += 8;
        ++s;
    }
}

// ---- 12-px proportional sans-serif text --------------------------------
//
// Glyph layout (see font12.h): 12 rows per glyph, each row is two
// bytes (low byte first). Bit 0 of byte 0 is the leftmost pixel,
// bit 7 of byte 1 is the rightmost. Per-glyph widths in
// font12_widths[] give the meaningful column count.

#define FONT12_TRACKING 1   // pixels of space between glyphs

int text12_width(const char *s) {
    int w = 0;
    while (*s) {
        uint8_t c = (uint8_t)*s++;
        if (c < FONT12_FIRST_CHAR || c > FONT12_LAST_CHAR) c = '?';
        w += font12_widths[c - FONT12_FIRST_CHAR] + FONT12_TRACKING;
    }
    if (w > 0) w -= FONT12_TRACKING;  // no trailing tracking
    return w;
}

// One glyph -- opaque path. Builds an (advance x H) RGB565 buffer
// and DMAs the whole block. Slightly wasteful for narrow glyphs but
// keeps SPI utilisation high.
static void draw_glyph12_opaque(uint8_t c, int x, int y,
                                int advance,
                                uint16_t fg, uint16_t bg) {
    if (c < FONT12_FIRST_CHAR || c > FONT12_LAST_CHAR) c = '?';
    const uint8_t *g = &font12_data[(c - FONT12_FIRST_CHAR)
                                    * FONT12_BYTES_PER_GLYPH];
    int gw = font12_widths[c - FONT12_FIRST_CHAR];

    // Off-screen?
    if (x + advance <= 0 || x >= ST7789_WIDTH ||
        y + FONT12_HEIGHT <= 0 || y >= ST7789_HEIGHT) {
        return;
    }

    // On-screen fast path: render glyph + tracking column into a
    // small RAM buffer then ship it as one blit.
    if (x >= 0 && y >= 0 &&
        x + advance <= ST7789_WIDTH &&
        y + FONT12_HEIGHT <= ST7789_HEIGHT) {
        uint16_t pix[FONT12_HEIGHT * (FONT12_MAX_WIDTH + FONT12_TRACKING)];
        int idx = 0;
        for (int row = 0; row < FONT12_HEIGHT; ++row) {
            uint16_t bits = g[row * FONT12_ROW_BYTES]
                          | ((uint16_t)g[row * FONT12_ROW_BYTES + 1] << 8);
            for (int col = 0; col < advance; ++col) {
                bool on = (col < gw) && (bits >> col) & 1;
                pix[idx++] = on ? fg : bg;
            }
        }
        st7789_set_window(x, y, x + advance - 1, y + FONT12_HEIGHT - 1);
        st7789_blit_pixels(pix, (uint32_t)idx);
        return;
    }

    // Clipped path: per-pixel.
    for (int row = 0; row < FONT12_HEIGHT; ++row) {
        uint16_t bits = g[row * FONT12_ROW_BYTES]
                      | ((uint16_t)g[row * FONT12_ROW_BYTES + 1] << 8);
        int py = y + row;
        if (py < 0 || py >= ST7789_HEIGHT) continue;
        for (int col = 0; col < advance; ++col) {
            int px = x + col;
            if (px < 0 || px >= ST7789_WIDTH) continue;
            bool on = (col < gw) && (bits >> col) & 1;
            st7789_pixel(px, py, on ? fg : bg);
        }
    }
}

// Transparent path -- only set foreground pixels.
static void draw_glyph12_transparent(uint8_t c, int x, int y, uint16_t fg) {
    if (c < FONT12_FIRST_CHAR || c > FONT12_LAST_CHAR) c = '?';
    const uint8_t *g = &font12_data[(c - FONT12_FIRST_CHAR)
                                    * FONT12_BYTES_PER_GLYPH];
    int gw = font12_widths[c - FONT12_FIRST_CHAR];

    if (x + gw <= 0 || x >= ST7789_WIDTH ||
        y + FONT12_HEIGHT <= 0 || y >= ST7789_HEIGHT) {
        return;
    }

    for (int row = 0; row < FONT12_HEIGHT; ++row) {
        uint16_t bits = g[row * FONT12_ROW_BYTES]
                      | ((uint16_t)g[row * FONT12_ROW_BYTES + 1] << 8);
        if (bits == 0) continue;
        int py = y + row;
        if (py < 0 || py >= ST7789_HEIGHT) continue;
        for (int col = 0; col < gw; ++col) {
            if (!((bits >> col) & 1)) continue;
            int px = x + col;
            if (px < 0 || px >= ST7789_WIDTH) continue;
            st7789_pixel(px, py, fg);
        }
    }
}

int draw_text12(const char *s, int x, int y, uint16_t fg,
                uint16_t bg, bool transparent) {
    while (*s) {
        uint8_t c = (uint8_t)*s++;
        if (c < FONT12_FIRST_CHAR || c > FONT12_LAST_CHAR) c = '?';
        int gw = font12_widths[c - FONT12_FIRST_CHAR];
        int advance = gw + FONT12_TRACKING;

        if (transparent) {
            draw_glyph12_transparent(c, x, y, fg);
        } else {
            draw_glyph12_opaque(c, x, y, advance, fg, bg);
        }
        x += advance;
    }
    return x;
}

// Render text12 into an RGB565 framebuffer (u16 per pixel, native LE).
// `bg` is the opaque background colour; if has_bg is false, only foreground
// pixels are written and the background is left untouched (transparent).
int draw_text12_fb(uint16_t *fb, int fb_w, int fb_h,
                   const char *s, int x, int y,
                   uint16_t fg, bool has_bg, uint16_t bg) {
    while (*s) {
        uint8_t c = (uint8_t)*s++;
        if (c < FONT12_FIRST_CHAR || c > FONT12_LAST_CHAR) c = '?';
        const uint8_t *g = &font12_data[(c - FONT12_FIRST_CHAR)
                                        * FONT12_BYTES_PER_GLYPH];
        int gw = font12_widths[c - FONT12_FIRST_CHAR];
        int advance = gw + FONT12_TRACKING;

        // Skip glyphs entirely off the buffer.
        if (x + advance <= 0 || x >= fb_w ||
            y + FONT12_HEIGHT <= 0 || y >= fb_h) {
            x += advance;
            continue;
        }

        for (int row = 0; row < FONT12_HEIGHT; row++) {
            int py = y + row;
            if (py < 0 || py >= fb_h) continue;
            uint16_t bits = g[row * FONT12_ROW_BYTES]
                          | ((uint16_t)g[row * FONT12_ROW_BYTES + 1] << 8);
            uint16_t *line = fb + (size_t)py * fb_w;
            for (int col = 0; col < advance; col++) {
                int px = x + col;
                if (px < 0 || px >= fb_w) continue;
                bool on = (col < gw) && ((bits >> col) & 1);
                if (on)         line[px] = fg;
                else if (has_bg) line[px] = bg;
            }
        }
        x += advance;
    }
    return x;
}

// ====================================================================
// text18 -- 18-px proportional font, identical approach to text12.
//
// Glyph layout (see font18.h): 24 rows per glyph, each row is three
// bytes (low byte first). Bit 0 of byte 0 is the leftmost pixel.
// Per-glyph widths in font18_widths[] give the meaningful column
// count. Same DejaVu Sans family as font12, rendered ~1.5x larger.
// font18_data is `static const` -> it lives in flash, not RAM.

#ifndef FONT18_TRACKING
#define FONT18_TRACKING 1   // pixels of space between glyphs
#endif

int text18_width(const char *s) {
    int w = 0;
    while (*s) {
        uint8_t c = (uint8_t)*s++;
        if (c < FONT18_FIRST_CHAR || c > FONT18_LAST_CHAR) c = '?';
        w += font18_widths[c - FONT18_FIRST_CHAR] + FONT18_TRACKING;
    }
    if (w > 0) w -= FONT18_TRACKING;  // no trailing tracking
    return w;
}

// One glyph -- opaque path. Builds an (advance x H) RGB565 buffer
// and DMAs the whole block.
static void draw_glyph18_opaque(uint8_t c, int x, int y,
                                int advance,
                                uint16_t fg, uint16_t bg) {
    if (c < FONT18_FIRST_CHAR || c > FONT18_LAST_CHAR) c = '?';
    const uint8_t *g = &font18_data[(c - FONT18_FIRST_CHAR)
                                    * FONT18_BYTES_PER_GLYPH];
    int gw = font18_widths[c - FONT18_FIRST_CHAR];

    // Off-screen?
    if (x + advance <= 0 || x >= ST7789_WIDTH ||
        y + FONT18_HEIGHT <= 0 || y >= ST7789_HEIGHT) {
        return;
    }

    // On-screen fast path: render glyph + tracking column into a
    // small RAM buffer then ship it as one blit.
    if (x >= 0 && y >= 0 &&
        x + advance <= ST7789_WIDTH &&
        y + FONT18_HEIGHT <= ST7789_HEIGHT) {
        uint16_t pix[FONT18_HEIGHT * (FONT18_MAX_WIDTH + FONT18_TRACKING)];
        int idx = 0;
        for (int row = 0; row < FONT18_HEIGHT; ++row) {
            uint32_t bits = g[row * FONT18_ROW_BYTES]
                          | ((uint32_t)g[row * FONT18_ROW_BYTES + 1] << 8)
                          | ((uint32_t)g[row * FONT18_ROW_BYTES + 2] << 16);
            for (int col = 0; col < advance; ++col) {
                bool on = (col < gw) && (bits >> col) & 1;
                pix[idx++] = on ? fg : bg;
            }
        }
        st7789_set_window(x, y, x + advance - 1, y + FONT18_HEIGHT - 1);
        st7789_blit_pixels(pix, (uint32_t)idx);
        return;
    }

    // Clipped path: per-pixel.
    for (int row = 0; row < FONT18_HEIGHT; ++row) {
        uint32_t bits = g[row * FONT18_ROW_BYTES]
                      | ((uint32_t)g[row * FONT18_ROW_BYTES + 1] << 8)
                      | ((uint32_t)g[row * FONT18_ROW_BYTES + 2] << 16);
        int py = y + row;
        if (py < 0 || py >= ST7789_HEIGHT) continue;
        for (int col = 0; col < advance; ++col) {
            int px = x + col;
            if (px < 0 || px >= ST7789_WIDTH) continue;
            bool on = (col < gw) && (bits >> col) & 1;
            st7789_pixel(px, py, on ? fg : bg);
        }
    }
}

// Transparent path -- only set foreground pixels.
static void draw_glyph18_transparent(uint8_t c, int x, int y, uint16_t fg) {
    if (c < FONT18_FIRST_CHAR || c > FONT18_LAST_CHAR) c = '?';
    const uint8_t *g = &font18_data[(c - FONT18_FIRST_CHAR)
                                    * FONT18_BYTES_PER_GLYPH];
    int gw = font18_widths[c - FONT18_FIRST_CHAR];

    if (x + gw <= 0 || x >= ST7789_WIDTH ||
        y + FONT18_HEIGHT <= 0 || y >= ST7789_HEIGHT) {
        return;
    }

    for (int row = 0; row < FONT18_HEIGHT; ++row) {
        uint32_t bits = g[row * FONT18_ROW_BYTES]
                      | ((uint32_t)g[row * FONT18_ROW_BYTES + 1] << 8)
                      | ((uint32_t)g[row * FONT18_ROW_BYTES + 2] << 16);
        if (bits == 0) continue;
        int py = y + row;
        if (py < 0 || py >= ST7789_HEIGHT) continue;
        for (int col = 0; col < gw; ++col) {
            if (!((bits >> col) & 1)) continue;
            int px = x + col;
            if (px < 0 || px >= ST7789_WIDTH) continue;
            st7789_pixel(px, py, fg);
        }
    }
}

int draw_text18(const char *s, int x, int y, uint16_t fg,
                uint16_t bg, bool transparent) {
    while (*s) {
        uint8_t c = (uint8_t)*s++;
        if (c < FONT18_FIRST_CHAR || c > FONT18_LAST_CHAR) c = '?';
        int gw = font18_widths[c - FONT18_FIRST_CHAR];
        int advance = gw + FONT18_TRACKING;

        if (transparent) {
            draw_glyph18_transparent(c, x, y, fg);
        } else {
            draw_glyph18_opaque(c, x, y, advance, fg, bg);
        }
        x += advance;
    }
    return x;
}

// Render text18 into an RGB565 framebuffer (u16 per pixel, native LE).
// `bg` is the opaque background colour; if has_bg is false, only foreground
// pixels are written and the background is left untouched (transparent).
int draw_text18_fb(uint16_t *fb, int fb_w, int fb_h,
                   const char *s, int x, int y,
                   uint16_t fg, bool has_bg, uint16_t bg) {
    while (*s) {
        uint8_t c = (uint8_t)*s++;
        if (c < FONT18_FIRST_CHAR || c > FONT18_LAST_CHAR) c = '?';
        const uint8_t *g = &font18_data[(c - FONT18_FIRST_CHAR)
                                        * FONT18_BYTES_PER_GLYPH];
        int gw = font18_widths[c - FONT18_FIRST_CHAR];
        int advance = gw + FONT18_TRACKING;

        // Skip glyphs entirely off the buffer.
        if (x + advance <= 0 || x >= fb_w ||
            y + FONT18_HEIGHT <= 0 || y >= fb_h) {
            x += advance;
            continue;
        }

        for (int row = 0; row < FONT18_HEIGHT; row++) {
            int py = y + row;
            if (py < 0 || py >= fb_h) continue;
            uint32_t bits = g[row * FONT18_ROW_BYTES]
                          | ((uint32_t)g[row * FONT18_ROW_BYTES + 1] << 8)
                          | ((uint32_t)g[row * FONT18_ROW_BYTES + 2] << 16);
            uint16_t *line = fb + (size_t)py * fb_w;
            for (int col = 0; col < advance; col++) {
                int px = x + col;
                if (px < 0 || px >= fb_w) continue;
                bool on = (col < gw) && ((bits >> col) & 1);
                if (on)          line[px] = fg;
                else if (has_bg) line[px] = bg;
            }
        }
        x += advance;
    }
    return x;
}

// Glyph layout (see font24.h): 32 rows per glyph, each row is three
// bytes (low byte first). Bit 0 of byte 0 is the leftmost pixel.
// Per-glyph widths in font24_widths[] give the meaningful column
// count. Same DejaVu Sans family as font12, rendered 2x larger.
// font24_data is `static const` -> it lives in flash, not RAM.

#ifndef FONT24_TRACKING
#define FONT24_TRACKING 1   // pixels of space between glyphs
#endif

int text24_width(const char *s) {
    int w = 0;
    while (*s) {
        uint8_t c = (uint8_t)*s++;
        if (c < FONT24_FIRST_CHAR || c > FONT24_LAST_CHAR) c = '?';
        w += font24_widths[c - FONT24_FIRST_CHAR] + FONT24_TRACKING;
    }
    if (w > 0) w -= FONT24_TRACKING;  // no trailing tracking
    return w;
}

// One glyph -- opaque path. Builds an (advance x H) RGB565 buffer
// and DMAs the whole block.
static void draw_glyph24_opaque(uint8_t c, int x, int y,
                                int advance,
                                uint16_t fg, uint16_t bg) {
    if (c < FONT24_FIRST_CHAR || c > FONT24_LAST_CHAR) c = '?';
    const uint8_t *g = &font24_data[(c - FONT24_FIRST_CHAR)
                                    * FONT24_BYTES_PER_GLYPH];
    int gw = font24_widths[c - FONT24_FIRST_CHAR];

    // Off-screen?
    if (x + advance <= 0 || x >= ST7789_WIDTH ||
        y + FONT24_HEIGHT <= 0 || y >= ST7789_HEIGHT) {
        return;
    }

    // On-screen fast path: render glyph + tracking column into a
    // small RAM buffer then ship it as one blit.
    if (x >= 0 && y >= 0 &&
        x + advance <= ST7789_WIDTH &&
        y + FONT24_HEIGHT <= ST7789_HEIGHT) {
        uint16_t pix[FONT24_HEIGHT * (FONT24_MAX_WIDTH + FONT24_TRACKING)];
        int idx = 0;
        for (int row = 0; row < FONT24_HEIGHT; ++row) {
            uint32_t bits = g[row * FONT24_ROW_BYTES]
                          | ((uint32_t)g[row * FONT24_ROW_BYTES + 1] << 8)
                          | ((uint32_t)g[row * FONT24_ROW_BYTES + 2] << 16);
            for (int col = 0; col < advance; ++col) {
                bool on = (col < gw) && (bits >> col) & 1;
                pix[idx++] = on ? fg : bg;
            }
        }
        st7789_set_window(x, y, x + advance - 1, y + FONT24_HEIGHT - 1);
        st7789_blit_pixels(pix, (uint32_t)idx);
        return;
    }

    // Clipped path: per-pixel.
    for (int row = 0; row < FONT24_HEIGHT; ++row) {
        uint32_t bits = g[row * FONT24_ROW_BYTES]
                      | ((uint32_t)g[row * FONT24_ROW_BYTES + 1] << 8)
                      | ((uint32_t)g[row * FONT24_ROW_BYTES + 2] << 16);
        int py = y + row;
        if (py < 0 || py >= ST7789_HEIGHT) continue;
        for (int col = 0; col < advance; ++col) {
            int px = x + col;
            if (px < 0 || px >= ST7789_WIDTH) continue;
            bool on = (col < gw) && (bits >> col) & 1;
            st7789_pixel(px, py, on ? fg : bg);
        }
    }
}

// Transparent path -- only set foreground pixels.
static void draw_glyph24_transparent(uint8_t c, int x, int y, uint16_t fg) {
    if (c < FONT24_FIRST_CHAR || c > FONT24_LAST_CHAR) c = '?';
    const uint8_t *g = &font24_data[(c - FONT24_FIRST_CHAR)
                                    * FONT24_BYTES_PER_GLYPH];
    int gw = font24_widths[c - FONT24_FIRST_CHAR];

    if (x + gw <= 0 || x >= ST7789_WIDTH ||
        y + FONT24_HEIGHT <= 0 || y >= ST7789_HEIGHT) {
        return;
    }

    for (int row = 0; row < FONT24_HEIGHT; ++row) {
        uint32_t bits = g[row * FONT24_ROW_BYTES]
                      | ((uint32_t)g[row * FONT24_ROW_BYTES + 1] << 8)
                      | ((uint32_t)g[row * FONT24_ROW_BYTES + 2] << 16);
        if (bits == 0) continue;
        int py = y + row;
        if (py < 0 || py >= ST7789_HEIGHT) continue;
        for (int col = 0; col < gw; ++col) {
            if (!((bits >> col) & 1)) continue;
            int px = x + col;
            if (px < 0 || px >= ST7789_WIDTH) continue;
            st7789_pixel(px, py, fg);
        }
    }
}

int draw_text24(const char *s, int x, int y, uint16_t fg,
                uint16_t bg, bool transparent) {
    while (*s) {
        uint8_t c = (uint8_t)*s++;
        if (c < FONT24_FIRST_CHAR || c > FONT24_LAST_CHAR) c = '?';
        int gw = font24_widths[c - FONT24_FIRST_CHAR];
        int advance = gw + FONT24_TRACKING;

        if (transparent) {
            draw_glyph24_transparent(c, x, y, fg);
        } else {
            draw_glyph24_opaque(c, x, y, advance, fg, bg);
        }
        x += advance;
    }
    return x;
}

// Render text24 into an RGB565 framebuffer (u16 per pixel, native LE).
// `bg` is the opaque background colour; if has_bg is false, only foreground
// pixels are written and the background is left untouched (transparent).
int draw_text24_fb(uint16_t *fb, int fb_w, int fb_h,
                   const char *s, int x, int y,
                   uint16_t fg, bool has_bg, uint16_t bg) {
    while (*s) {
        uint8_t c = (uint8_t)*s++;
        if (c < FONT24_FIRST_CHAR || c > FONT24_LAST_CHAR) c = '?';
        const uint8_t *g = &font24_data[(c - FONT24_FIRST_CHAR)
                                        * FONT24_BYTES_PER_GLYPH];
        int gw = font24_widths[c - FONT24_FIRST_CHAR];
        int advance = gw + FONT24_TRACKING;

        // Skip glyphs entirely off the buffer.
        if (x + advance <= 0 || x >= fb_w ||
            y + FONT24_HEIGHT <= 0 || y >= fb_h) {
            x += advance;
            continue;
        }

        for (int row = 0; row < FONT24_HEIGHT; row++) {
            int py = y + row;
            if (py < 0 || py >= fb_h) continue;
            uint32_t bits = g[row * FONT24_ROW_BYTES]
                          | ((uint32_t)g[row * FONT24_ROW_BYTES + 1] << 8)
                          | ((uint32_t)g[row * FONT24_ROW_BYTES + 2] << 16);
            uint16_t *line = fb + (size_t)py * fb_w;
            for (int col = 0; col < advance; col++) {
                int px = x + col;
                if (px < 0 || px >= fb_w) continue;
                bool on = (col < gw) && ((bits >> col) & 1);
                if (on)          line[px] = fg;
                else if (has_bg) line[px] = bg;
            }
        }
        x += advance;
    }
    return x;
}
