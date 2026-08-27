/* game.h -- Super Transball 2 fuer den PicoBoy Color Plus.
 *
 * Portiert aus Super Transball 2 von Santiago Ontanon, GPL-2.0 -- siehe
 * COPYING.  Die Portierung steht damit ebenfalls unter GPL-2.0.
 *
 * Plattformunabhaengiger Kern: kennt weder Display noch Tasten noch Ton.
 * Gezeichnet wird direkt ins Panelformat (RGB565), weil die Kacheln schon so
 * vorliegen -- anders als bei den Vektorspielen lohnt hier kein Indexpuffer.
 */
#ifndef STB_GAME_H
#define STB_GAME_H

#include <stdint.h>
#include "data.h"

#define FACTOR      512                    /* Festkomma wie in der Vorlage  */
#define EMPTY_ROWS  8                      /* leere Zeilen ueber der Karte  */

#define SCR_W       240
#define SCR_H       280
#define VIEW_H      256                    /* Spielfeld, darunter Anzeige   */
#define HUD_H       (SCR_H - VIEW_H)

#define MAP_MAX_CELLS (96 * 48)
#define MAX_ENEMIES   192
#define MAX_BULLETS   24
#define MAX_DOORS     32
#define MAX_SWITCHES  16
#define MAX_FUEL      16
#define MAX_ATRACT_P  48

/* ---- Tasten -------------------------------------------------------------- */
#define PAD_RI 0x0001
#define PAD_LF 0x0002
#define PAD_DN 0x0004
#define PAD_UP 0x0008
#define PAD_B  0x0010
#define PAD_A  0x0020
#define PAD_C  0x0040
#define TRG_RI 0x0100
#define TRG_LF 0x0200
#define TRG_DN 0x0400
#define TRG_UP 0x0800
#define TRG_B  0x1000
#define TRG_A  0x2000
#define TRG_C  0x4000

enum { ST_TITLE, ST_SHIPSEL, ST_GAME, ST_DEAD, ST_LEVELDONE, ST_GAMEOVER, ST_WON };

/* ---- Tonereignisse ------------------------------------------------------- */
#define EV_SHOT      0x0001
#define EV_ENEMYSHOT 0x0002
#define EV_HIT       0x0004
#define EV_EXPLODE   0x0008
#define EV_TAKEBALL  0x0010
#define EV_SWITCH    0x0020
#define EV_FUEL      0x0040
#define EV_UI        0x0080
#define EV_LEVELDONE 0x0100
#define EV_DIE       0x0200

typedef struct {
    uint16_t ev;
    uint8_t  thrust;
} stb_audio_t;

typedef struct {
    int status;
    int level, ship_type, lives;
    int fuel, fuel_max;
    int ship_x, ship_y, ship_angle;        /* Festkomma bzw. Grad           */
    int ball_state;
    int map_x, map_y;                      /* linke obere Ecke der Ansicht  */
    int n_enemies, n_bullets;
    int shots, hits, destroyed, fuel_used;
    char msg[24];
    char msg2[28];
    stb_audio_t audio;
} stb_state_t;

extern stb_state_t stb;

void stb_init(uint32_t seed, int hiscore_level);
void stb_tick(uint16_t pad);
uint32_t stb_rand(void);
int  stb_selfcheck(void);

/* Zeichnet die Bildzeilen [y0, y0+rows) nach dst (rows * SCR_W, RGB565). */
void stb_render_rows(uint16_t *dst, int y0, int rows);
void stb_render_init(void);

typedef struct { int blink, show_debug, ms_frame, ms_cpu, mute; } stb_render_cfg_t;
extern stb_render_cfg_t stb_render;

#endif
