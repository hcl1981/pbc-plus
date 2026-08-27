/*
 * pbc_display -- ST7789-Ausgabe fuer den PicoBoy Color Plus.
 *
 * Anders als bei der Doom-Portierung, von der die Init-Folge stammt, gibt es
 * hier KEINEN vollstaendigen Bildpuffer im RAM. Das Bild entsteht streifenweise
 * und wird sofort weitergeschoben (siehe PBC_STRIP_ROWS in pbc_config.h).
 *
 * Der Grund ist Arithmetik: Tyrian braucht bereits drei 320x200-Puffer
 * (192 KB) fuer sich. Ein 240x280-RGB565-Bild waeren 134 KB obendrauf, und
 * damit waere von 512 KB nichts mehr uebrig. Streifenweise kostet dasselbe
 * Ergebnis 7,7 KB.
 *
 * Nebeneffekt, der sich als wichtiger herausstellt als die Speicherersparnis:
 * ein Vollbild belegt den SPI-Bus rund 17 ms am Stueck. Der Multiplayer-Link
 * vertraegt keine so langen Pausen. Zwischen zwei Streifen liegt dagegen alle
 * ~0,5 ms eine Gelegenheit, ihn zu bedienen.
 *
 * GPLv2, wie OpenTyrian.
 */
#ifndef PBC_DISPLAY_H
#define PBC_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>

#include "pbc_config.h"

void pbc_display_init(void);

/* 0..255. Beim Start voll aufgedreht. */
void pbc_display_backlight(uint8_t level);

/*
 * Einen Bildausschnitt beginnen. y0/rows beziehen sich auf das sichtbare
 * Panel (0..279), der Panelversatz wird intern zugeschlagen.
 */
void pbc_display_begin(int y0, int rows);

/*
 * Puffer fuer den naechsten Streifen. Zeigt immer auf den gerade NICHT vom DMA
 * gelesenen der beiden Puffer, ist also sofort beschreibbar. Platz fuer
 * PBC_TFT_W * PBC_STRIP_ROWS Pixel.
 */
uint16_t *pbc_display_strip_buffer(void);

/*
 * Streifen abschicken. Wartet auf den vorherigen Transfer und startet dann
 * diesen -- kehrt also zurueck, WAEHREND der Streifen noch laeuft. Genau in
 * dieser Zeit gehoert der naechste umgerechnet.
 */
void pbc_display_push_strip(uint16_t *buf, int npix);

/* Letzten Transfer abwarten und den Bus freigeben. */
void pbc_display_end(void);

/* Laeuft gerade ein Transfer? Fuer Code, der selbst Flanken zaehlen muss. */
bool pbc_display_busy(void);

/* ------------------------------------------------------------ Notausgabe */

/*
 * Meldung auf schwarzem Grund, blockierend, ohne Umweg ueber den Rest des
 * Systems. Dafuer da, dass ein fehlendes Datenarchiv oder ein Ladefehler etwas
 * Lesbares hinterlaesst statt eines schwarzen Bildschirms.
 */
void pbc_display_message(const char *line1, const char *line2, const char *line3);

/*
 * 5x7-Zeichensatz, ASCII 32..90 (Kleinbuchstaben werden gross gezeichnet).
 * Spaltenweise: Bit n von Spalte c ist der Bildpunkt (c, n).
 *
 * Wird ausser fuer die Notausgabe auch von der Anzeigeleiste benutzt -- die
 * zeichnet direkt in RGB565 und kann Tyrians eigene Schrift deshalb nicht
 * verwenden.
 */
extern const uint8_t pbc_font5x7[59][5];

/*
 * Farbtestbild: vier reine Balken (Rot, Gruen, Blau, Weiss) mit Beschriftung,
 * darunter ein Graukeil und ein Farbverlauf.
 *
 * Beantwortet ohne Umweg ueber das Spiel, ob die Farbreihenfolge des Panels
 * stimmt: steht "R" ueber einem blauen Balken, ist PBC_PANEL_BGR falsch
 * herum. Der Graukeil zeigt zusaetzlich, ob der Bildaufbau selbst sauber ist
 * -- Streifen oder Versatz darin waeren ein Fehler im Ausgabeweg, nicht in den
 * Farben.
 *
 * Beim Einschalten mit gehaltener Taste B erreichbar.
 */
void pbc_display_test_pattern(void);

#endif /* PBC_DISPLAY_H */
