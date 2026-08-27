/*
 * SDL-Attrappe fuer den PicoBoy-Color-Plus-Port von OpenTyrian.
 *
 * OpenTyrian zieht in fast jeder Quelldatei "SDL.h" herein, benutzt davon aber
 * nur einen kleinen, gut abgrenzbaren Ausschnitt: die Ganzzahltypen, eine
 * Oberflaechenstruktur (Zeiger auf Pixel + Breite/Hoehe/Pitch), Farben,
 * Rechtecke und die Scancode-Aufzaehlung. Alles Uebrige (Fenster, Renderer,
 * Texturen, Joysticks, Audio-Geraete) taucht nur in Dateien auf, die dieser
 * Port ohnehin ersetzt.
 *
 * Statt 100 Quelldateien anzufassen liegt dieses Verzeichnis im Include-Pfad
 * VOR einem echten SDL. Damit bleiben die OpenTyrian-Quellen unveraendert und
 * ein spaeterer Abgleich mit dem Upstream bleibt moeglich.
 *
 * GPLv2, wie OpenTyrian.
 */
#ifndef PBC_FAKE_SDL_H
#define PBC_FAKE_SDL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ Typen */

typedef int8_t   Sint8;
typedef uint8_t  Uint8;
typedef int16_t  Sint16;
typedef uint16_t Uint16;
typedef int32_t  Sint32;
typedef uint32_t Uint32;
typedef int64_t  Sint64;
typedef uint64_t Uint64;

typedef enum { SDL_FALSE = 0, SDL_TRUE = 1 } SDL_bool;

/* --------------------------------------------------------------- Endianness */

#define SDL_LIL_ENDIAN 1234
#define SDL_BIG_ENDIAN 4321

#ifndef SDL_BYTEORDER
#  if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#    define SDL_BYTEORDER SDL_BIG_ENDIAN
#  else
#    define SDL_BYTEORDER SDL_LIL_ENDIAN
#  endif
#endif

static inline Uint16 SDL_Swap16(Uint16 x) { return (Uint16)((x << 8) | (x >> 8)); }
static inline Uint32 SDL_Swap32(Uint32 x)
{
	return ((x << 24) & 0xff000000u) | ((x <<  8) & 0x00ff0000u) |
	       ((x >>  8) & 0x0000ff00u) | ((x >> 24) & 0x000000ffu);
}

#if SDL_BYTEORDER == SDL_LIL_ENDIAN
#  define SDL_SwapLE16(x) (x)
#  define SDL_SwapLE32(x) (x)
#else
#  define SDL_SwapLE16(x) SDL_Swap16(x)
#  define SDL_SwapLE32(x) SDL_Swap32(x)
#endif

/* Der Port baut nur gegen diese Attrappe; Versionsabfragen sind gegenstandslos.
   "immer alt genug" ist hier die richtige Antwort: die betroffenen Zweige in
   OpenTyrian schalten neuere SDL2-Bequemlichkeiten zu, die wir nicht haben. */
#define SDL_VERSION_ATLEAST(x, y, z) 0

/* ------------------------------------------------------------ Oberflaechen */

/*
 * OpenTyrian rendert ausschliesslich in 8-Bit-Palettenpuffer fester Groesse
 * (320x200) und schreibt direkt in ->pixels. Mehr als diese vier Felder
 * werden nie gelesen -- format/userdata existieren nur, damit die wenigen
 * Stellen uebersetzen, die sie erwaehnen.
 */
typedef struct SDL_PixelFormat
{
	Uint8 BitsPerPixel;
	Uint8 BytesPerPixel;
} SDL_PixelFormat;

typedef struct SDL_Surface
{
	void *pixels;
	int w, h;
	int pitch;
	SDL_PixelFormat *format;
} SDL_Surface;

typedef struct SDL_Color
{
	Uint8 r, g, b, a;
} SDL_Color;

typedef struct SDL_Rect
{
	int x, y, w, h;
} SDL_Rect;

/* Software-Oberflaechen muessen nie gesperrt werden. */
#define SDL_MUSTLOCK(surface) (0)

/* In pbc_video.c implementiert. */
void SDL_FillRect(SDL_Surface *dst, const SDL_Rect *rect, Uint32 color);
void SDL_BlitSurface(SDL_Surface *src, const SDL_Rect *srcrect,
                     SDL_Surface *dst, SDL_Rect *dstrect);
Uint32 SDL_MapRGB(const SDL_PixelFormat *fmt, Uint8 r, Uint8 g, Uint8 b);

/* ------------------------------------------------------------------ Fenster */

/* Es gibt genau einen Bildschirm und er ist unveraenderlich. Die Typen
   existieren nur als Platzhalter fuer Zeiger, die nie benutzt werden. */
typedef struct SDL_Window SDL_Window;
typedef struct SDL_Texture SDL_Texture;

/* ------------------------------------------------------------------ Zeit */

Uint32 SDL_GetTicks(void);
void SDL_Delay(Uint32 ms);

/* --------------------------------------------------------------- Sonstiges */

#define SDL_INIT_VIDEO     0x00000020u
#define SDL_INIT_AUDIO     0x00000010u
#define SDL_INIT_JOYSTICK  0x00000200u

int  SDL_Init(Uint32 flags);
int  SDL_InitSubSystem(Uint32 flags);
void SDL_QuitSubSystem(Uint32 flags);
Uint32 SDL_WasInit(Uint32 flags);
void SDL_Quit(void);
const char *SDL_GetError(void);

size_t SDL_strlcpy(char *dst, const char *src, size_t maxlen);

/* --------------------------------------------------------------- Scancodes */

/*
 * Nur die von OpenTyrian tatsaechlich benannten Tasten. Die Zahlenwerte sind
 * die echten SDL2-Scancodes -- so bleiben in der Konfiguration gespeicherte
 * Tastenbelegungen mit denen einer PC-Fassung vergleichbar, auch wenn dieses
 * Geraet nur sieben Knoepfe hat.
 */
typedef enum
{
	SDL_SCANCODE_UNKNOWN = 0,

	SDL_SCANCODE_A = 4,  SDL_SCANCODE_C = 6,  SDL_SCANCODE_F = 9,
	SDL_SCANCODE_N = 17, SDL_SCANCODE_P = 19, SDL_SCANCODE_Q = 20,
	SDL_SCANCODE_R = 21, SDL_SCANCODE_S = 22, SDL_SCANCODE_T = 23,
	SDL_SCANCODE_V = 25, SDL_SCANCODE_W = 26, SDL_SCANCODE_X = 27,
	SDL_SCANCODE_Z = 29,

	SDL_SCANCODE_1 = 30, SDL_SCANCODE_2 = 31, SDL_SCANCODE_3 = 32,
	SDL_SCANCODE_4 = 33, SDL_SCANCODE_5 = 34, SDL_SCANCODE_6 = 35,
	SDL_SCANCODE_7 = 36, SDL_SCANCODE_8 = 37, SDL_SCANCODE_9 = 38,
	SDL_SCANCODE_0 = 39,

	SDL_SCANCODE_RETURN = 40, SDL_SCANCODE_ESCAPE = 41,
	SDL_SCANCODE_BACKSPACE = 42, SDL_SCANCODE_TAB = 43,
	SDL_SCANCODE_SPACE = 44,

	SDL_SCANCODE_MINUS = 45, SDL_SCANCODE_EQUALS = 46,
	SDL_SCANCODE_LEFTBRACKET = 47, SDL_SCANCODE_RIGHTBRACKET = 48,
	SDL_SCANCODE_BACKSLASH = 49,
	SDL_SCANCODE_SEMICOLON = 51,
	SDL_SCANCODE_GRAVE = 53, SDL_SCANCODE_COMMA = 54,
	SDL_SCANCODE_PERIOD = 55, SDL_SCANCODE_SLASH = 56,
	SDL_SCANCODE_CAPSLOCK = 57,

	SDL_SCANCODE_F1 = 58, SDL_SCANCODE_F2 = 59, SDL_SCANCODE_F3 = 60,
	SDL_SCANCODE_F4 = 61, SDL_SCANCODE_F5 = 62, SDL_SCANCODE_F6 = 63,
	SDL_SCANCODE_F7 = 64, SDL_SCANCODE_F8 = 65, SDL_SCANCODE_F9 = 66,
	SDL_SCANCODE_F10 = 67, SDL_SCANCODE_F11 = 68, SDL_SCANCODE_F12 = 69,

	SDL_SCANCODE_SCROLLLOCK = 71,
	SDL_SCANCODE_INSERT = 73, SDL_SCANCODE_HOME = 74,
	SDL_SCANCODE_PAGEUP = 75, SDL_SCANCODE_DELETE = 76,
	SDL_SCANCODE_END = 77, SDL_SCANCODE_PAGEDOWN = 78,
	SDL_SCANCODE_RIGHT = 79, SDL_SCANCODE_LEFT = 80,
	SDL_SCANCODE_DOWN = 81, SDL_SCANCODE_UP = 82,

	SDL_SCANCODE_NUMLOCKCLEAR = 83,
	SDL_SCANCODE_KP_ENTER = 88,
	SDL_SCANCODE_KP_0 = 98, SDL_SCANCODE_KP_2 = 90, SDL_SCANCODE_KP_3 = 91,
	SDL_SCANCODE_KP_4 = 92, SDL_SCANCODE_KP_5 = 93, SDL_SCANCODE_KP_6 = 94,
	SDL_SCANCODE_KP_8 = 96, SDL_SCANCODE_KP_9 = 97,

	SDL_SCANCODE_LCTRL = 224, SDL_SCANCODE_LSHIFT = 225,
	SDL_SCANCODE_LALT = 226,
	SDL_SCANCODE_RCTRL = 228, SDL_SCANCODE_RALT = 230,

	SDL_NUM_SCANCODES = 512
} SDL_Scancode;

typedef Sint32 SDL_Keycode;
typedef Uint16 SDL_Keymod;

/* Umschalttasten. Dieses Geraet erzeugt keine davon -- die Konstanten werden
   aber in Tastenkombinationen der Menues abgefragt und muessen existieren. */
#define KMOD_NONE   0x0000
#define KMOD_LSHIFT 0x0001
#define KMOD_RSHIFT 0x0002
#define KMOD_LCTRL  0x0040
#define KMOD_RCTRL  0x0080
#define KMOD_LALT   0x0100
#define KMOD_RALT   0x0200
#define KMOD_LGUI   0x0400
#define KMOD_RGUI   0x0800
#define KMOD_NUM    0x1000
#define KMOD_CAPS   0x2000
#define KMOD_SHIFT  (KMOD_LSHIFT | KMOD_RSHIFT)
#define KMOD_CTRL   (KMOD_LCTRL | KMOD_RCTRL)
#define KMOD_ALT    (KMOD_LALT | KMOD_RALT)
#define KMOD_GUI    (KMOD_LGUI | KMOD_RGUI)

#define SDLK_d 'd'
#define SDLK_g 'g'
#define SDLK_l 'l'
#define SDLK_o 'o'
#define SDLK_r 'r'
#define SDLK_s 's'
#define SDLK_RIGHTBRACKET ']'

const char *SDL_GetScancodeName(SDL_Scancode scancode);
SDL_Scancode SDL_GetScancodeFromName(const char *name);

/* ------------------------------------------------------------------ Maus */

/* Es gibt keine Maus. Die Konstanten muessen nur existieren; mouse.c und die
   Menues fragen die Knoepfe ab, bekommen aber nie einen Klick geliefert. */
#define SDL_BUTTON(X)     (1u << ((X) - 1))
#define SDL_BUTTON_LEFT   1
#define SDL_BUTTON_MIDDLE 2
#define SDL_BUTTON_RIGHT  3
#define SDL_BUTTON_LMASK  SDL_BUTTON(SDL_BUTTON_LEFT)
#define SDL_BUTTON_MMASK  SDL_BUTTON(SDL_BUTTON_MIDDLE)
#define SDL_BUTTON_RMASK  SDL_BUTTON(SDL_BUTTON_RIGHT)

int SDL_ShowCursor(int toggle);
int SDL_SetRelativeMouseMode(SDL_bool enabled);
#define SDL_DISABLE 0
#define SDL_ENABLE  1
#define SDL_IGNORE  0

SDL_bool SDL_SetHint(const char *name, const char *value);
#define SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE "SDL_MOUSE_RELATIVE_SYSTEM_SCALE"

void SDL_StartTextInput(void);
void SDL_StopTextInput(void);

/* --------------------------------------------------------------- Ereignisse */

/*
 * Der Port erzeugt keine SDL-Ereignisse -- die Tasten werden in pbc_input.c
 * direkt per GPIO abgefragt und in OpenTyrians eigene Zustandsfelder
 * (keysactive, mousedown, ...) geschrieben. SDL_PollEvent meldet deshalb
 * immer "nichts da". Die Struktur existiert nur, damit keyboard.c uebersetzt.
 */
typedef enum
{
	SDL_QUIT = 0x100,
	SDL_WINDOWEVENT = 0x200,
	SDL_KEYDOWN = 0x300, SDL_KEYUP,
	SDL_TEXTINPUT = 0x303,
	SDL_MOUSEMOTION = 0x400, SDL_MOUSEBUTTONDOWN, SDL_MOUSEBUTTONUP
} SDL_EventType;

#define SDL_WINDOWEVENT_RESIZED       5
#define SDL_WINDOWEVENT_FOCUS_GAINED 12
#define SDL_WINDOWEVENT_FOCUS_LOST   13

#define SDL_PRESSED  1
#define SDL_RELEASED 0

typedef struct SDL_Keysym
{
	SDL_Scancode scancode;
	SDL_Keycode sym;
	SDL_Keymod mod;
} SDL_Keysym;

typedef struct SDL_Event
{
	Uint32 type;
	struct { Uint8 event; Sint32 data1, data2; } window;
	struct { Uint8 state; Uint8 repeat; SDL_Keysym keysym; } key;
	struct { char text[32]; } text;
	struct { Sint32 x, y, xrel, yrel; Uint32 state; } motion;
	struct { Uint8 button; Uint8 state; Sint32 x, y; } button;
} SDL_Event;

int SDL_PollEvent(SDL_Event *event);
int SDL_PushEvent(SDL_Event *event);

/* ------------------------------------------------------------------ Joystick */

/*
 * Der PicoBoy hat keinen anschliessbaren Joystick -- sein Steuerkreuz sitzt an
 * GPIOs und wird in pbc_input.c gelesen. joystick.c wird nicht mitgebaut;
 * diese Deklarationen fangen nur die Erwaehnungen in den Menue-Dateien ab.
 */
typedef struct _SDL_Joystick SDL_Joystick;

#define SDL_HAT_CENTERED 0x00
#define SDL_HAT_UP       0x01
#define SDL_HAT_RIGHT    0x02
#define SDL_HAT_DOWN     0x04
#define SDL_HAT_LEFT     0x08

int SDL_NumJoysticks(void);
SDL_Joystick *SDL_JoystickOpen(int index);
void SDL_JoystickClose(SDL_Joystick *j);
const char *SDL_JoystickName(SDL_Joystick *j);
int SDL_JoystickNumAxes(SDL_Joystick *j);
int SDL_JoystickNumButtons(SDL_Joystick *j);
int SDL_JoystickNumHats(SDL_Joystick *j);
Sint16 SDL_JoystickGetAxis(SDL_Joystick *j, int axis);
Uint8 SDL_JoystickGetButton(SDL_Joystick *j, int button);
Uint8 SDL_JoystickGetHat(SDL_Joystick *j, int hat);
void SDL_JoystickUpdate(void);
int SDL_JoystickEventState(int state);

int SDL_GetNumVideoDisplays(void);

/* ------------------------------------------------------------------ Audio */

/*
 * loudness.c wird durch pbc_audio.c ersetzt; diese Typen existieren nur, damit
 * die Kopfdatei uebersetzt. Der Mischer laeuft hier nicht in einem
 * SDL-Rueckruf, sondern speist einen PWM-DMA-Ringpuffer.
 */
typedef Uint32 SDL_AudioDeviceID;

typedef struct SDL_AudioSpec
{
	int freq;
	Uint16 format;
	Uint8 channels;
	Uint16 samples;
	Uint32 size;
	void (*callback)(void *userdata, Uint8 *stream, int len);
	void *userdata;
} SDL_AudioSpec;

typedef struct SDL_AudioCVT
{
	int needed;
	int len;
	int len_cvt;
	int len_mult;
	double len_ratio;
	Uint8 *buf;
} SDL_AudioCVT;

#define AUDIO_S8     0x8008
#define AUDIO_S16SYS 0x8010
#define SDL_AUDIO_ALLOW_FREQUENCY_CHANGE 0x01
#define SDL_AUDIO_ALLOW_SAMPLES_CHANGE   0x08

SDL_AudioDeviceID SDL_OpenAudioDevice(const char *device, int iscapture,
                                      const SDL_AudioSpec *desired,
                                      SDL_AudioSpec *obtained,
                                      int allowed_changes);
void SDL_LockAudioDevice(SDL_AudioDeviceID dev);
void SDL_UnlockAudioDevice(SDL_AudioDeviceID dev);
void SDL_PauseAudioDevice(SDL_AudioDeviceID dev, int pause_on);
void SDL_CloseAudioDevice(SDL_AudioDeviceID dev);

int SDL_BuildAudioCVT(SDL_AudioCVT *cvt, Uint16 src_format, Uint8 src_channels,
                      int src_rate, Uint16 dst_format, Uint8 dst_channels,
                      int dst_rate);
int SDL_ConvertAudio(SDL_AudioCVT *cvt);

#ifdef __cplusplus
}
#endif

#endif /* PBC_FAKE_SDL_H */
