/* segfont.h -- Sechzehn-Segment-Schrift, wie sie eine Taschenrechner- oder
 * Digitaluhranzeige zeichnet.
 *
 * Die Vorlage rendert ihren Text als Strichsegmente (letterrender.c), nicht
 * als Bitmap -- daher der Anzeigen-Look.  Hier dasselbe Prinzip: je Zeichen
 * eine Maske aus 16 Segmenten plus zwei Punkten, gezeichnet in beliebiger
 * Groesse.  Waagerechte und senkrechte Balken haben harte Kanten (so sieht
 * eine Anzeige aus), die Schraegen werden kantengeglaettet, sonst franst z.B.
 * das X aus.
 */
#ifndef NOIZ_SEGFONT_H
#define NOIZ_SEGFONT_H

#include <stdint.h>

typedef struct {
    uint8_t w, h;                          /* Zeichenkasten in Bildpunkten  */
    uint8_t t;                             /* Strichstaerke                 */
    uint8_t adv;                           /* Vorschub inkl. Abstand        */
} segfont_t;

extern const segfont_t segfont_s;          /* klein, Beschriftungen         */
extern const segfont_t segfont_m;          /* Zahlenwerte                   */
extern const segfont_t segfont_l;          /* Meldungen                     */
extern const segfont_t segfont_xl;         /* Titel                         */

int  segfont_text_w(const segfont_t *f, const char *s);
/* Zeichnet die Bildzeile py einer Textzeile, deren Kasten bei y beginnt. */
void segfont_row(uint16_t *row, int row_w, int py, int x, int y,
                 const segfont_t *f, const char *s, uint16_t col);

#endif /* NOIZ_SEGFONT_H */
