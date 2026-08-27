/* game.h -- Noiz2sa fuer den PicoBoy Color Plus, plattformunabhaengiger Kern.
 *
 * Portiert aus noiz2sa 0.52 von Kenta Cho, BSD-Lizenz, siehe COPYING.
 *
 * Der Kern kennt weder Display noch Tasten noch Ton.  Gezeichnet wird in einen
 * 8-Bit-Indexpuffer (Gruppe*16 + Helligkeit); die Umsetzung nach RGB565 und
 * die Streifenausgabe macht render.c.
 */
#ifndef NOIZ_GAME_H
#define NOIZ_GAME_H

#include <stdint.h>

/* ---- Geometrie ----------------------------------------------------------
 * Die Vorlage rechnet in einem Feld von 320x480 und zeigt es 1:1.  Das Panel
 * ist 240x280.  Die Breite bleibt deshalb bei 320 -- damit stimmen saemtliche
 * Konstanten der Vorlage, vor allem die Geschossgeschwindigkeiten -- und wird
 * beim Zeichnen mit 3/4 verkleinert.  Die Hoehe folgt daraus: 280/0,75 = 373.
 * Ausweichraum in der Breite ist also identisch zur Vorlage, nur nach oben ist
 * weniger Vorwarnung.
 */
#define SCAN_W       320
#define SCAN_H       373
#define SCAN_W8      (SCAN_W << 8)
#define SCAN_H8      (SCAN_H << 8)

#define SCR_W        240
#define SCR_H        280
/* Feldkoordinate (ganzzahlig) -> Bildpunkt */
#define TO_SCR(v)    (((v) * 3) >> 2)

#define DIV          1024                  /* Winkelschritte je Vollkreis   */

/* ---- Grenzen (die Vorlage ist grosszuegiger, hier zaehlt der Speicher) --- */
#define FOE_MAX      640
#define SHOT_MAX     16
#define FRAG_MAX     160
#define BONUS_MAX    32
#define BOARD_MAX    64

/* ---- Farbindex: Gruppe*16 + Stufe (0 = weiss/Hintergrund, 15 = voll) ----- */
#define CL(g, l)     (uint8_t)(((g) << 4) | (l))
#define CL_GROUP(c)  ((c) >> 4)
#define CL_LEVEL(c)  ((c) & 15)

enum {                                     /* Farbgruppen, siehe palette.h  */
    CG_GREY = 0, CG_RED, CG_GREEN, CG_BLUE, CG_YELLOW, CG_MAGENTA, CG_CYAN,
    CG_ORANGE, CG_LIME, CG_AZURE, CG_VIOLET, CG_ROSE, CG_TEAL, CG_AMBER,
    CG_MINT, CG_INK                        /* CG_INK = dunkelste Tinte      */
};

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

/* ---- Spielphasen --------------------------------------------------------- */
enum { ST_TITLE, ST_GAME, ST_GAMEOVER, ST_CLEAR };

/* ---- Tonereignisse ------------------------------------------------------- */
#define EV_SHOT      0x01
#define EV_HIT       0x02
#define EV_FOE_DIE   0x04
#define EV_BOSS_DIE  0x08
#define EV_SHIP_DIE  0x10
#define EV_BONUS     0x20
#define EV_UI        0x40
#define EV_EXTEND    0x80

typedef struct {
    uint16_t ev;
    uint16_t bullets;                      /* fuer den Grundton             */
} noiz_audio_t;

/* ---- Zustand fuer Anzeige und Ton ---------------------------------------- */
typedef struct {
    uint8_t *fb;                           /* nachglimmende Ebene           */
    uint8_t *fb_top;                       /* scharfe Ebene, jedes Bild neu  */
    int      status;
    int      score, hiscore, left, bonus_score, scene, stage;
    int      endless, insane;
    int      n_foe, n_bullet, n_runner;
    int      ship_x, ship_y, ship_inv;   /* Bildpunkte, fuer Anzeige/Test */
    int      interval;                     /* Bildabstand in ms-Einheiten   */
    char     msg[24];
    char     msg2[28];
    noiz_audio_t audio;
} noiz_state_t;

extern noiz_state_t noiz;

void noiz_init(uint32_t seed, int hiscore);
void noiz_tick(uint16_t pad);
uint32_t noiz_rand(void);

/* Selbsttest der Buchhaltung; 0 = alles stimmig */
int noiz_selfcheck(void);

#endif /* NOIZ_GAME_H */
