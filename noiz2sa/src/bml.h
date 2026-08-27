/* bml.h -- BulletML in Bytecode statt XML.
 *
 * Die Vorlage laedt 73 XML-Dateien und wertet sie mit libBulletML aus (C++,
 * eigener Parser, dynamischer Speicher).  Auf dem Geraet aendern sich die
 * Muster nach dem Bauen nie mehr, also uebersetzt tools/mkbml.py sie vorab in
 * flache Tabellen; hier steht nur noch die Ausfuehrung.
 *
 * Alle Zahlen sind 16.16-Festkomma.  Winkel in Grad, Geschwindigkeit in
 * BulletML-Einheiten -- die Umrechnung in Spieleinheiten macht der Aufrufer.
 */
#ifndef BML_H
#define BML_H

#include <stdint.h>

#define BML_FP        16
#define BML_ONE       (1 << BML_FP)
#define bml_mul(a, b) ((int32_t)(((int64_t)(a) * (b)) >> BML_FP))
#define bml_div(a, b) ((b) ? (int32_t)(((int64_t)(a) << BML_FP) / (b)) : 0)

#define BML_NONE      0xFFFFu

/* Knotenbefehle -- muessen zu tools/mkbml.py passen */
enum {
    BOP_END, BOP_WAIT, BOP_FIRE, BOP_CHDIR, BOP_CHSPD,
    BOP_ACCEL, BOP_VANISH, BOP_REPEAT, BOP_ACTION
};

/* Richtungs-/Geschwindigkeitsarten */
enum { BT_AIM, BT_ABS, BT_REL, BT_SEQ };

/* RPN-Token */
enum { BE_CONST, BE_RANK, BE_RAND, BE_PARAM, BE_ADD, BE_SUB, BE_MUL, BE_DIV, BE_NEG };

typedef struct { uint8_t  op; int32_t val; } bml_tok_t;
typedef struct { uint16_t off; uint8_t n; } bml_expr_t;
typedef struct { uint8_t op, flags; uint16_t a, b, c; } bml_node_t;
typedef struct {
    uint16_t dir, spd, bullet, bpoff, fpoff;
    uint8_t  dtype, stype, bpn, fpn;
} bml_fire_t;
typedef struct {
    uint16_t dir, spd, action, poff;
    uint8_t  dtype, stype, pn;
} bml_bullet_t;
typedef struct { uint8_t type; uint16_t toff; uint8_t tn; char name[24]; } bml_brg_t;

extern const bml_tok_t    bml_tok[];
extern const bml_expr_t   bml_expr[];
extern const bml_node_t   bml_node[];
extern const uint16_t     bml_action[];
extern const bml_fire_t   bml_fire[];
extern const bml_bullet_t bml_bullet[];
extern const uint16_t     bml_pidx[];
extern const uint16_t     bml_top[];
extern const bml_brg_t    bml_brg[];
extern const uint16_t     bml_brg_num;

/* ---- Laufzeit ----------------------------------------------------------- */
#define BML_MAX_PARAM 4                    /* die Muster nutzen hoechstens $3 */
#define BML_MAX_DEPTH 6
#define BML_MAX_RUNNER 256

typedef struct {
    uint16_t node;                         /* naechster Knoten, absolut      */
    uint16_t start;                        /* Startknoten fuer Wiederholung  */
    uint16_t rep_left;
    uint8_t  pn;
    int32_t  param[BML_MAX_PARAM];
} bml_frame_t;

typedef struct {
    uint8_t  used, depth;
    int16_t  wait;
    int32_t  prev_dir, prev_spd;           /* fuer type="sequence"           */
    int16_t  chg_dir_turns, chg_spd_turns, acc_turns;
    int32_t  chg_dir_vel, chg_spd_vel, acc_x_vel, acc_y_vel;
    bml_frame_t st[BML_MAX_DEPTH];
} bml_runner_t;

/* Anbindung ans Spiel: der Aufrufer stellt diese Struktur bereit. */
typedef struct {
    int32_t dir;                           /* Grad, 16.16                    */
    int32_t spd;                           /* BulletML-Einheiten, 16.16      */
    int32_t vx, vy;                        /* accel-Anteil, BulletML         */
    int32_t rank;                          /* 0..1, 16.16                    */
    int32_t aim;                           /* Grad zum Spieler, 16.16        */
    uint8_t vanished;
} bml_state_t;

/* Vom Spiel bereitzustellen */
void     bml_fire_bullet(void *owner, int32_t dir, int32_t spd,
                         uint16_t action, const int32_t *param, uint8_t pn);
uint32_t bml_random(void);

void bml_runner_init(void);
int  bml_runner_alloc(uint16_t action, const int32_t *param, uint8_t pn);
void bml_runner_free(int r);
int  bml_runner_done(int r);
/* Fuehrt einen Tick aus.  Gibt 0 zurueck, wenn der Laeufer fertig ist. */
int  bml_runner_step(int r, bml_state_t *st, void *owner);

int  bml_runner_used(void);

#endif /* BML_H */
