/*
 * pbc_video.c -- Ersatz fuer OpenTyrians video.c.
 *
 * Aufgaben:
 *   - die drei 320x200-Zeichenflaechen bereitstellen, in die Tyrian rendert
 *   - die wenigen SDL-Oberflaechenfunktionen liefern, die das Spiel benutzt
 *   - aus VGAScreen das Panelbild zusammensetzen und hinausschieben
 *
 * Zur Bildaufteilung siehe den langen Kommentar in pbc_config.h. Kurz: im Spiel
 * wird das Spielfeld in Originalpixeln gezeigt (240 der 264 Spalten, Kamera
 * folgt dem Schiff) und darunter eine eigens gebaute Anzeigeleiste; in Menues
 * und Zwischenbildern wird das ganze 320x200-Bild auf 240x150 verkleinert,
 * weil dort jede Ecke Inhalt traegt.
 *
 * GPLv2, wie OpenTyrian.
 */

#include "opentyr.h"
#include "video.h"
#include "palette.h"
#include "video_scale.h"

#include <string.h>

#include "pbc_config.h"
#include "pbc_display.h"
#include "pbc_video.h"
#include "pbc_hud.h"
#include "pbc_link.h"

/* ---------------------------------------------------------- Oberflaechen */

/*
 * Fest angelegt statt per malloc: die Groesse steht ohnehin fest, und so
 * taucht der Verbrauch im Linker-Bericht auf, statt sich zur Laufzeit als
 * fehlgeschlagene Anforderung zu zeigen. 3 x 64 KB = 192 KB von 512 KB.
 */
static Uint8 vga_pixels[vga_width * vga_height];
static Uint8 vga2_pixels[vga_width * vga_height];
static Uint8 game_pixels[vga_width * vga_height];

static SDL_PixelFormat fmt_8bit = { .BitsPerPixel = 8, .BytesPerPixel = 1 };

static SDL_Surface surf_vga   = { vga_pixels,  vga_width, vga_height, vga_width, &fmt_8bit };
static SDL_Surface surf_vga2  = { vga2_pixels, vga_width, vga_height, vga_width, &fmt_8bit };
static SDL_Surface surf_game  = { game_pixels, vga_width, vga_height, vga_width, &fmt_8bit };

SDL_Surface *VGAScreen = &surf_vga, *VGAScreenSeg = &surf_vga;
SDL_Surface *VGAScreen2 = &surf_vga2;
SDL_Surface *game_screen = &surf_game;

/* Es gibt genau ein Fenster und es ist das Panel. */
SDL_Window *main_window = NULL;
SDL_PixelFormat *main_window_tex_format = &fmt_8bit;

/* Von OpenTyrians Optionsmenue erwartet; hier ohne Wirkung. */
int fullscreen_display = 0;
ScalingMode scaling_mode = SCALE_CENTER;
const char *const scaling_mode_names[ScalingMode_MAX] = {
	"Center", "Integer", "8:5", "4:3"
};

/* ------------------------------------------------------ Ansichtsmodus */

static pbc_view_mode_t view_mode = PBC_VIEW_MENU;

/*
 * Beide Ansichten fuellen nicht das ganze Panel -- daneben bleibt es schwarz.
 * Geschoben werden deshalb im Regelfall nur die belegten Zeilen. Nach einem
 * Wechsel der Ansicht (oder wenn sich die Statuszeile aendert) muss der Rand
 * einmal mit, sonst bleiben Reste der vorherigen Ansicht stehen.
 */
static bool full_refresh = true;

void pbc_set_view_mode(pbc_view_mode_t m)
{
	if (m != view_mode)
		full_refresh = true;
	view_mode = m;
}

pbc_view_mode_t pbc_get_view_mode(void)
{
	return view_mode;
}

/* ------------------------------------------------- SDL-Oberflaechen-API */

Uint32 SDL_MapRGB(const SDL_PixelFormat *fmt, Uint8 r, Uint8 g, Uint8 b)
{
	(void)fmt;
	/*
	 * Fertiger Panelwert, nicht RGB888. palette.c legt das Ergebnis in
	 * rgb_palette ab, und genau diese Tabelle liest der Compositor unten --
	 * damit ist die Umrechnung einmal je Farbwechsel erledigt statt einmal je
	 * Bildpunkt.
	 *
	 * Reihenfolge BGR, siehe PBC_PANEL_BGR in pbc_config.h. Wer hier RGB
	 * einsetzt, bekommt kein falsch aussehendes Bild, sondern ein scheinbar
	 * zu dunkles: Tyrians Bilder sind rotlastig, und als Blau ausgegeben wirkt
	 * Rot erheblich dunkler.
	 */
#if PBC_PANEL_BGR
	return (Uint32)(((b & 0xF8) << 8) | ((g & 0xFC) << 3) | (r >> 3));
#else
	return (Uint32)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
#endif
}

void SDL_FillRect(SDL_Surface *dst, const SDL_Rect *rect, Uint32 color)
{
	if (dst == NULL)
		return;

	if (rect == NULL)
	{
		memset(dst->pixels, (int)color, (size_t)dst->pitch * dst->h);
		return;
	}

	int x0 = rect->x, y0 = rect->y;
	int x1 = x0 + rect->w, y1 = y0 + rect->h;

	if (x0 < 0) x0 = 0;
	if (y0 < 0) y0 = 0;
	if (x1 > dst->w) x1 = dst->w;
	if (y1 > dst->h) y1 = dst->h;
	if (x0 >= x1 || y0 >= y1)
		return;

	Uint8 *p = (Uint8 *)dst->pixels + (size_t)y0 * dst->pitch + x0;
	for (int y = y0; y < y1; ++y, p += dst->pitch)
		memset(p, (int)color, (size_t)(x1 - x0));
}

void SDL_BlitSurface(SDL_Surface *src, const SDL_Rect *srcrect,
                     SDL_Surface *dst, SDL_Rect *dstrect)
{
	/*
	 * Es gibt genau einen Aufrufer (game_menu.c) und der kopiert eine ganze
	 * Flaeche auf eine gleich grosse. Mehr zu koennen waere hier ungenutzter
	 * Code -- der Vollstaendigkeit halber wird der Fall trotzdem geprueft.
	 */
	if (src == NULL || dst == NULL || srcrect != NULL || dstrect != NULL)
		return;
	if (src->w != dst->w || src->h != dst->h)
		return;

	memcpy(dst->pixels, src->pixels, (size_t)src->pitch * src->h);
}

void JE_clr256(SDL_Surface *screen)
{
	SDL_FillRect(screen, NULL, 0);
}

/* -------------------------------------------------------- Compositor */

/* rgb_palette wird von palette.c ueber SDL_MapRGB mit RGB565 gefuellt. */
static inline uint16_t pal(unsigned int index)
{
	return (uint16_t)rgb_palette[index];
}

/*
 * Mittelwert zweier RGB565-Werte, ohne sie in die Einzelanteile zu zerlegen.
 *
 * (a+b)/2 waere falsch: die Summe truege Uebertraege ueber die Grenzen der
 * Farbanteile hinweg, Blau liefe also in Gruen. Die Maske loescht vorher das
 * unterste Bit jedes Anteils, danach ist das Schieben gefahrlos; der gemeinsame
 * Teil (a & b) bringt genau die Haelfte zurueck, die dabei verlorenging.
 */
static inline uint16_t avg565(uint16_t a, uint16_t b)
{
	return (uint16_t)((((a ^ b) & 0xF7DEu) >> 1) + (a & b));
}

/*
 * Spielansicht: das ganze 320x200-Bild auf 240x150, senkrecht mittig.
 *
 * Ab dem Ladenbildschirm spannt sich der Inhalt ueber die volle Breite --
 * links das Spielfeld, rechts Tyrians eigene Statusspalte. Ein Ausschnitt
 * wuerde die Spalte abschneiden, also wird verkleinert.
 *
 * 320 -> 240 und 200 -> 150 sind beide genau 3/4. Gemittelt wird ueber 2x2
 * (siehe PBC_SCALE_AVERAGE in pbc_config.h): Zielpunkt k bekommt den
 * Mittelwert aus Quelle k und k+1, waagerecht wie senkrecht. Dadurch geht
 * jeder der vier Quellpunkte ein und keiner doppelt -- Tyrians Wolken bleiben
 * weich statt fleckig zu werden.
 */
static void compose_rows_game(uint16_t *out, int y0, int rows)
{
	const Uint8 *const base = (const Uint8 *)VGAScreen->pixels;
	const int pitch = VGAScreen->pitch;

	for (int r = 0; r < rows; ++r)
	{
		int y = y0 + r;
		uint16_t *dst = out + (size_t)r * PBC_TFT_W;

		if (y >= PBC_GAME_Y && y < PBC_GAME_Y + PBC_GAME_H)
		{
			const int sy = ((y - PBC_GAME_Y) * 4) / 3;
			const Uint8 *s0 = base + (size_t)sy * pitch;

#if PBC_SCALE_AVERAGE
			/* Zweite Quellzeile; die letzte Bildzeile mittelt mit sich selbst. */
			const Uint8 *s1 = base + (size_t)(sy + 1 < vga_height ? sy + 1 : sy) * pitch;

			int sx = 0;
			for (int d = 0; d < PBC_GAME_W; d += 3)
			{
				for (int k = 0; k < 3; ++k)
				{
					const uint16_t a = pal(s0[sx + k]);
					const uint16_t b = pal(s0[sx + k + 1]);
					const uint16_t c = pal(s1[sx + k]);
					const uint16_t e = pal(s1[sx + k + 1]);
					dst[d + k] = avg565(avg565(a, b), avg565(c, e));
				}
				sx += 4;
			}
#else
			/* Drei Zielpunkte je vier Quellpunkte, ohne Division in der
			   Schleife -- jeder vierte faellt weg. */
			int sx = 0;
			for (int d = 0; d < PBC_GAME_W; d += 3)
			{
				dst[d]     = pal(s0[sx]);
				dst[d + 1] = pal(s0[sx + 1]);
				dst[d + 2] = pal(s0[sx + 2]);
				sx += 4;
			}
#endif
			continue;
		}

		if (y >= PBC_LINK_ROW_Y && y < PBC_LINK_ROW_Y + PBC_LINK_ROW_H)
		{
			pbc_hud_render_row(dst, y - PBC_LINK_ROW_Y);
			continue;
		}

		memset(dst, 0, PBC_TFT_W * sizeof *dst);
	}
}

/*
 * Menuemodus: Ausschnitt in Originalgroesse, mittig. Zur Begruendung siehe
 * PBC_MENU_W in pbc_config.h -- kurz: verkleinerte Schrift war nicht lesbar.
 */
static void compose_rows_menu(uint16_t *out, int y0, int rows)
{
	for (int r = 0; r < rows; ++r)
	{
		int y = y0 + r;
		uint16_t *dst = out + (size_t)r * PBC_TFT_W;

		if (y < PBC_MENU_Y || y >= PBC_MENU_Y + PBC_MENU_H)
		{
			memset(dst, 0, PBC_TFT_W * sizeof *dst);
			continue;
		}

		const Uint8 *src = (const Uint8 *)VGAScreen->pixels
		                 + (size_t)(y - PBC_MENU_Y) * VGAScreen->pitch
		                 + PBC_MENU_SRC_X;

		for (int x = 0; x < PBC_MENU_W; ++x)
			dst[x] = pal(src[x]);
	}
}

void JE_showVGA(void)
{
	/*
	 * Geschoben werden nur die Zeilen, in denen etwas steht -- 150 statt 280
	 * in der Spielansicht, 200 in Menues. Bei 62,5 MHz sind das rund 9 statt
	 * 17 ms je Bild, und der SPI-Bus ist hier der Engpass, nicht der Kern.
	 */
	/* SHOP und GAME werden gleich dargestellt -- sie unterscheiden sich nur
	   in der Tastenbelegung. */
	const bool scaled = (view_mode != PBC_VIEW_MENU);

	if (scaled)
	{
		pbc_hud_update();
		if (pbc_hud_dirty())
			full_refresh = true;
	}

	int y0, rows_total;

	if (full_refresh)
	{
		y0 = 0;
		rows_total = PBC_TFT_H;
	}
	else if (scaled)
	{
		y0 = PBC_GAME_Y;
		rows_total = PBC_GAME_H;
	}
	else
	{
		y0 = PBC_MENU_Y;
		rows_total = PBC_MENU_H;
	}

	pbc_display_begin(y0, rows_total);

	for (int r = 0; r < rows_total; r += PBC_STRIP_ROWS)
	{
		int rows = rows_total - r;
		if (rows > PBC_STRIP_ROWS)
			rows = PBC_STRIP_ROWS;

		uint16_t *buf = pbc_display_strip_buffer();

		if (scaled)
			compose_rows_game(buf, y0 + r, rows);
		else
			compose_rows_menu(buf, y0 + r, rows);

		pbc_display_push_strip(buf, rows * PBC_TFT_W);

		/*
		 * Der Streifen laeuft jetzt per DMA hinaus. Das ist die Gelegenheit,
		 * den Multiplayer-Link zu bedienen: er vertraegt keine langen Pausen,
		 * und ein Vollbild am Stueck waere eine.
		 */
		pbc_link_pump();
	}

	pbc_display_end();

	if (full_refresh)
	{
		full_refresh = false;
		pbc_hud_clear_dirty();
	}
}

/* ------------------------------------------------------------- Aufbau */

void init_video(void)
{
	pbc_display_init();
	pbc_hud_init();
	JE_clr256(VGAScreen);
}

void deinit_video(void)
{
	pbc_display_backlight(0);
}

/* ------------------------------------------ Nicht zutreffende Funktionen */

/*
 * Alles, was Fenster, Vollbild und Skalierer betrifft, hat auf einem Geraet mit
 * fest verbautem Panel keine Entsprechung. Die Funktionen bleiben, damit
 * OpenTyrians Optionsmenue uebersetzt; sie tun nichts.
 */
void video_on_win_resize(void) { }
void reinit_fullscreen(int new_display) { (void)new_display; }
void toggle_fullscreen(void) { }
bool init_scaler(unsigned int new_scaler) { (void)new_scaler; return true; }

/*
 * Skalierer: auf diesem Geraet gibt es genau einen, naemlich die feste
 * Bildaufteilung. Der Eintrag existiert, damit die gespeicherte Konfiguration
 * und das Optionsmenue etwas anzuzeigen haben.
 */
uint scaler = 0;
const struct Scalers scalers[] = {
	{ PBC_TFT_W, PBC_TFT_H, NULL, NULL, "PicoBoy" },
};
const uint scalers_count = COUNTOF(scalers);

void set_scaler_by_name(const char *name)
{
	(void)name;
}

bool set_scaling_mode_by_name(const char *name)
{
	for (unsigned i = 0; i < ScalingMode_MAX; ++i)
	{
		if (strcmp(name, scaling_mode_names[i]) == 0)
		{
			scaling_mode = (ScalingMode)i;
			return true;
		}
	}
	return false;
}

/*
 * Es gibt keine Maus, also auch keine Umrechnung zwischen Fenster- und
 * Spielkoordinaten. Die Funktionen liefern ihre Eingabe unveraendert zurueck.
 */
void mapScreenPointToWindow(Sint32 *inout_x, Sint32 *inout_y) { (void)inout_x; (void)inout_y; }
void mapWindowPointToScreen(Sint32 *inout_x, Sint32 *inout_y) { (void)inout_x; (void)inout_y; }
void scaleWindowDistanceToScreen(Sint32 *inout_x, Sint32 *inout_y) { (void)inout_x; (void)inout_y; }
