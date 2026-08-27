/* render.h -- erzeugt Bildzeilen im Panelformat (RGB565), 240 px breit.
 *
 * Auf dem Geraet ruft die Hauptschleife das zeilenweise fuer 8-Zeilen-Streifen
 * auf (kein Vollbildpuffer, siehe CLAUDE.md Abschnitt 3), der Host-Test ruft
 * es fuer das ganze Bild.  Beide benutzen denselben Code.
 */
#ifndef SPOUT_RENDER_H
#define SPOUT_RENDER_H

#include <stdint.h>
#include "game.h"

#define ZOOM    2
#define SCR_W   (VIEW_W * ZOOM)            /* 240 */
#define PF_H    (VIEW_H * ZOOM)            /* 240 */
#define HUD_H   40
#define SCR_H   (PF_H + HUD_H)             /* 280 */

typedef struct {
    int dark;                              /* dunkles Farbschema            */
    int blink;                             /* Framezaehler fuer Blinken     */
    int show_debug;
    int ms_frame, ms_cpu, fps;             /* Anzeige im Debugmodus         */
    int mute;
} spout_render_cfg_t;

extern spout_render_cfg_t spout_render;

void spout_render_init(int dark);
/* Schreibt `rows` Bildzeilen ab y0 nach dst (rows * SCR_W Werte, RGB565). */
void spout_render_rows(uint16_t *dst, int y0, int rows);

#endif /* SPOUT_RENDER_H */
