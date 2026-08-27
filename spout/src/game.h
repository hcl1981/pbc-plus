/* game.h -- Spout fuer PicoBoy Color Plus, plattformunabhaengiger Spielkern.
 *
 * Portiert aus spout-1.4 von Nick White, basierend auf dem Original von Kuni.
 * MIT-Lizenz, siehe COPYING.
 *
 * Der Kern kennt weder Display noch Tasten noch Ton: er fuehrt einen Ringpuffer
 * aus Zellen, eine Liste von Overlay-Punkten (Schiff, Duesenstrahl) und einen
 * Satz Anzeigewerte.  Ausgabe macht die Plattform (src/pbc) bzw. der
 * Host-Test (tools/host_test.c).
 */
#ifndef SPOUT_GAME_H
#define SPOUT_GAME_H

#include <stdint.h>

/* ---- Geometrie ----------------------------------------------------------
 * Das Original rechnet in einem Ring aus 128x128 Zellen und zeigt davon
 * 128x78.  Das Panel des PBC+ ist hochkant (240x280), also wird der sichtbare
 * Ausschnitt hoeher: 120 Spalten x 120 Zeilen, bei Zoom 2 exakt 240x240 px.
 * Die Spielfeldbreite (Spalten 4..123) ist damit identisch zum Original, nur
 * die Sichtweite nach oben und unten waechst.  Der Ring muss dafuer 256 Zeilen
 * haben (sichtbar 120 + Vorlauf fuer die Hoehlenerzeugung).
 */
#define CELL_W      128                    /* Zellen je Ringzeile (Stride) */
#define RING_H      256                    /* Zeilen im Ring               */
#define RING_CELLS  (CELL_W * RING_H)
#define RING_MASK   (RING_CELLS - 1)
#define ROW_MASK    (RING_H - 1)

#define VIEW_X0     4                      /* erste sichtbare Spalte       */
#define VIEW_W      120                    /* sichtbare Spalten            */
#define VIEW_H      120                    /* sichtbare Zeilen             */
#define SCROLL_Y    60                     /* ab hier scrollt die Welt mit */

#define MAX_GRAIN   500
#define FRAMERATE   50

/* ---- Zellcodierung (wie im Original) ------------------------------------
 *   Bit 0..1  Farbindex 0..3 (0 = leer/Hintergrund, 3 = schwarz)
 *   Bit 2     Zelle ist ein Korn
 *   Bit 6..7  Trefferzaehler: gesetzt = zerstoerbar, je Treffer -0x40
 * Feste Werte: 0x00 leer, 0x0b Randspalte, 0x13 Boden (unzerstoerbar),
 *              0xd2 Fels (3 Treffer), 0xd3 Sperrmauer (3 Treffer)
 */
#define CELL_EMPTY  0x00
#define CELL_BORDER 0x0b
#define CELL_FLOOR  0x13
#define CELL_ROCK   0xd2
#define CELL_BAR    0xd3
#define CELL_GRAIN  0x04

/* ---- Tasten -------------------------------------------------------------- */
#define PAD_RI 0x0001
#define PAD_LF 0x0002
#define PAD_DN 0x0004
#define PAD_UP 0x0008
#define PAD_B  0x0010
#define PAD_A  0x0020
#define PAD_C  0x0040                      /* Joystick-Mitte = Pause       */
#define TRG_RI 0x0100
#define TRG_LF 0x0200
#define TRG_DN 0x0400
#define TRG_UP 0x0800
#define TRG_B  0x1000
#define TRG_A  0x2000
#define TRG_C  0x4000

/* ---- Spielphasen (Zahlenwerte wie upstream: Bit1 = im Spiel, Bit0 = init) */
enum {
    PH_TITLE_INIT = 0,
    PH_TITLE      = 1,
    PH_GAME_INIT  = 2,
    PH_GAME       = 3,
    PH_PAUSE      = 4
};

/* ---- Tonereignisse eines Ticks ------------------------------------------- */
#define SPOUT_EV_BONUS   0x01              /* Zeitbonus alle 128 Hoehe      */
#define SPOUT_EV_OVER    0x02
#define SPOUT_EV_START   0x04
#define SPOUT_EV_TICK    0x08              /* Sekundenpiep bei knapper Zeit */
#define SPOUT_EV_UI      0x10              /* Pause/Menue                   */
#define SPOUT_EV_BARRIER 0x20              /* Sperrmauer erscheint          */

typedef struct {
    uint8_t  thrust;                       /* Schub aktiv                   */
    uint16_t hits;                         /* Kornaufschlaege im Tick       */
    uint16_t breaks;                       /* zerstoerte Wandzellen im Tick */
    uint8_t  ev;                           /* SPOUT_EV_*                    */
} spout_audio_t;

/* ---- Titelbild: Textbloecke, die durchs Bild wandern ---------------------
 * Sie liegen NICHT im Zellring, sondern werden vom Renderer als Overlay mit
 * voller Kantenglaettung gezeichnet -- eine Zelle sind zwei Bildpunkte, feiner
 * wird Text dort nicht.  Preis: auf den Buchstaben haeuft sich kein Sand mehr.
 */
#define SPOUT_MAX_BANNER 4
typedef struct {
    int16_t row;                           /* Ringzeile der Blockoberkante  */
    uint8_t slot;
    uint8_t used;
} spout_banner_t;

/* ---- Overlay ------------------------------------------------------------
 * Das Schiff sitzt im Zellraster -- es markiert die Zelle, in der es stirbt,
 * und soll deshalb hart bleiben.  Die Zielhilfe dagegen ist reine Anzeige und
 * wird mit Subpixelgenauigkeit gefuehrt: die Bahn liegt ohnehin in 1/256
 * Zellen vor, das Runden auf ganze Zellen hat die Aufloesung nur weggeworfen.
 */
typedef struct { uint8_t x, y, c; } spout_pt_t;     /* Zellraster           */
typedef struct { uint16_t x, y; } spout_dot_t;      /* 1/64 Bildpunkt       */
#define SPOUT_MAX_OVL 16
#define SPOUT_MAX_DOT 12

/* ---- Zustand fuer Anzeige und Ton ---------------------------------------- */
typedef struct {
    uint8_t  *cells;                       /* RING_CELLS Bytes              */
    int       disp_pos;                    /* oberste sichtbare Ringzeile   */
    spout_pt_t ovl[SPOUT_MAX_OVL];
    int       n_ovl;
    spout_dot_t dot[SPOUT_MAX_DOT];
    int       n_dot;

    int       phase;
    int       gameover;
    int       score, dispscore, height, timeleft;
    int       hiscore[2];                  /* [0] Punkte, [1] Hoehe         */
    int       n_grain;
    int       ship_x, ship_y, ship_r;      /* Zellkoordinaten / Winkel 0..1023 */
    char      msg[16];                     /* zentrierte Meldung, "" = keine */
    char      msg2[24];                    /* kleine Zeile darunter          */
    spout_banner_t banner[SPOUT_MAX_BANNER];
    spout_audio_t audio;
} spout_state_t;

extern spout_state_t spout;

void spout_init(uint32_t seed, const int hiscore[2]);
void spout_tick(uint16_t pad);
uint32_t spout_rand(void);
void     spout_reseed(uint32_t seed);

/* Selbsttest: prueft die Kornbuchhaltung (Zellbit, v2g, Benutzungsliste).
 * Gibt 0 zurueck, wenn alles stimmt, sonst einen Fehlercode. */
int spout_selfcheck(void);

#endif /* SPOUT_GAME_H */
