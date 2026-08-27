/* palette.h -- Farbindex -> RGB.
 *
 * Wie in der Vorlage traegt der Index Gruppe*16 + Stufe.  Entscheidend: die
 * Vorlage loescht den Schirm mit Index 0, und Index 0 ihrer Palette ist
 * WEISS.  Noiz2sa spielt also auf weissem Grund, alles Bewegte ist dunkle
 * Tinte darauf.
 *
 * Stufe 0 ist deshalb hier reines Weiss (= Hintergrund), und mit steigender
 * Stufe wird die Farbe kraeftiger.  Das passt zugleich zum Nachglüh-Schritt,
 * der je Bild eine Stufe abzieht: Spuren verblassen nach Weiss, nicht nach
 * Schwarz.
 */
#ifndef NOIZ_PALETTE_H
#define NOIZ_PALETTE_H

#include <stdint.h>
#include "color565.h"

/* Volltonwerte je Gruppe -- als Tinte auf Weiss gewaehlt, nicht als Leuchte */
static const uint32_t noiz_group_rgb[16] = {
    0x8A8A96u,  /* grau     */ 0xD01818u,  /* rot      */ 0x0E9028u,  /* gruen   */
    0x2030C8u,  /* blau     */ 0xB09000u,  /* gelb     */ 0xC020A0u,  /* magenta */
    0x0090A8u,  /* cyan     */ 0xE06000u,  /* orange   */ 0x5FA000u,  /* limone  */
    0x0062D8u,  /* azur     */ 0x6620C8u,  /* violett  */ 0xD02060u,  /* rosé    */
    0x008068u,  /* tuerkis  */ 0xC08000u,  /* bernstein*/ 0x18A070u,  /* mint    */
    0x101018u   /* tinte    */
};

/* Mischt linear von Weiss zur Vollfarbe.  Linear ist hier richtig: die Vorlage
 * zeichnet die Hintergrundbretter mit Deckungen um 12 bis 25 Prozent, und eine
 * quadratische Kurve druesst genau diese Stufen unsichtbar ins Weiss. */
static inline void noiz_build_palette(uint16_t out[256])
{
    int g, l;
    for (g = 0; g < 16; g++) {
        uint32_t rgb = noiz_group_rgb[g];
        for (l = 0; l < 16; l++) {
            unsigned f = (unsigned)(l * 255 / 15);        /* 0..255 Deckung */
            unsigned r = 255 - (255 - ((rgb >> 16) & 0xff)) * f / 255;
            unsigned gg = 255 - (255 - ((rgb >> 8) & 0xff)) * f / 255;
            unsigned b = 255 - (255 - (rgb & 0xff)) * f / 255;
            out[g * 16 + l] = PBC_RGB(r, gg, b);
        }
    }
}

#endif /* NOIZ_PALETTE_H */
