/* font.c -- Textausgabe fuer Bildzeilen. */

#include "font.h"
#include "color565.h"

const glyph_t *font_glyph(const font_t *f, unsigned ch)
{
    if (ch >= 'a' && ch <= 'z')
        ch -= 32;                          /* die Tabellen fuehren nur Grossbuchstaben */
    if (ch < f->first || ch > f->last)
        ch = '?';
    return &f->g[ch - f->first];
}

int font_text_w(const font_t *f, const char *s)
{
    int w = 0;
    for (; *s; s++)
        w += font_glyph(f, (unsigned char)*s)->adv;
    return w;
}

void font_row(uint16_t *row, int row_w, int py, int x, int base,
              const font_t *f, const char *s, uint16_t col)
{
    for (; *s; s++) {
        const glyph_t *g = font_glyph(f, (unsigned char)*s);
        int top = base + g->oy;
        int gx;

        if (g->w && py >= top && py < top + g->h) {
            const uint8_t *p = f->bits + g->off + (unsigned)(py - top) * g->w;
            int x0 = x + g->ox;
            for (gx = 0; gx < g->w; gx++) {
                unsigned a = p[gx];
                int px = x0 + gx;
                if (a && px >= 0 && px < row_w)
                    row[px] = pbc_blend565(row[px], col, a);
            }
        }
        x += g->adv;
    }
}
