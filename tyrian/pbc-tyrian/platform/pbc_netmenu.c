/*
 * pbc_netmenu -- Auswahl der Rolle für ein Netzspiel.
 *
 * OpenTyrian wählt Gastgeber oder Beitretender über die Kommandozeile
 * (`--net`). Auf einem Handheld gibt es die nicht, also wird gefragt — und
 * zwar erst dann, wenn im Spielmodus-Menü tatsächlich "Network Game" gewählt
 * wurde. (Eine frühere Fassung fragte gleich beim Einschalten; das ist die
 * falsche Reihenfolge, weil die allermeisten Starts Einzelspieler sind.)
 *
 * Gezeichnet wird mit Tyrians eigenen Mitteln auf VGAScreen, damit sich der
 * Bildschirm nicht vom Rest des Menüs unterscheidet.
 *
 * GPLv2, wie OpenTyrian.
 */

#include "opentyr.h"

#ifdef WITH_NETWORK

#include "fonthand.h"
#include "sprite.h"
#include "keyboard.h"
#include "network.h"
#include "nortsong.h"
#include "palette.h"
#include "picload.h"
#include "sndmast.h"
#include "video.h"

#include <string.h>

#include "pbc_link.h"
#include "pbc_netmenu.h"
#include "pbc_video.h"

#define ITEM_COUNT 2

static const char *const item_text[ITEM_COUNT] = {
	"Host Game",
	"Join Game",
};

static const char *const item_hint[ITEM_COUNT] = {
	"This device sets the pace.",
	"Connect to a waiting host.",
};

int pbc_choose_network_role(void)
{
	/* Das ist ein Menü: ganzes Bild verkleinert statt Spielfeldausschnitt. */
	pbc_set_view_mode(PBC_VIEW_MENU);

	JE_loadPic(VGAScreen, 2, false);
	memcpy(VGAScreen2->pixels, VGAScreen->pixels,
	       (size_t)VGAScreen2->pitch * VGAScreen2->h);

	int selected = 0;
	bool first = true;

	for (;;)
	{
		setFrameCount(1);

		memcpy(VGAScreen->pixels, VGAScreen2->pixels,
		       (size_t)VGAScreen->pitch * VGAScreen->h);

		JE_dString(VGAScreen, JE_fontCenter("Two Player Link", SMALL_FONT_SHAPES),
		           20, "Two Player Link", SMALL_FONT_SHAPES);

		for (int i = 0; i < ITEM_COUNT; ++i)
		{
			const int y = 70 + i * 30;
			JE_outTextAdjust(VGAScreen,
			                 JE_fontCenter(item_text[i], SMALL_FONT_SHAPES), y,
			                 item_text[i], 15, i == selected ? 0 : -4,
			                 SMALL_FONT_SHAPES, true);
		}

		JE_outTextAdjust(VGAScreen,
		                 JE_fontCenter(item_hint[selected], TINY_FONT), 140,
		                 item_hint[selected], 15, -3, TINY_FONT, true);

		/*
		 * Der Hinweis auf das Kabel steht hier bewusst mit im Bild: ohne
		 * durchverbundene D+/D--Leitungen passiert gar nichts, und sehr viele
		 * USB-C-Kabel sind reine Ladekabel. Das ist der wahrscheinlichste
		 * Grund, warum eine Verbindung nicht zustande kommt.
		 */
		JE_outTextAdjust(VGAScreen,
		                 JE_fontCenter("Connect both devices with a USB-C data cable.", TINY_FONT),
		                 160, "Connect both devices with a USB-C data cable.",
		                 15, -4, TINY_FONT, true);

		JE_showVGA();

		if (first)
		{
			fade_palette(colors, 10, 0, 255);
			first = false;
		}

		waitUntilElapsed();
		waitUntilHasInput(INPUT_ANY);

		KeyboardInput input;
		if (!keyboardGetInput(&input))
			continue;

		switch (input.scancode)
		{
			case SDL_SCANCODE_UP:
			case SDL_SCANCODE_DOWN:
				JE_playSampleNum(S_CURSOR);
				selected = (selected + 1) % ITEM_COUNT;
				break;

			case SDL_SCANCODE_RETURN:
			case SDL_SCANCODE_SPACE:
				JE_playSampleNum(S_SELECT);
				fade_black(10);
				return selected == 0 ? PBC_NET_HOST : PBC_NET_JOIN;

			case SDL_SCANCODE_ESCAPE:
				JE_playSampleNum(S_SPRING);
				fade_black(10);
				return PBC_NET_CANCEL;

			default:
				break;
		}
	}
}

bool pbc_setup_network(void)
{
	const int role = pbc_choose_network_role();
	if (role == PBC_NET_CANCEL)
		return false;

	/*
	 * Namen fest vergeben statt eingeben zu lassen: mit sieben Knöpfen ist
	 * eine Namenseingabe nicht zu bedienen, und kurze Namen halten zugleich
	 * das Verbindungspaket unter den 36 Byte, die über den Link passen.
	 */
	isNetworkGame = true;

	if (role == PBC_NET_HOST)
	{
		thisPlayerNum = 1;
		network_player_name = (char *)"P1";
		pbc_link_start(PBC_ROLE_HOST);
	}
	else
	{
		thisPlayerNum = 2;
		network_player_name = (char *)"P2";
		pbc_link_start(PBC_ROLE_JOIN);
	}

	if (network_init())
	{
		/* Kommt hier praktisch nicht vor -- der "Socket" ist eine Attrappe
		   über zwei Warteschlangen. Der Zweig bleibt trotzdem stehen. */
		pbc_link_stop();
		isNetworkGame = false;
		return false;
	}

	return true;
}

#endif /* WITH_NETWORK */
