// ============================================================================
//  Jump 'n Bump  -  Portierung auf zwei PicoBoy Color Plus (RP2350), USB-Kopplung
//
//  Spielmechanik und Daten stammen 1:1 aus dem Original von Brainchild Design
//  (1998), SDL-Portierung von Florian Schulze u.a., GPL2+. Die Umsetzung haelt
//  sich an main.c des Originals: gleiche Festkommaphysik, gleiche Kachelkarte,
//  gleiche Animations- und Objekttabellen.
//
//  Anzeige: 240x280. Oben 240x256 Spielfeld (Originalpixel, horizontal
//  gescrollt - die volle Levelhoehe von 256 Pixeln passt genau), unten 24 Pixel
//  Punkteleiste. Jedes Geraet scrollt auf den EIGENEN Hasen.
// ============================================================================
#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "assets_gfx.h"
#include "assets_level.h"
#include "assets_snd.h"
#include "assets_title.h"

// ---- Hardware PicoBoy Color Plus -------------------------------------------
#define TFT_CS 10
#define TFT_RST 9
#define TFT_DC 8
#define KEY_RIGHT 1
#define KEY_DOWN 2
#define KEY_LEFT 3
#define KEY_UP 4
#define KEY_CENTER 0
#define KEY_A 27
#define KEY_B 28
#define BACKLIGHT 26
#define SPEAKER 15
#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 280

// ---- Bildaufteilung --------------------------------------------------------
#define JNB_PLAY_W 352 // Spielfeldbreite im Original (danach kaeme die Punktetafel)
#define VIEW_W 240     // sichtbarer Ausschnitt (Originalpixel)
#define VIEW_H 256     // volle Levelhoehe
#define HUD_Y VIEW_H
#define HUD_H (DISPLAY_HEIGHT - VIEW_H)

// ---- Originalkonstanten aus globals.h --------------------------------------
#define JNB_MAX_PLAYERS 2 // zwei Geraete = zwei Hasen
#define JNB_END_SCORE 100

#define NUM_OBJECTS 200
#define NUM_FLIES 20

#define OBJ_SPRING 0
#define OBJ_SPLASH 1
#define OBJ_SMOKE 2
#define OBJ_YEL_BUTFLY 3
#define OBJ_PINK_BUTFLY 4
#define OBJ_FUR 5
#define OBJ_FLESH 6
#define OBJ_FLESH_TRACE 7

#define OBJ_ANIM_SPRING 0
#define OBJ_ANIM_SPLASH 1
#define OBJ_ANIM_SMOKE 2
#define OBJ_ANIM_YEL_BUTFLY_RIGHT 3
#define OBJ_ANIM_YEL_BUTFLY_LEFT 4
#define OBJ_ANIM_PINK_BUTFLY_RIGHT 5
#define OBJ_ANIM_PINK_BUTFLY_LEFT 6
#define OBJ_ANIM_FLESH_TRACE 7

#define BAN_VOID 0
#define BAN_SOLID 1
#define BAN_WATER 2
#define BAN_ICE 3
#define BAN_SPRING 4

#define SFX_JUMP 0
#define SFX_DEATH 1
#define SFX_SPRING 2
#define SFX_SPLASH 3
#define SFX_FLY 4

#define SFX_JUMP_FREQ 15000
#define SFX_DEATH_FREQ 20000
#define SFX_SPRING_FREQ 15000
#define SFX_SPLASH_FREQ 12000
#define SFX_FLY_FREQ 12000

// ---- Spielzustand ----------------------------------------------------------
typedef struct {
  int action_left, action_up, action_right;
  int enabled, dead_flag;
  int bumps;
  int x, y;         // 16.16-Festkomma wie im Original
  int x_add, y_add; // Geschwindigkeit, 16.16
  int direction, jump_ready, jump_abort, in_water;
  int anim, frame, frame_tick, image;
  uint8_t deaths;    // zaehlt jeden Tod (Gast erkennt daran neue Gedaerme)
  uint8_t sfxflags;  // in diesem Tick ausgeloeste Klaenge/Effekte, s. SF_*
} player_t;

// Ereignisbits, die der Host im Zustandspaket mitschickt. Der Gast erzeugt
// daraus die gleichen Klaenge und Partikel, ohne dass sie einzeln uebertragen
// werden muessen.
#define SF_JUMP 0x01
#define SF_SPRING 0x02
#define SF_SPLASH 0x04
#define SF_SMOKE 0x08

typedef struct {
  int used, type;
  int x, y;
  int x_add, y_add;
  int x_acc, y_acc;
  int anim;
  int frame, ticks;
  int image;
} object_t;

typedef struct {
  int16_t x, y;
} fly_t;

extern player_t player[JNB_MAX_PLAYERS];
extern object_t objects[NUM_OBJECTS];
extern fly_t flies[NUM_FLIES];
extern int flies_enabled;

// ---- game.cpp --------------------------------------------------------------
void gameInit(void);            // Level aufbauen: Spieler setzen, Federn, Falter, Fliegen
void steer_players(void);       // Original: steer_players()
void collision_check(void);     // Original: collision_check()
void update_objects(void);      // Original: update_objects()
void update_flies(int with_sound);
void position_player(int n);
void add_object(int type, int x, int y, int x_add, int y_add, int anim, int frame);
void spawn_gore(int who, int x, int y); // Fell + Fleisch beim Tod, Original: processKillPacket
void player_kill(int killer, int victim);
unsigned short rnd(unsigned short max);
int ban(int x, int y); // GET_BAN_MAP_XY mit Bereichsschutz

// ---- Textausgabe mit der Originalschrift des Spiels (font.pcx) -------------
// Weiss mit schwarzem Rand, ganzzahlig vergroessert - passt zum Pixelbild.
int  gameTextWidth(const char *s, int scale);
void gameText(int x, int y, const char *s, int scale, uint16_t col);
void gameTextCentered(int cx, int y, const char *s, int scale, uint16_t col);
// Waehlt die groesste Vergroesserung, die noch in maxW passt; liefert sie zurueck.
int  gameTextFit(int cx, int y, const char *s, int maxW, int maxScale, uint16_t col);

// ---- render.cpp ------------------------------------------------------------
uint16_t blend565(uint16_t a, uint16_t b, uint8_t t); // t = Anteil von b, 0..255
void drawTitleBackdrop(void);
void drawGlassBox(int x, int y, int w, int h, uint8_t opacity);
void drawRoleMenu(int sel);                 // Titelbild + Box mit HOST/JOIN
void drawInfoScreen(const char *a, const char *b);
void drawWinnerBanner(int status);
bool renderInit(void);
void renderFrame(int me);   // zeichnet aus Sicht von Spieler "me"
void stainsClear(void);
void addStain(int x, int y, int image);

// ---- audio.cpp -------------------------------------------------------------
void audioInit(void);
void sfxPlay(uint8_t num, uint16_t freq, uint8_t volume, int8_t channel);
void sfxChannelVolume(uint8_t channel, uint8_t volume);
void musicStart(void);
void musicStop(void);
void musicVolume(uint8_t v);
uint8_t musicMaxVolume(void); // Zielwert der Einblendung

// ---- main.cpp --------------------------------------------------------------
extern uint8_t myPlayer; // 0 = Host, 1 = Gast
// Waehrend der rund 16 ms langen Bilduebertragung zwischendurch aufrufen, damit
// der USB-Stack nicht verhungert (der Gast schickt alle 12 ms seine Tasten).
void linkPump(void);
