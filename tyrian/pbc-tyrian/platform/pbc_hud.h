/*
 * pbc_hud -- die Anzeigeleiste unter dem Spielfeld.
 *
 * Tyrians eigene Statusanzeige ist eine 56 Pixel breite Spalte rechts neben dem
 * Spielfeld, mit senkrechten Balken fuer Schild und Panzerung. Auf einem 240
 * Pixel breiten Panel ist neben dem 264 Pixel breiten Spielfeld dafuer kein
 * Platz -- wohl aber DARUNTER, wo das hochkant stehende Panel 96 Zeilen uebrig
 * laesst.
 *
 * Die Leiste wird deshalb neu aufgebaut, aber mit Tyrians eigenen Mitteln: sie
 * ist eine ganz normale 8-Bit-Zeichenflaeche in derselben Palette, auf der
 * dieselben Zeichenroutinen laufen (JE_outText, fill_rectangle_wh). Es kommen
 * also keine fremden Schriften oder Farben ins Bild.
 *
 * GPLv2, wie OpenTyrian.
 */
/*
 * Achtung beim Umbenennen: der Waechter heisst bewusst NICHT PBC_HUD_H --
 * so heisst in pbc_config.h bereits die Hoehe der Leiste. Waeren beide gleich,
 * wuerde diese Datei stillschweigend uebersprungen, sobald pbc_config.h zuerst
 * eingebunden ist, und der Fehler zeigte sich erst als "implizite Deklaration"
 * an ganz anderer Stelle.
 */
#ifndef PBC_HUD_HEADER_H
#define PBC_HUD_HEADER_H

#include <stdint.h>
#include <stdbool.h>

void pbc_hud_init(void);

/*
 * Zustand einlesen und, falls sich etwas geaendert hat, neu zeichnen. Einmal je
 * Bild aus JE_showVGA gerufen.
 */
void pbc_hud_update(void);

/* Muss die Leiste neu zum Panel geschoben werden? Solange sie unveraendert
   bleibt, spart das Weglassen rund ein Drittel der Bildausgabezeit. */
bool pbc_hud_dirty(void);
void pbc_hud_clear_dirty(void);

/* Zeile y (0..6) der Statuszeile direkt als RGB565 nach dst. */
void pbc_hud_render_row(uint16_t *dst, int y);

#endif /* PBC_HUD_HEADER_H */
