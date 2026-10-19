/* gfx.h -- drawing into a full-screen frame buffer for the launcher.
 *
 * Two buffers of 240 x 280 panel words (BGR565, see color565.h): one is
 * drawn into while DMA sends the other to the display. Colours are given as
 * 0xRRGGBB, alpha as 0..255. Everything clips to the clip rectangle.
 */
#ifndef PBC_GFX_H
#define PBC_GFX_H

#include <stdint.h>
#include <stdbool.h>
#include "assets.h"

#define GFX_W 240
#define GFX_H 280

void gfx_init(void);
/* Sends the finished frame and switches to the other buffer. */
void gfx_present(void);
/* Waits until the last frame has left, e.g. before handing over the panel. */
void gfx_finish(void);

void gfx_clip(int x0, int y0, int x1, int y1);     /* x1/y1 exclusive */
void gfx_noclip(void);

/* Vertical gradient over the whole screen (the common background). */
void gfx_background(uint32_t top, uint32_t bottom);
void gfx_rect(int x, int y, int w, int h, uint32_t rgb, int alpha);
void gfx_rrect(int x, int y, int w, int h, int r, uint32_t rgb, int alpha);
void gfx_circle(int cx, int cy, int r, uint32_t rgb, int alpha);
/* Soft elliptic glow, strongest in the middle. */
void gfx_glow(int cx, int cy, int rx, int ry, uint32_t rgb, int alpha);
/* Filled triangle pointing left (dir < 0) or right, as a navigation arrow. */
void gfx_chevron(int cx, int cy, int size, int dir, uint32_t rgb, int alpha);

/* Picture with its own alpha, scaled by `alpha`. */
void gfx_picture(const pbc_picture_t *p, int x, int y, int alpha);

/* Text: y is the top of the line (font->height high). Returns the width. */
int  gfx_text(const pbc_font_t *f, int x, int y, const char *s, uint32_t rgb, int alpha);
int  gfx_text_w(const pbc_font_t *f, const char *s);
void gfx_text_centered(const pbc_font_t *f, int cx, int y, const char *s, uint32_t rgb, int alpha);
/* Cuts `s` with "..." so it is at most max_w wide; out needs strlen(s) + 4. */
void gfx_text_fit(const pbc_font_t *f, const char *s, int max_w, char *out);

uint32_t gfx_mix(uint32_t a, uint32_t b, int t);   /* t 0..255 from a to b */

#endif /* PBC_GFX_H */
