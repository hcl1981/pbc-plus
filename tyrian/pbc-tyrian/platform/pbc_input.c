/*
 * pbc_input -- die sieben Knoepfe des PicoBoy als Tastatur.
 *
 * OpenTyrians keyboard.c ist ein sauber abgegrenzter Baustein: es fragt
 * SDL_PollEvent ab und pflegt daraus keysactive[] sowie eine Eingabe-
 * warteschlange. Diese Datei ersetzt deshalb NICHT keyboard.c, sondern nur
 * SDL_PollEvent -- damit bleiben Menuefuehrung, Tastenwiederholung und
 * Texteingabe des Originals unveraendert in Betrieb.
 *
 * Der PicoBoy hat ein Steuerkreuz und zwei Knoepfe. Tyrian erwartet acht
 * Tasten (vier Richtungen, Feuer, Waffenwechsel, zwei Begleiter) plus ESC.
 * Die Luecke schliessen zwei Tastenkombinationen -- dasselbe Verfahren, mit
 * dem die Doom-Portierung auf derselben Hardware ihr Menue erreicht.
 *
 * Was ein Knopf bedeutet, haengt davon ab, ob gerade gespielt oder ein Menue
 * bedient wird. Das weiss die Ansicht (pbc_video.h): PBC_VIEW_GAME heisst
 * Spiel, alles andere Menue. Eine eigene Zustandsverwaltung braucht es dafuer
 * nicht.
 *
 *   Im Spiel
 *     Steuerkreuz          Pfeiltasten (fliegen)
 *     B                    LEER          Feuer
 *     A                    EINGABE       Waffenmodus wechseln
 *     Mitte antippen       ESC           Menue
 *     Mitte halten + A     STRG links    linker Begleiter
 *     Mitte halten + B     ALT links     rechter Begleiter
 *
 *   In Menues und im Laden
 *     Steuerkreuz          navigieren
 *     B                    bestaetigen
 *     A                    ESC           zurueck
 *     Mitte antippen       bestaetigen
 *
 * Tyrian will acht Tasten, das Geraet hat sieben Knoepfe -- die Mitte ist
 * deshalb Umschalttaste UND eigene Taste. Ihre eigene Wirkung loest erst beim
 * LOSLASSEN aus, und nur wenn zwischendurch kein anderer Knopf kam. Das ist
 * hier wichtiger als frueher: waere sie sofort wirksam, oeffnete jeder Griff
 * zum Begleiter zuerst das Menue. Die Verzoegerung faellt nicht auf -- sie
 * betrifft nur Menue und Bestaetigen, nicht das Fliegen und Schiessen.
 *
 * GPLv2, wie OpenTyrian.
 */

#include "SDL.h"

#include <string.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"

#include "pbc_config.h"
#include "pbc_input.h"
#include "pbc_video.h"

/* --------------------------------------------------------- Knoepfe */

enum
{
	BTN_UP = 0, BTN_DOWN, BTN_LEFT, BTN_RIGHT,
	BTN_CENTER, BTN_A, BTN_B,
	BTN_COUNT
};

static const uint8_t btn_pin[BTN_COUNT] = {
	PBC_PIN_JOY_UP, PBC_PIN_JOY_DOWN, PBC_PIN_JOY_LEFT, PBC_PIN_JOY_RIGHT,
	PBC_PIN_JOY_CENTER, PBC_PIN_BTN_A, PBC_PIN_BTN_B,
};

static uint8_t state, state_prev;
static uint8_t debounce[BTN_COUNT];
static bool    inited;

/*
 * Entprellen: ein Rohwert muss zweimal hintereinander abweichen, bevor der
 * gemerkte Zustand kippt. Abgefragt wird bei jedem SDL_PollEvent, also
 * mehrfach je Bild -- zwei Durchlaeufe sind damit wenige Millisekunden und
 * fallen beim Fliegen nicht auf.
 */
#define DEBOUNCE_POLLS 2

/* Nicht schneller als alle 2 ms abfragen: SDL_PollEvent wird in engen
   Schleifen gerufen, und jeder Durchlauf liest sieben GPIOs. */
#define POLL_INTERVAL_US 2000
static uint64_t last_poll_us;

static uint8_t read_raw(void)
{
	uint32_t all = gpio_get_all();
	uint8_t s = 0;
	for (int i = 0; i < BTN_COUNT; ++i)
		if ((all & (1u << btn_pin[i])) == 0)   /* aktiv LOW */
			s |= (uint8_t)(1u << i);
	return s;
}

void pbc_input_init(void)
{
	if (inited)
		return;
	inited = true;

	for (int i = 0; i < BTN_COUNT; ++i)
	{
		gpio_init(btn_pin[i]);
		gpio_set_dir(btn_pin[i], GPIO_IN);
		gpio_pull_up(btn_pin[i]);
	}

	/* Den Pullups einen Moment geben, sonst liest der erste Durchlauf alle
	   Knoepfe als gedrueckt und das Spiel startet mit einem Tastendruck. */
	busy_wait_us(3000);
	state = state_prev = read_raw();
	memset(debounce, 0, sizeof debounce);
}

/* ------------------------------------------------- Ereigniswarteschlange */

#define EVQ_LEN 32
static SDL_Event evq[EVQ_LEN];
static int evq_head, evq_count;

static void push_event(const SDL_Event *ev)
{
	if (evq_count >= EVQ_LEN)
		return;   /* lieber verlieren als ueberschreiben */
	evq[(evq_head + evq_count) % EVQ_LEN] = *ev;
	++evq_count;
}

static void push_key(SDL_Scancode sc, bool down)
{
	SDL_Event ev;
	memset(&ev, 0, sizeof ev);
	ev.type = down ? SDL_KEYDOWN : SDL_KEYUP;
	ev.key.state = down ? SDL_PRESSED : SDL_RELEASED;
	ev.key.repeat = 0;
	ev.key.keysym.scancode = sc;
	ev.key.keysym.sym = 0;
	ev.key.keysym.mod = 0;
	push_event(&ev);
}

/* ------------------------------------------------------- Zuordnung */

/*
 * Welche Taste ein Knopf gerade bedeutet, haengt an den Kombinationen. Damit
 * ein Loslassen immer dieselbe Taste freigibt wie das Druecken sie belegt hat
 * -- sonst bliebe eine Taste haengen und das Schiff schoesse ewig weiter --
 * wird die einmal vergebene Zuordnung je Knopf gemerkt.
 */
static SDL_Scancode held_key[BTN_COUNT];

/* Eigene Wirkung der Mitte -- ausgeloest beim Loslassen, siehe oben. */
static SDL_Scancode center_key(void)
{
	return (pbc_get_view_mode() == PBC_VIEW_GAME)
	     ? SDL_SCANCODE_ESCAPE      /* Menue */
	     : SDL_SCANCODE_RETURN;     /* bestaetigen */
}

static SDL_Scancode map_press(int btn, uint8_t now)
{
	const bool center = (now & (1u << BTN_CENTER)) != 0;
	const bool ingame = (pbc_get_view_mode() == PBC_VIEW_GAME);

	switch (btn)
	{
		case BTN_UP:    return SDL_SCANCODE_UP;
		case BTN_DOWN:  return SDL_SCANCODE_DOWN;
		case BTN_LEFT:  return SDL_SCANCODE_LEFT;
		case BTN_RIGHT: return SDL_SCANCODE_RIGHT;

		case BTN_CENTER: return SDL_SCANCODE_UNKNOWN;   /* siehe center_key */

		case BTN_A:
			if (!ingame)
				return SDL_SCANCODE_ESCAPE;           /* zurueck */
			return center ? SDL_SCANCODE_LCTRL        /* linker Begleiter */
			              : SDL_SCANCODE_RETURN;      /* Waffenmodus */

		case BTN_B:
			if (!ingame)
				return SDL_SCANCODE_SPACE;            /* bestaetigen */
			return center ? SDL_SCANCODE_LALT         /* rechter Begleiter */
			              : SDL_SCANCODE_SPACE;       /* Feuer */

		default:
			return SDL_SCANCODE_UNKNOWN;
	}
}

static void poll_buttons(void)
{
	uint64_t t = time_us_64();
	if (t - last_poll_us < POLL_INTERVAL_US)
		return;
	last_poll_us = t;

	uint8_t raw = read_raw();

	for (int i = 0; i < BTN_COUNT; ++i)
	{
		bool raw_down = (raw & (1u << i)) != 0;
		bool cur_down = (state & (1u << i)) != 0;

		if (raw_down == cur_down)
		{
			debounce[i] = 0;
			continue;
		}
		if (++debounce[i] < DEBOUNCE_POLLS)
			continue;

		debounce[i] = 0;
		state ^= (uint8_t)(1u << i);
	}

	const uint8_t pressed  = (uint8_t)(state & ~state_prev);
	const uint8_t released = (uint8_t)(~state & state_prev);

	/* Wurde die Mitte waehrend ihres Gedrueckthaltens als Umschalttaste
	   benutzt? Dann entfaellt ihre eigene Wirkung beim Loslassen. */
	static bool center_modified;

	if (pressed & (1u << BTN_CENTER))
		center_modified = false;

	for (int i = 0; i < BTN_COUNT; ++i)
	{
		if (pressed & (1u << i))
		{
			if (i == BTN_CENTER)
				continue;   /* Wirkung erst beim Loslassen */

			if ((i == BTN_A || i == BTN_B) && (state & (1u << BTN_CENTER)))
				center_modified = true;

			/*
			 * Die Zuordnung wird beim DRUECKEN festgehalten und beim
			 * Loslassen wiederverwendet. Sonst gaebe ein Ansichtswechsel
			 * zwischen Druck und Loslassen eine Taste frei, die nie gedrueckt
			 * wurde -- und die gedrueckte bliebe haengen.
			 */
			held_key[i] = map_press(i, state);
			push_key(held_key[i], true);
		}

		if (released & (1u << i))
		{
			if (i == BTN_CENTER)
			{
				/* Kurz getippt und nichts dazwischen: eigene Wirkung.
				   Druck und Loslassen unmittelbar hintereinander, damit
				   OpenTyrian beides im selben Bild sieht. */
				if (!center_modified)
				{
					const SDL_Scancode sc = center_key();
					push_key(sc, true);
					push_key(sc, false);
				}
				continue;
			}

			if (held_key[i] != SDL_SCANCODE_UNKNOWN)
				push_key(held_key[i], false);
			held_key[i] = SDL_SCANCODE_UNKNOWN;
		}
	}

	state_prev = state;
}

bool pbc_input_button_held(int choice)
{
	pbc_input_init();

	const uint8_t mask = (choice == PBC_CHOICE_A)      ? (uint8_t)(1u << BTN_A)
	                   : (choice == PBC_CHOICE_B)      ? (uint8_t)(1u << BTN_B)
	                   :                                 (uint8_t)(1u << BTN_CENTER);

	/* Zweimal mit Abstand lesen -- ein einzelner Blick kann prellen. */
	uint8_t raw = read_raw();
	busy_wait_us(20000);
	return (raw & read_raw() & mask) != 0;
}

/*
 * Auswahl beim Start. Bewusst ohne den Ereignisweg oben -- der setzt voraus,
 * dass OpenTyrian bereits laeuft und die Warteschlange abholt.
 */
int pbc_input_wait_choice(void)
{
	pbc_input_init();

	/* Erst warten, bis nichts mehr gedrueckt ist: sonst uebernimmt die
	   Auswahl einen Knopf, der noch vom Einschalten gehalten wird. */
	while (read_raw() != 0)
		busy_wait_us(2000);

	for (;;)
	{
		uint8_t raw = read_raw();
		if (raw == 0)
		{
			busy_wait_us(2000);
			continue;
		}

		/* Grob entprellen: kurz warten und noch einmal nachsehen. */
		busy_wait_us(20000);
		raw &= read_raw();

		if (raw & (1u << BTN_A))      return PBC_CHOICE_A;
		if (raw & (1u << BTN_B))      return PBC_CHOICE_B;
		if (raw & (1u << BTN_CENTER)) return PBC_CHOICE_CENTER;
	}
}

/* ------------------------------------------------------- SDL-Ersatz */

int SDL_PollEvent(SDL_Event *event)
{
	if (!inited)
		pbc_input_init();

	poll_buttons();

	if (evq_count == 0)
		return 0;

	if (event != NULL)
		*event = evq[evq_head];
	evq_head = (evq_head + 1) % EVQ_LEN;
	--evq_count;
	return 1;
}

int SDL_PushEvent(SDL_Event *event)
{
	if (event != NULL)
		push_event(event);
	return 1;
}

/* ------------------------------------------------- Nicht vorhanden */

/* Es gibt keine Maus und keinen anschliessbaren Joystick. Die Funktionen
   existieren, damit OpenTyrians Menues uebersetzen. */

int SDL_ShowCursor(int toggle) { (void)toggle; return 0; }
int SDL_SetRelativeMouseMode(SDL_bool enabled) { (void)enabled; return 0; }
SDL_bool SDL_SetHint(const char *n, const char *v) { (void)n; (void)v; return SDL_TRUE; }
void SDL_StartTextInput(void) { }
void SDL_StopTextInput(void) { }

int SDL_NumJoysticks(void) { return 0; }
SDL_Joystick *SDL_JoystickOpen(int i) { (void)i; return NULL; }
void SDL_JoystickClose(SDL_Joystick *j) { (void)j; }
const char *SDL_JoystickName(SDL_Joystick *j) { (void)j; return ""; }
int SDL_JoystickNumAxes(SDL_Joystick *j) { (void)j; return 0; }
int SDL_JoystickNumButtons(SDL_Joystick *j) { (void)j; return 0; }
int SDL_JoystickNumHats(SDL_Joystick *j) { (void)j; return 0; }
Sint16 SDL_JoystickGetAxis(SDL_Joystick *j, int a) { (void)j; (void)a; return 0; }
Uint8 SDL_JoystickGetButton(SDL_Joystick *j, int b) { (void)j; (void)b; return 0; }
Uint8 SDL_JoystickGetHat(SDL_Joystick *j, int h) { (void)j; (void)h; return SDL_HAT_CENTERED; }
void SDL_JoystickUpdate(void) { }
int SDL_JoystickEventState(int s) { (void)s; return 0; }

int SDL_GetNumVideoDisplays(void) { return 1; }

/* ------------------------------------------------- Tastennamen */

/*
 * Nur die Tasten, die dieses Geraet ueberhaupt erzeugen kann. Die Namen
 * erscheinen im Optionsmenue und in der gespeicherten Konfiguration.
 */
static const struct { SDL_Scancode sc; const char *name; } key_names[] = {
	{ SDL_SCANCODE_UP,     "Up"     },
	{ SDL_SCANCODE_DOWN,   "Down"   },
	{ SDL_SCANCODE_LEFT,   "Left"   },
	{ SDL_SCANCODE_RIGHT,  "Right"  },
	{ SDL_SCANCODE_SPACE,  "Space"  },
	{ SDL_SCANCODE_RETURN, "Return" },
	{ SDL_SCANCODE_LCTRL,  "Left Ctrl" },
	{ SDL_SCANCODE_LALT,   "Left Alt"  },
	{ SDL_SCANCODE_ESCAPE, "Escape" },
};

const char *SDL_GetScancodeName(SDL_Scancode scancode)
{
	for (unsigned i = 0; i < sizeof key_names / sizeof *key_names; ++i)
		if (key_names[i].sc == scancode)
			return key_names[i].name;
	return "";
}

SDL_Scancode SDL_GetScancodeFromName(const char *name)
{
	for (unsigned i = 0; i < sizeof key_names / sizeof *key_names; ++i)
		if (strcmp(key_names[i].name, name) == 0)
			return key_names[i].sc;
	return SDL_SCANCODE_UNKNOWN;
}
