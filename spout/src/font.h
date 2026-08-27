/* font.h -- proportionale Bitmapschriften, in Zielgroesse gerastert.
 *
 * Es wird nichts hochskaliert: jede Groesse ist einzeln aus der TTF gerastert
 * (tools/mkfont.py).  Jede Tabelle traegt 8 Bit Deckung je Pixel; gemischt
 * wird zur Laufzeit gegen den vorhandenen Hintergrund, der Text funktioniert
 * also auch ueber dem Spielfeld.
 */
#ifndef SPOUT_FONT_H
#define SPOUT_FONT_H

#include <stdint.h>

typedef struct {
    uint8_t  w, h, adv;
    int8_t   ox, oy;                       /* Versatz zum Zeichenursprung   */
    uint16_t off;                          /* Index in die Pixeldaten       */
} glyph_t;

typedef struct {
    uint8_t first, last;
    uint8_t line_h, ascent;
    const glyph_t *g;
    const uint8_t *bits;
} font_t;

extern const font_t font_ui_s;             /* 12 px, Beschriftungen         */
extern const font_t font_ui_m;             /* 20 px fett, Zahlenwerte       */
extern const font_t font_ui_l;             /* 27 px fett, Meldungen         */
extern const font_t font_ui_xl;            /* 48 px schmal fett, nur O..U   */

const glyph_t *font_glyph(const font_t *f, unsigned ch);
int  font_text_w(const font_t *f, const char *s);

/* Zeichnet die Bildzeile py einer Textzeile mit Grundlinie `base` nach row.
 * Deckungswerte werden gegen den vorhandenen Inhalt gemischt. */
void font_row(uint16_t *row, int row_w, int py, int x, int base,
              const font_t *f, const char *s, uint16_t col);

#endif /* SPOUT_FONT_H */
