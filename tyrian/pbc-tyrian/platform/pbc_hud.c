/*
 * pbc_hud -- die Statuszeile unter dem Spielbild.
 *
 * Frueher war das eine ganze Anzeigeleiste mit Schild, Panzerung, Waffen und
 * Geld. Die ist entfallen: die Spielansicht zeigt inzwischen das ganze
 * 320x200-Bild verkleinert, und darin steht Tyrians eigene Statusspalte
 * bereits drin. Sie daneben noch einmal nachzubauen waere doppelt und wuerde
 * bei jeder Aenderung am Original auseinanderlaufen.
 *
 * Uebrig bleibt, was das Spiel selbst nicht anzeigen kann: der Zustand des
 * Multiplayer-Links. Der steht dauerhaft da und nicht nur im Fehlerfall --
 * waehrend eines Netzspiels gehoert der USB-Port dem Link, die serielle
 * Schnittstelle faellt als Diagnoseweg also aus, und ohne Messwerte ist bei
 * einem Abbruch jede Theorie gleich plausibel.
 *
 * Gezeichnet wird zeilenweise direkt in RGB565: eine eigene 8-Bit-Flaeche
 * dafuer waeren 23 KB, und Tyrians Zeichenroutinen brauchen genau so eine.
 * Fuer eine Zeile Text ist der 5x7-Satz aus pbc_display.c genug.
 *
 * GPLv2, wie OpenTyrian.
 */

#include "opentyr.h"

#include <stdio.h>
#include <string.h>

#include "pbc_config.h"
#include "pbc_display.h"
#include "pbc_hud.h"
#include "pbc_link.h"

#define C_BLACK    PBC_RGB(  0,   0,   0)
#define C_LINK_OK  PBC_RGB(  0, 255,   0)
#define C_LINK_BAD PBC_RGB(255,  80,  60)

#define TEXT_X     8
#define FONT_ADV   6   /* 5 Punkte Zeichen + 1 Punkt Abstand */

static int  shown_state = -1;
static uint32_t shown_counter = 0xFFFFFFFFu;
static char text[24];
static bool dirty = true;

void pbc_hud_init(void)
{
	shown_state = -1;
	shown_counter = 0xFFFFFFFFu;
	text[0] = '\0';
	dirty = true;
}

void pbc_hud_update(void)
{
	const int state = pbc_link_state();
	const uint32_t counter = pbc_link_counter();

	if (state == shown_state && counter == shown_counter)
		return;

	shown_state = state;
	shown_counter = counter;

	if (state == PBC_LINK_OFF)
		text[0] = '\0';
	else
		snprintf(text, sizeof text, "%s %lu",
		         pbc_link_state_name(state), (unsigned long)counter);

	dirty = true;
}

bool pbc_hud_dirty(void)       { return dirty; }
void pbc_hud_clear_dirty(void) { dirty = false; }

void pbc_hud_render_row(uint16_t *dst, int y)
{
	for (int i = 0; i < PBC_TFT_W; ++i)
		dst[i] = C_BLACK;

	if (y < 0 || y >= 7 || text[0] == '\0')
		return;

	const uint16_t color = (shown_state == PBC_LINK_UP) ? C_LINK_OK : C_LINK_BAD;

	for (int i = 0; text[i] != '\0'; ++i)
	{
		char ch = text[i];
		if (ch >= 'a' && ch <= 'z')
			ch = (char)(ch - 32);

		const int idx = (ch >= 32 && ch <= 90) ? ch - 32 : 0;
		const int cx = TEXT_X + i * FONT_ADV;
		if (cx >= PBC_TFT_W)
			return;

		for (int col = 0; col < 5; ++col)
		{
			if (!(pbc_font5x7[idx][col] & (1 << y)))
				continue;
			const int px = cx + col;
			if (px >= 0 && px < PBC_TFT_W)
				dst[px] = color;
		}
	}
}
