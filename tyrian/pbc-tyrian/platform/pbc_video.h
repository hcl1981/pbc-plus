/*
 * pbc_video.h -- was der Port ueber video.h hinaus braucht.
 *
 * GPLv2, wie OpenTyrian.
 */
#ifndef PBC_VIDEO_H
#define PBC_VIDEO_H

#include "pbc_config.h"

/* Bewusst ohne opentyr.h: diese Kopfdatei wird auch aus der Plattformschicht
   eingebunden, und opentyr.h definiert MIN/MAX neu -- die gibt es dort schon
   aus den SDK-Kopfdateien. Gebraucht wird von hier ohnehin nichts. */

/*
 * Zwei Ansichten, weil zwei Dinge gezeigt werden muessen, die nicht dieselbe
 * Behandlung vertragen:
 *
 *   PBC_VIEW_MENU  Ausschnitt in Originalgroesse, mittig: 240 der 320 Spalten.
 *                  Fuer Titelbild, Schiffauswahl, Episode und Schwierigkeit --
 *                  dort steht der Inhalt mittig, und Tyrians kleine Schrift
 *                  ueberlebt eine Verkleinerung nicht.
 *
 *   PBC_VIEW_SHOP  das ganze 320x200-Bild auf 240x150 verkleinert. Der
 *                  Ladenbildschirm spannt sich ueber die volle Breite --
 *                  rechts stehen die Schiffsdaten, die duerfen nicht
 *                  abgeschnitten werden.
 *
 *   PBC_VIEW_GAME  wie SHOP dargestellt, aber anders BEDIENT: im Spiel
 *                  bedeuten dieselben Knoepfe etwas anderes als in einem
 *                  Menue. Die Ansicht ist deshalb zugleich die Auskunft
 *                  darueber, welche Tastenbelegung gilt (siehe pbc_input.c).
 */
typedef enum
{
	PBC_VIEW_MENU = 0,
	PBC_VIEW_SHOP,
	PBC_VIEW_GAME
} pbc_view_mode_t;

/*
 * Linke Kante des sichtbaren Ausschnitts, in Tyrians 320er-Koordinaten.
 *
 * In der Menueansicht sind nur 240 der 320 Spalten zu sehen, mittig -- also
 * x = 40..279. Wer im Menue links ausrichtet statt zu zentrieren, muss sich
 * daran halten, sonst faellt der Anfang weg. Genau das ist beim Episodenschirm
 * passiert: er zeichnete bei x = 20, und aus "Episode 1" wurde "sode 1".
 *
 * Vier Spalten Luft, damit der Text nicht an der Kante klebt.
 */
#define PBC_MENU_SAFE_X (PBC_MENU_SRC_X + 4)

void pbc_set_view_mode(pbc_view_mode_t m);
pbc_view_mode_t pbc_get_view_mode(void);

#endif /* PBC_VIDEO_H */
