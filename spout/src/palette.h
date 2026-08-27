/* palette.h -- Zellbyte -> Farbe.
 *
 * Das Original kennt vier Graustufen (Index = Zellbyte & 3).  Auf einem
 * Farbpanel geht mehr, ohne den Charakter zu aendern: der Trefferzaehler in
 * Bit 6..7 wird als Helligkeit sichtbar (angeschlagener Fels wird heller),
 * die Sperrmauer bekommt einen eigenen Ton, Koerner leuchten bernstein.
 * Damit sieht man beim Sprengen, wie weit man ist -- upstream ist das
 * unsichtbar, weil dort nur die unteren zwei Bit die Farbe bestimmen.
 */
#ifndef SPOUT_PALETTE_H
#define SPOUT_PALETTE_H

#include <stdint.h>
#include "game.h"
#include "color565.h"

/* 0xRRGGBB */
static inline uint32_t spout_cell_rgb(uint8_t cell, int dark)
{
    static const uint32_t grain_l[3] = { 0xFFC64Bu, 0xFF9E1Bu, 0xE05A00u }; /* wenig..viel Leben */
    static const uint32_t rock_l[3]  = { 0x9AA2AEu, 0x606874u, 0x39404Cu };
    static const uint32_t bar_l[3]   = { 0xC08088u, 0x9A4451u, 0x6B2733u };
    static const uint32_t solid_l[4] = { 0xEDEDE6u, 0x8A8F97u, 0x3A3F47u, 0x14161Au };

    static const uint32_t grain_d[3] = { 0x9A5A00u, 0xE08A10u, 0xFFC24Bu };
    static const uint32_t rock_d[3]  = { 0x2A3038u, 0x424A57u, 0x6E7886u };
    static const uint32_t bar_d[3]   = { 0x53202Au, 0x8A3B47u, 0xC2626Du };
    static const uint32_t solid_d[4] = { 0x0B0C10u, 0x3A3F47u, 0x9AA2AEu, 0xE8E8E0u };

    const uint32_t *grain = dark ? grain_d : grain_l;
    const uint32_t *rock  = dark ? rock_d  : rock_l;
    const uint32_t *bar   = dark ? bar_d   : bar_l;
    const uint32_t *solid = dark ? solid_d : solid_l;

    int life;

    if (cell == 0)
        return solid[0];

    life = ((cell >> 6) & 3);              /* 0 = unzerstoerbar, sonst Treffer */

    if (cell & CELL_GRAIN)
        return grain[life ? life - 1 : 2];

    if (life)                              /* zerstoerbar: Fels oder Sperrmauer */
        return ((cell & 3) == 3 ? bar : rock)[life - 1];

    return solid[cell & 3];                /* Rand, Boden, Titeltext */
}

/* Die Bitfolge steckt in color565.h -- hier nur die Weiterleitung. */
static inline uint16_t spout_rgb565(uint32_t rgb)
{
    return pbc_rgb24(rgb);
}

/* Farben fuer Overlay (Schiff, Duesenstrahl) und Anzeige */
#define SPOUT_UI_ACCENT_L 0x1478D2u
#define SPOUT_UI_ACCENT_D 0x4CB0FFu

static inline uint32_t spout_ovl_rgb(uint8_t c, int dark)
{
    if (c == 0x02)                         /* Duesenstrahl / Zielrichtung */
        return dark ? SPOUT_UI_ACCENT_D : SPOUT_UI_ACCENT_L;
    return spout_cell_rgb(c, dark);
}

#endif /* SPOUT_PALETTE_H */
