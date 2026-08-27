/* color565.h -- die EINZIGE Stelle, an der ueber die Bitfolge zum Panel
 * entschieden wird.
 *
 * MADCTL 0xC8 setzt das BGR-Bit (CLAUDE.md Abschnitt 3): der ST7789 liest
 * dann Blau aus den oberen 5 Bit und Rot aus den unteren -- also BGR565, nicht
 * RGB565.  Falsch herum sieht nicht offensichtlich falsch aus: Grau, Schwarz
 * und Weiss stimmen weiter, nur Gelb wird zu Cyan und Rot zu Blau.
 *
 * Wer hier etwas aendert, aendert es fuer Spiel, Testfirmware und
 * Fehlerbildschirm gleichzeitig.  Rohe 0xF800-Konstanten gehoeren nirgends
 * sonst in den Quelltext.
 */
#ifndef PBC_COLOR565_H
#define PBC_COLOR565_H

#include <stdint.h>

#define PBC_RGB(r, g, b) ((uint16_t)((((unsigned)(b) & 0xf8u) << 8) | \
                                     (((unsigned)(g) & 0xfcu) << 3) | \
                                     (((unsigned)(r) & 0xf8u) >> 3)))

/* 0xRRGGBB -> Panelwort */
static inline uint16_t pbc_rgb24(uint32_t rgb)
{
    return PBC_RGB((rgb >> 16) & 0xffu, (rgb >> 8) & 0xffu, rgb & 0xffu);
}

/* Rueckrichtung, damit der Host-Test echte Farben ausgeben kann */
static inline uint32_t pbc_rgb24_of(uint16_t p)
{
    unsigned b = (p >> 11) & 0x1fu, g = (p >> 5) & 0x3fu, r = p & 0x1fu;
    return ((r * 255u / 31u) << 16) | ((g * 255u / 63u) << 8) | (b * 255u / 31u);
}

/* Mischt src mit Deckung a (0..255) auf dst.  Die Kanalreihenfolge ist dabei
 * egal, es zaehlen nur die Feldbreiten 5/6/5. */
static inline uint16_t pbc_blend565(uint16_t d, uint16_t s, unsigned a)
{
    unsigned a8, ia, c0, c1, c2;
    if (a >= 250u)
        return s;
    a8 = a + (a >> 7);                     /* 0..256                        */
    ia = 256u - a8;
    c0 = (((s >> 11) & 31u) * a8 + ((d >> 11) & 31u) * ia) >> 8;
    c1 = (((s >> 5) & 63u) * a8 + ((d >> 5) & 63u) * ia) >> 8;
    c2 = ((s & 31u) * a8 + (d & 31u) * ia) >> 8;
    return (uint16_t)((c0 << 11) | (c1 << 5) | c2);
}

/* Grundfarben, ueberall benutzen statt eigener Literale */
#define PBC_BLACK   PBC_RGB(0, 0, 0)
#define PBC_WHITE   PBC_RGB(255, 255, 255)
#define PBC_RED     PBC_RGB(255, 0, 0)
#define PBC_GREEN   PBC_RGB(0, 255, 0)
#define PBC_BLUE    PBC_RGB(0, 0, 255)
#define PBC_YELLOW  PBC_RGB(255, 255, 0)
#define PBC_CYAN    PBC_RGB(0, 255, 255)
#define PBC_MAGENTA PBC_RGB(255, 0, 255)
#define PBC_GREY    PBC_RGB(128, 128, 128)

#endif /* PBC_COLOR565_H */
