// Drawing primitives for the PBC+ display in direct-write mode.
//
// All functions render directly to the ST7789 via st7789_* helpers.
// Algorithms match `framebuf` semantics so the same call writes the
// same pixels in either mode -- copy/paste between Display and
// framebuf.FrameBuffer code is safe.

#ifndef PBC_DISPLAY_DRAWING_H
#define PBC_DISPLAY_DRAWING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

void draw_line(int x0, int y0, int x1, int y1, uint16_t color);
void draw_rect_outline(int x, int y, int w, int h, uint16_t color);

// Ellipse, framebuf-compatible quadrant mask:
//   bit 0 = top-right    (NE)
//   bit 1 = top-left     (NW)
//   bit 2 = bottom-right (SE)
//   bit 3 = bottom-left  (SW)
// Default mask is 0x0F (all four).
void draw_ellipse(int cx, int cy, int xr, int yr, uint16_t color,
                  bool fill, uint8_t mask);

// Polygon. `coords` is `n_points` (x, y) int16 pairs, translated
// by (ox, oy) at draw time.
void draw_poly_outline(int ox, int oy, const int16_t *coords,
                       int n_points, uint16_t color);
void draw_poly_fill(int ox, int oy, const int16_t *coords,
                    int n_points, uint16_t color);

// 8x8 petme text (same glyphs as framebuf.text()).
void draw_text(const char *s, int x, int y, uint16_t fg,
               uint16_t bg, bool transparent);

// 12-px proportional sans-serif text. Per-glyph widths come from a
// table in font12.h; advance is glyph_width + 1 px tracking.
// Returns the x position one pixel past the last drawn glyph.
int draw_text12(const char *s, int x, int y, uint16_t fg,
                uint16_t bg, bool transparent);

// Same, but renders into an RGB565 framebuffer (u16 native LE per pixel)
// instead of via SPI. Use this when you have a canvas you blit later.
int draw_text12_fb(uint16_t *fb, int fb_w, int fb_h,
                   const char *s, int x, int y,
                   uint16_t fg, bool has_bg, uint16_t bg);

// Pixel width of a string when rendered with the 12-px font, useful
// for centering text. Includes the inter-glyph tracking.
int text12_width(const char *s);

// 18-px proportional sans-serif text -- same DejaVu Sans family as
// text12, rendered ~1.5x larger. Per-glyph widths come from font18.h.
// Returns the x position one pixel past the last drawn glyph.
int draw_text18(const char *s, int x, int y, uint16_t fg,
                uint16_t bg, bool transparent);

// Same, but renders into an RGB565 framebuffer (u16 native LE per pixel)
// instead of via SPI. Use this when you have a canvas you blit later.
int draw_text18_fb(uint16_t *fb, int fb_w, int fb_h,
                   const char *s, int x, int y,
                   uint16_t fg, bool has_bg, uint16_t bg);

// Pixel width of a string when rendered with the 18-px font.
int text18_width(const char *s);

// 24-px proportional sans-serif text -- same DejaVu Sans family as
// text12, rendered 2x larger. Per-glyph widths come from font24.h.
// Returns the x position one pixel past the last drawn glyph.
int draw_text24(const char *s, int x, int y, uint16_t fg,
                uint16_t bg, bool transparent);

// Same, but renders into an RGB565 framebuffer (u16 native LE per pixel)
// instead of via SPI. Use this when you have a canvas you blit later.
int draw_text24_fb(uint16_t *fb, int fb_w, int fb_h,
                   const char *s, int x, int y,
                   uint16_t fg, bool has_bg, uint16_t bg);

// Pixel width of a string when rendered with the 24-px font.
int text24_width(const char *s);

#endif // PBC_DISPLAY_DRAWING_H
