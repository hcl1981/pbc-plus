/* game.c -- Noiz2sa, Spielkern.
 *
 * Portiert aus noiz2sa 0.52 von Kenta Cho (BSD, siehe COPYING).  Uebernommen
 * sind Bewegung, Trefferpruefung, Punktevergabe, Bonuslogik und der Aufbau der
 * Szenen; die Anpassungen fuer den PicoBoy Color Plus stehen in doc/PORT.md.
 */
#include <string.h>
#include "game.h"
#include "bml.h"
#include "tables.h"

#define NOT_EXIST 0
#define SPC_FOE   1
#define SPC_BOSSB 2
#define SPC_ABUL  3
#define SPC_BUL   4

#define BOSS_TYPE 3
#define SPD_RATE  800                      /* wie COMMAND_SCREEN_SPD_RATE   */
#define VEL_RATE  800

noiz_state_t noiz;

/* Zwei Ebenen wie in der Vorlage: was ins Bild "eingebrannt" wird und
 * nachglimmt (Hintergrund, Gegner, Spuren, Splitter, Bonus) und was scharf
 * obendrauf liegt (Schuesse, Schiff, Geschosse).  Die Vorlage erreicht das
 * ueber zwei Ebenen plus Mischtabelle; hier genuegen zwei Indexpuffer. */
static uint8_t fb_glow[SCR_W * SCR_H];
static uint8_t fb_top[SCR_W * SCR_H];
static uint8_t *dtgt = fb_glow;            /* aktuelles Zeichenziel         */
static uint32_t rng = 0x1234567u;

uint32_t noiz_rand(void)
{
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    return rng;
}
uint32_t bml_random(void) { return noiz_rand(); }

static int randN(int n)  { return n > 0 ? (int)(noiz_rand() % (uint32_t)n) : 0; }
static int randNS(int n) { return n > 0 ? randN(n << 1) - n : 0; }

/* ---- Zeichnen in den Indexpuffer ---------------------------------------
 * Geschrieben wird nur, wenn die neue Helligkeit mindestens so gross ist wie
 * die vorhandene.  Das ahmt die additive Mischung der Vorlage nach, ohne je
 * Bildpunkt eine 64-KB-Tabelle zu befragen. */
static inline void put(int x, int y, uint8_t c)
{
    if ((unsigned)x < SCR_W && (unsigned)y < SCR_H) {
        uint8_t *p = &dtgt[y * SCR_W + x];
        if ((c & 15) >= (*p & 15))
            *p = c;
    }
}

static void fill_row(int x0, int x1, int y, uint8_t c)
{
    uint8_t *p;
    if ((unsigned)y >= SCR_H) return;
    if (x0 < 0) x0 = 0;
    if (x1 > SCR_W) x1 = SCR_W;
    p = &dtgt[y * SCR_W];
    for (; x0 < x1; x0++)
        if ((c & 15) >= (p[x0] & 15))
            p[x0] = c;
}

/* Kasten mit Rand: innen c1, Rand c2 -- wie drawBox der Vorlage. */
static void draw_box(int x, int y, int w, int h, uint8_t c1, uint8_t c2)
{
    int i;
    x -= w >> 1; y -= h >> 1;
    if (w <= 1 || h <= 1) return;
    if (x >= SCR_W || y >= SCR_H || x + w <= 0 || y + h <= 0) return;
    fill_row(x, x + w, y, c2);
    for (i = 1; i < h - 1; i++) {
        put(x, y + i, c2);
        fill_row(x + 1, x + w - 1, y + i, c1);
        put(x + w - 1, y + i, c2);
    }
    fill_row(x, x + w, y + h - 1, c2);
}

static void draw_line(int x1, int y1, int x2, int y2, uint8_t c)
{
    int dx = x2 - x1, dy = y2 - y1, n, i;
    int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    n = (ax > ay ? ax : ay);
    if (n == 0) { put(x1, y1, c); return; }
    if (n > 128) n = 128;
    for (i = 0; i <= n; i++)
        put(x1 + dx * i / n, y1 + dy * i / n, c);
}

/* Dicke Linie: die Geschossspur der Vorlage.  Statt echter Polygonfuellung
 * werden parallele Linien gezogen -- bei 4..5 px Breite nicht zu unterscheiden
 * und deutlich billiger. */
static void draw_thick_line(int x1, int y1, int x2, int y2,
                            uint8_t c1, uint8_t c2, int w)
{
    int dx = x2 - x1, dy = y2 - y1, i;
    int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    int ox, oy, h = w >> 1;
    if (ax > ay) { ox = 0; oy = 1; } else { ox = 1; oy = 0; }
    for (i = -h; i <= h; i++)
        draw_line(x1 + ox * i, y1 + oy * i, x2 + ox * i, y2 + oy * i,
                  (i == -h || i == h) ? c2 : c1);
}

/* ---- Winkel ------------------------------------------------------------- */
static int get_deg(int x, int y)
{
    int tx, ty, f, od, tn;
    if (x == 0 && y == 0) return 0;
    if (x < 0) {
        tx = -x;
        if (y < 0) {
            ty = -y;
            if (tx > ty) { f = 1;  od = DIV * 3 / 4; tn = ty * TAN_TABLE_SIZE / tx; }
            else         { f = -1; od = DIV;         tn = tx * TAN_TABLE_SIZE / ty; }
        } else {
            ty = y;
            if (tx > ty) { f = -1; od = DIV * 3 / 4; tn = ty * TAN_TABLE_SIZE / tx; }
            else         { f = 1;  od = DIV / 2;     tn = tx * TAN_TABLE_SIZE / ty; }
        }
    } else {
        tx = x;
        if (y < 0) {
            ty = -y;
            if (tx > ty) { f = -1; od = DIV / 4; tn = ty * TAN_TABLE_SIZE / tx; }
            else         { f = 1;  od = 0;       tn = tx * TAN_TABLE_SIZE / ty; }
        } else {
            ty = y;
            if (tx > ty) { f = 1;  od = DIV / 4; tn = ty * TAN_TABLE_SIZE / tx; }
            else         { f = -1; od = DIV / 2; tn = tx * TAN_TABLE_SIZE / ty; }
        }
    }
    if (tn > TAN_TABLE_SIZE) tn = TAN_TABLE_SIZE;
    return (od + tantbl[tn] * f) & (DIV - 1);
}

static int get_dist(int x, int y)
{
    if (x < 0) x = -x;
    if (y < 0) y = -y;
    return (x > y) ? x + (y >> 1) : y + (x >> 1);
}

/* Grad (16.16) -> Winkelschritt 0..1023 */
static inline int deg_to_div(int32_t deg)
{
    return (int)(((int64_t)deg * 186413) >> 24) & (DIV - 1);
}
static inline int32_t div_to_deg(int d)
{
    return (int32_t)(((int64_t)d * (360 << 16)) / DIV);
}

/* ---- Gegner und Geschosse ----------------------------------------------- */
typedef struct {
    int32_t x, y, px, py, sx, sy;
    int32_t mvx, mvy;
    int32_t dir, spd;                      /* BulletML: Grad / Einheit, 16.16 */
    int32_t bvx, bvy;                      /* accel-Anteil, BulletML-Einheiten */
    int32_t rank;
    int16_t shield, cnt;
    uint8_t spc, type, color, hit;
    int16_t runner[3];
    uint8_t nrun, brg;
} Foe;

static Foe foe[FOE_MAX];
static int foe_cnt, en_num[4];
static int foe_idx;

static void foe_drop_runners(Foe *fe)
{
    int i;
    for (i = 0; i < fe->nrun; i++)
        if (fe->runner[i] >= 0) {
            bml_runner_free(fe->runner[i]);
            fe->runner[i] = -1;
        }
    fe->nrun = 0;
}

static void remove_foe(Foe *fe)
{
    if (fe->spc == SPC_FOE) {
        foe_cnt--;
        en_num[fe->type]--;
    }
    fe->spc = NOT_EXIST;
    foe_drop_runners(fe);
}

static void init_foes(void)
{
    int i;
    for (i = 0; i < FOE_MAX; i++) {
        foe[i].spc = NOT_EXIST;
        foe[i].nrun = 0;
    }
    bml_runner_init();
    foe_cnt = 0;
    for (i = 0; i < 4; i++) en_num[i] = 0;
}

static Foe *next_foe(void)
{
    int i;
    for (i = 0; i < FOE_MAX; i++) {
        foe_idx--;
        if (foe_idx < 0) foe_idx = FOE_MAX - 1;
        if (foe[foe_idx].spc == NOT_EXIST)
            return &foe[foe_idx];
    }
    return 0;
}

static Foe *add_foe(int x, int y, int32_t rank, int d, int32_t spd,
                    int type, int shield, int brg)
{
    Foe *fe = next_foe();
    int i;
    if (!fe) return 0;
    memset(fe, 0, sizeof *fe);
    fe->x = fe->px = fe->sx = x;
    fe->y = fe->py = fe->sy = y;
    fe->rank = rank;
    fe->dir = div_to_deg(d);
    fe->spd = spd;
    fe->spc = SPC_FOE;
    fe->type = (uint8_t)type;
    fe->shield = (int16_t)shield;
    fe->brg = (uint8_t)brg;
    fe->nrun = 0;
    for (i = 0; i < bml_brg[brg].tn && fe->nrun < 3; i++) {
        int r = bml_runner_alloc(bml_top[bml_brg[brg].toff + i], 0, 0);
        if (r >= 0) fe->runner[fe->nrun++] = (int16_t)r;
    }
    foe_cnt++;
    en_num[type]++;
    return fe;
}

/* Wird von der BulletML-Maschine gerufen. */
void bml_fire_bullet(void *owner, int32_t dir, int32_t spd,
                     uint16_t action, const int32_t *param, uint8_t pn)
{
    Foe *src = (Foe *)owner, *fe;
    if (!src) return;
    fe = next_foe();
    if (!fe) return;
    memset(fe, 0, sizeof *fe);
    fe->x = fe->px = fe->sx = src->x;
    fe->y = fe->py = fe->sy = src->y;
    fe->rank = src->rank;
    fe->dir = dir;
    fe->spd = spd;
    fe->color = (uint8_t)(src->color + 1);
    fe->nrun = 0;
    if (action != BML_NONE) {
        int r = bml_runner_alloc(action, param, pn);
        if (r >= 0) {
            fe->runner[0] = (int16_t)r;
            fe->nrun = 1;
            fe->spc = SPC_ABUL;
            return;
        }
    }
    fe->spc = SPC_BUL;                     /* ohne Laeufer: einfaches Geschoss */
}

/* ---- Spieler ------------------------------------------------------------ */
#define SHIP_SPEED       1280
#define SHIP_SLOW_SPEED  640
#define SHIP_SLOW_DOWN   64
#define SHIP_INV_BASE    240
#define SHOT_INTERVAL    3
#define SHIP_EDGE        (1024 * 3)
#define SHIP_HIT_WIDTH   (512 * 512)

static struct { int32_t x, y; int cnt, shot_cnt, speed, inv; } ship;

static const int16_t ship_mv[8][2] = {
    {0, -256}, {181, -181}, {256, 0}, {181, 181},
    {0, 256}, {-181, 181}, {-256, 0}, {-181, -181}
};

static void init_ship(void)
{
    ship.x = (SCAN_W / 2) << 8;
    ship.y = (SCAN_H / 5 * 4) << 8;
    ship.cnt = 0;
    ship.shot_cnt = -1;
    ship.speed = SHIP_SPEED;
    ship.inv = SHIP_INV_BASE * (100 - noiz.scene) / 100;
    if (ship.inv < 0) ship.inv = 0;
}

/* ---- Schuesse ----------------------------------------------------------- */
#define SHOT_SPEED  4096
#define SHOT_WIDTH  8
#define SHOT_HEIGHT 24
#define SHOT_SCAN_H (SHOT_HEIGHT * 256 / 2)

static struct { int32_t x, y; int cnt; } shot[SHOT_MAX];

static void init_shots(void) { int i; for (i = 0; i < SHOT_MAX; i++) shot[i].cnt = -1; }

static void add_shot(void)
{
    int i;
    for (i = 0; i < SHOT_MAX; i++)
        if (shot[i].cnt < 0) {
            shot[i].x = ship.x;
            shot[i].y = ship.y;
            shot[i].cnt = 0;
            noiz.audio.ev |= EV_SHOT;
            return;
        }
}

static void move_shots(void)
{
    int i;
    for (i = 0; i < SHOT_MAX; i++) {
        if (shot[i].cnt < 0) continue;
        shot[i].y -= SHOT_SPEED;
        shot[i].cnt++;
        if (shot[i].y < 0) shot[i].cnt = -1;
    }
}

/* ---- Splitter ----------------------------------------------------------- */
static struct { int32_t x, y, vx, vy; int16_t w, h, cnt; uint8_t spc; } frag[FRAG_MAX];
static int frag_idx;

static void init_frags(void) { int i; for (i = 0; i < FRAG_MAX; i++) frag[i].cnt = 0; }

static void add_frag(int x, int y, int vx, int vy, int spc, int size)
{
    int i;
    for (i = 0; i < FRAG_MAX; i++) {
        frag_idx--;
        if (frag_idx < 0) frag_idx = FRAG_MAX - 1;
        if (frag[frag_idx].cnt <= 0) break;
    }
    if (i >= FRAG_MAX) return;
    i = frag_idx;
    frag[i].x = x; frag[i].y = y;
    frag[i].vx = vx; frag[i].vy = vy;
    frag[i].spc = (uint8_t)spc;
    switch (spc) {
    case 0: frag[i].w = (int16_t)(4 + randN(7)); frag[i].h = frag[i].w;
            frag[i].cnt = (int16_t)(4 + randN(8)); break;
    case 1: frag[i].w = (int16_t)(size * 4 + randN(size * 2)); frag[i].h = frag[i].w;
            frag[i].cnt = (int16_t)(12 + randN(12)); break;
    default: frag[i].w = frag[i].h = 3;
            frag[i].cnt = (int16_t)(10 + randN(4)); break;
    }
}

static void add_shot_frag(int32_t x, int32_t y)
{
    add_frag(TO_SCR(x >> 8), TO_SCR(y >> 8), randNS(6), -12 + randNS(6), 0, 0);
}

static void add_enemy_frag(int32_t x, int32_t y, int mx, int my, int type)
{
    int i, sx = TO_SCR(x >> 8), sy = TO_SCR(y >> 8);
    int cmx = TO_SCR(mx >> 8), cmy = TO_SCR(my >> 8);
    type = type * 2 + 1;
    for (i = 0; i < type + randN(type * 2); i++)
        add_frag(sx, sy, randNS(12), randNS(12), 0, 0);
    for (i = 0; i < type * 2 + randN(type); i++)
        add_frag(sx, sy, cmx + randNS(3), cmy + randNS(3), 1, 2 + type);
}

static void add_ship_frag(void)
{
    int i, sx = TO_SCR(ship.x >> 8), sy = TO_SCR(ship.y >> 8);
    for (i = 0; i < 40; i++) add_frag(sx, sy, randNS(18), randNS(18), 0, 0);
    for (i = 0; i < 24; i++) add_frag(sx, sy, randNS(3), randNS(3), 1, 1 + randN(5));
}

static void add_clear_frag(int32_t x, int32_t y, int32_t vx, int32_t vy)
{
    add_frag(TO_SCR(x >> 8), TO_SCR(y >> 8), TO_SCR(vx >> 8), TO_SCR(vy >> 8), 2, 0);
}

static void move_frags(void)
{
    int i;
    for (i = 0; i < FRAG_MAX; i++) {
        if (frag[i].cnt <= 0) continue;
        frag[i].x += frag[i].vx;
        frag[i].y += frag[i].vy;
        frag[i].cnt--;
    }
}

/* ---- Bonus -------------------------------------------------------------- */
#define BONUS_SPEED    400
#define BONUS_INHALE   24000
#define BONUS_ACQUIRE  8000
#define BONUS_DRAW_W   6

static struct { int32_t x, y, vx, vy; int cnt, down; } bonus[BONUS_MAX];
static int bonus_idx;

static void show_score(void) { }

static void add_score(int s)
{
    int prev = noiz.score;
    noiz.score += s;
    if (prev < 200000 && noiz.score >= 200000) { noiz.left++; noiz.audio.ev |= EV_EXTEND; }
    else if (prev / 500000 != noiz.score / 500000 && noiz.score >= 500000) {
        noiz.left++; noiz.audio.ev |= EV_EXTEND;
    }
    if (noiz.score > noiz.hiscore) noiz.hiscore = noiz.score;
}

static void reset_bonus_score(void) { noiz.bonus_score = 10; show_score(); }

static void init_bonuses(void)
{
    int i;
    for (i = 0; i < BONUS_MAX; i++) bonus[i].cnt = -1;
    reset_bonus_score();
}

static void add_bonus(int32_t x, int32_t y, int32_t vx, int32_t vy)
{
    int i;
    for (i = 0; i < BONUS_MAX; i++) {
        bonus_idx--;
        if (bonus_idx < 0) bonus_idx = BONUS_MAX - 1;
        if (bonus[bonus_idx].cnt < 0) break;
    }
    if (i >= BONUS_MAX) return;
    i = bonus_idx;
    bonus[i].x = x; bonus[i].y = y;
    bonus[i].vx = vx; bonus[i].vy = vy;
    bonus[i].cnt = 0;
    bonus[i].down = 1;
}

static void move_bonuses(void)
{
    int i, d;
    for (i = 0; i < BONUS_MAX; i++) {
        if (bonus[i].cnt < 0) continue;
        bonus[i].x += bonus[i].vx;
        bonus[i].y += bonus[i].vy;
        bonus[i].vx -= bonus[i].vx >> 6;
        if (bonus[i].x < SCAN_W8 / 8) {
            bonus[i].x = SCAN_W8 / 8;
            if (bonus[i].vx < 0) bonus[i].vx = -bonus[i].vx;
        } else if (bonus[i].x > SCAN_W8 / 8 * 7) {
            bonus[i].x = SCAN_W8 / 8 * 7;
            if (bonus[i].vx > 0) bonus[i].vx = -bonus[i].vx;
        }
        if (bonus[i].down) {
            bonus[i].vy += (BONUS_SPEED - bonus[i].vy) >> 6;
            if (bonus[i].y > SCAN_H8) {
                bonus[i].down = 0;
                bonus[i].y = SCAN_H8;
                bonus[i].vy = -bonus[i].vy;
            }
        } else {
            bonus[i].vy += (-BONUS_SPEED - bonus[i].vy) >> 6;
            if (bonus[i].y < 0) {          /* verpasst: Wertung faellt zurueck */
                noiz.bonus_score = noiz.bonus_score / 20 * 10;
                if (noiz.bonus_score < 10) noiz.bonus_score = 10;
                bonus[i].cnt = -1;
                continue;
            }
        }
        d = get_dist((int)((ship.x - bonus[i].x) >> 4), (int)((ship.y - bonus[i].y) >> 4)) << 4;
        if (d < BONUS_ACQUIRE) {
            add_score(noiz.bonus_score);
            if (noiz.bonus_score < 1000) noiz.bonus_score += 10;
            noiz.audio.ev |= EV_BONUS;
            bonus[i].cnt = -1;
            continue;
        } else if (d < BONUS_INHALE) {
            bonus[i].vx += (int32_t)(((int64_t)(ship.x - bonus[i].x) * (BONUS_INHALE - d)) >> 20);
            bonus[i].vy += (int32_t)(((int64_t)(ship.y - bonus[i].y) * (BONUS_INHALE - d)) >> 20);
        }
        bonus[i].cnt++;
    }
}

/* ---- Hintergrund --------------------------------------------------------
 * Die Vorlage legt bis zu 128 Bretter an und zeichnet jedes bis zu 16-mal
 * gekachelt -- ueber 2000 Kaesten je Bild.  Das ist hier nicht zu bezahlen.
 * Uebernommen ist das Prinzip (Parallaxe ueber einen z-Teiler), die Anzahl ist
 * auf 64 Bretter und 2x2 Kachelungen gedeckelt. */
static struct { int32_t x, y, z; int16_t w, h; } board[BOARD_MAX];
static int board_n, board_mx, board_my, board_rep, board_repn;

static void add_board(int x, int y, int z, int w, int h)
{
    if (board_n >= BOARD_MAX || z <= 0) return;
    board[board_n].x = x; board[board_n].y = y; board[board_n].z = z;
    board[board_n].w = (int16_t)TO_SCR(w / z);
    board[board_n].h = (int16_t)TO_SCR(h / z);
    board_n++;
}

static void set_background(int bn)
{
    int i, j;
    board_n = 0;
    board_rep = 65536;
    board_repn = 2;
    switch (bn % 6) {
    case 0:
        add_board(9000, 9000, 500, 25000, 25000);
        for (i = 0; i < 4; i++)
            for (j = 0; j < 4; j++)
                if (i > 1 || j > 1)
                    add_board(i * 16384, j * 16384, 500,
                              10000 + (i * 12345) % 3000, 10000 + (j * 54321) % 3000);
        for (j = 0; j < 6; j++)
            for (i = 0; i < 3; i++)
                add_board(0, i * 16384, 500 - j * 50, 20000 - j * 1000, 12000 - j * 500);
        board_mx = 40; board_my = 300;
        break;
    case 1:
        add_board(12000, 12000, 400, 48000, 48000);
        add_board(12000, 44000, 400, 48000, 8000);
        add_board(44000, 12000, 400, 8000, 48000);
        for (i = 0; i < 12; i++) {
            add_board(0, 0, 400 - i * 10, 16000, 16000);
            if (i < 6) add_board(9600, 16000, 400 - i * 10, 40000, 16000);
        }
        board_mx = 128; board_my = 512;
        break;
    case 2:
        for (i = 0; i < 14; i++) {
            add_board(7000 + i * 3000, 0, 1600 - i * 100, 24000, 5000);
            add_board(-7000 - i * 3000, 0, 1600 - i * 100, 24000, 5000);
        }
        board_mx = 0; board_my = 1200;
        board_repn = 3;
        break;
    case 3:
        add_board(9000, 9000, 500, 30000, 30000);
        for (i = 0; i < 4; i++)
            for (j = 0; j < 4; j++)
                if (i > 1 || j > 1)
                    add_board(i * 16384, j * 16384, 500,
                              12000 + (i * 12345) % 3000, 12000 + (j * 54321) % 3000);
        add_board(9000, 9000, 480, 20000, 20000);
        add_board(9000, 9000, 450, 20000, 20000);
        add_board(32768, 40000, 420, 65536, 5000);
        add_board(30000, 32768, 370, 4800, 65536);
        board_mx = 10; board_my = 100;
        break;
    case 4:
        add_board(32000, 12000, 160, 48000, 48000);
        add_board(32000, 44000, 160, 48000, 8000);
        for (i = 0; i < 12; i++) {
            add_board(20000, 0, 160 - i * 10, 16000, 16000);
            if (i < 6) add_board(29600, 16000, 160 - i * 10, 40000, 16000);
        }
        board_mx = 0; board_my = 128;
        break;
    default: {
        int k, jj;
        for (k = 0; k < 3; k++) {
            jj = 0;
            for (i = 0; i < 14; i++) {
                add_board(jj, i * 4096, 200 - k * 10, 16000, 4096);
                add_board(jj + 16000 - jj * 2, i * 4096, 200 - k * 10, 16000, 4096);
                if (i < 4) jj += 2000;
                else if (i < 6) jj -= 3500;
                else if (i < 12) jj += 1500;
                else jj -= 2000;
            }
        }
        board_mx = -10; board_my = 25;
        break;
    }
    }
}

static void move_background(void)
{
    int i;
    for (i = 0; i < board_n; i++) {
        board[i].x = (board[i].x + board_mx) & (board_rep - 1);
        board[i].y = (board[i].y + board_my) & (board_rep - 1);
    }
}

static void draw_background(void)
{
    int i, rx, ry, ox, oy, os = -board_rep * (board_repn / 2);
    for (i = 0; i < board_n; i++) {
        ox = os;
        for (rx = 0; rx < board_repn; rx++, ox += board_rep) {
            oy = os;
            for (ry = 0; ry < board_repn; ry++, oy += board_rep) {
                int x = TO_SCR((board[i].x + ox) / board[i].z) + SCR_W / 2;
                int y = TO_SCR((board[i].y + oy) / board[i].z) + SCR_H / 2;
                if (x + board[i].w < 0 || x - board[i].w >= SCR_W ||
                    y + board[i].h < 0 || y - board[i].h >= SCR_H)
                    continue;
                draw_box(x, y, board[i].w, board[i].h, CL(CG_GREY, 2), CL(CG_GREY, 4));
            }
        }
    }
}

/* ---- Gegner bewegen ----------------------------------------------------- */
#define BULLET_WIPE_WIDTH 7200
#define DEFAULT_SLOWDOWN_BULLETS 180
#define EASY_SLOWDOWN_BULLETS    120
#define HARD_SLOWDOWN_BULLETS    260
#define INTERVAL_BASE 16

static int slowdown_bullets = DEFAULT_SLOWDOWN_BULLETS;
static const int16_t foe_size[4] = {30, 40, 56, 96};
static const int32_t enemy_score[4] = {500, 1000, 5000, 50000};

static int32_t foe_scan_size(int t) { return (int32_t)foe_size[t] * 256 * 3 / 4; }

static void wipe_bullets(int32_t x, int32_t y, int width)
{
    int i;
    for (i = 0; i < FOE_MAX; i++) {
        if (foe[i].spc != SPC_ABUL && foe[i].spc != SPC_BUL) continue;
        if (get_dist((int)((x - foe[i].x) >> 4), (int)((y - foe[i].y) >> 4)) << 4 < width) {
            add_bonus(foe[i].x, foe[i].y, foe[i].mvx, foe[i].mvy);
            remove_foe(&foe[i]);
        }
    }
}

static void destroy_ship(void);
static void boss_destroyed(void);

static void move_foes(void)
{
    int i, j, live = 0;
    int32_t aim_deg;

    for (i = 0; i < FOE_MAX; i++) {
        Foe *fe = &foe[i];
        int32_t mx, my;
        int wl;

        if (fe->spc == NOT_EXIST) continue;

        if (fe->nrun) {
            bml_state_t st;
            int r, alive = 0;
            aim_deg = div_to_deg(get_deg((int)((ship.x - fe->x) >> 8),
                                         (int)((ship.y - fe->y) >> 8)));
            st.dir = fe->dir; st.spd = fe->spd;
            st.vx = fe->bvx; st.vy = fe->bvy;
            st.rank = fe->rank; st.aim = aim_deg; st.vanished = 0;
            for (r = 0; r < fe->nrun; r++) {
                if (fe->runner[r] < 0) continue;
                if (bml_runner_done(fe->runner[r])) {
                    /* Der Boss startet sein Muster von vorn, alles andere
                     * bleibt einfach stehen -- wie in der Vorlage. */
                    if (fe->type == BOSS_TYPE && fe->spc == SPC_FOE) {
                        bml_runner_free(fe->runner[r]);
                        fe->runner[r] = (int16_t)bml_runner_alloc(
                            bml_top[bml_brg[fe->brg].toff +
                                    (r % bml_brg[fe->brg].tn)], 0, 0);
                    } else {
                        bml_runner_free(fe->runner[r]);
                        fe->runner[r] = -1;
                        continue;
                    }
                }
                if (fe->runner[r] < 0) continue;
                bml_runner_step(fe->runner[r], &st, fe);
                alive = 1;
            }
            fe->dir = st.dir; fe->spd = st.spd;
            fe->bvx = st.vx; fe->bvy = st.vy;
            if (st.vanished) {
                if (fe->type != BOSS_TYPE || fe->spc != SPC_FOE) {
                    remove_foe(fe);
                    continue;
                }
            }
            if (!alive) foe_drop_runners(fe);
        }

        {
            int d = deg_to_div(fe->dir);
            int32_t sg = (int32_t)(((int64_t)fe->spd * SPD_RATE) >> 16);
            mx =  ((int32_t)sctbl[d] * sg) >> 8;
            my = -((int32_t)sctbl[d + DIV / 4] * sg) >> 8;
            mx += (int32_t)(((int64_t)fe->bvx * VEL_RATE) >> 16);
            my += (int32_t)(((int64_t)fe->bvy * VEL_RATE) >> 16);
        }
        fe->x += mx; fe->y += my;
        fe->mvx = mx; fe->mvy = my;
        /* Laenge der Geschossspur waechst in den ersten Bildern.  Die Vorlage
         * schiebt hier links; bei negativer Bewegung ist das in C undefiniert,
         * deshalb multipliziert. */
        wl = (fe->cnt < 4) ? 1 : (fe->cnt < 8 ? 2 : 4);
        fe->px = fe->x - mx * wl;
        fe->py = fe->y - my * wl;
        fe->cnt++;

        if (fe->spc == SPC_FOE) {
            fe->hit = 0;
            for (j = 0; j < SHOT_MAX; j++) {
                int32_t sz;
                if (shot[j].cnt < 0) continue;
                sz = foe_scan_size(fe->type);
                if (((fe->x - shot[j].x) < sz && (shot[j].x - fe->x) < sz) &&
                    ((fe->y - shot[j].y) < sz + SHOT_SCAN_H &&
                     (shot[j].y - fe->y) < sz + SHOT_SCAN_H)) {
                    shot[j].cnt = -1;
                    fe->shield--;
                    fe->hit = 1;
                    add_shot_frag(shot[j].x, shot[j].y);
                    if (fe->shield <= 0) {
                        add_score(enemy_score[fe->type]);
                        wipe_bullets(fe->x, fe->y, BULLET_WIPE_WIDTH * (fe->type + 1));
                        add_enemy_frag(fe->x, fe->y, mx, my, fe->type);
                        if (fe->type == BOSS_TYPE) {
                            boss_destroyed();
                            noiz.audio.ev |= EV_BOSS_DIE;
                        } else {
                            noiz.audio.ev |= EV_FOE_DIE;
                        }
                        remove_foe(fe);
                        break;
                    }
                    noiz.audio.ev |= EV_HIT;
                }
            }
            if (fe->spc == NOT_EXIST) continue;
        } else if (noiz.status == ST_GAME && ship.inv <= 0) {
            /* Strecken-Punkt-Abstand: die Spur seit dem letzten Bild wird als
             * Strecke geprueft, damit schnelle Geschosse nicht durchrutschen. */
            int64_t bx = fe->x - fe->px, by = fe->y - fe->py;
            int64_t inaa = bx * bx + by * by;
            if (inaa > 65536) {
                int64_t ox = ship.x - fe->px, oy = ship.y - fe->py;
                int64_t inab = bx * ox + by * oy;
                if (inab > 0 && inab < inaa) {
                    int64_t hd = (ox * ox + oy * oy) - (inab / inaa) * (inab / inaa);
                    hd = (ox * ox + oy * oy) - ((inab * inab) / inaa);
                    if (hd >= 0 && hd < SHIP_HIT_WIDTH)
                        destroy_ship();
                }
            }
        }

        if (fe->px < 0 || fe->px >= SCAN_W8 || fe->py < 0 || fe->py >= SCAN_H8) {
            remove_foe(fe);
            continue;
        }
        live++;
    }

    noiz.n_foe = foe_cnt;
    noiz.n_bullet = live;
    noiz.n_runner = bml_runner_used();

    noiz.interval = INTERVAL_BASE;
    if (!noiz.insane && live > slowdown_bullets) {
        noiz.interval += (live - slowdown_bullets) * INTERVAL_BASE / slowdown_bullets;
        if (noiz.interval > INTERVAL_BASE * 2) noiz.interval = INTERVAL_BASE * 2;
    }
}

static void clear_foes(int zako_only)
{
    int i;
    for (i = 0; i < FOE_MAX; i++) {
        if (foe[i].spc == NOT_EXIST) continue;
        if (zako_only && (foe[i].type == BOSS_TYPE || foe[i].spc == SPC_BOSSB)) continue;
        add_clear_frag(foe[i].x, foe[i].y, foe[i].mvx, foe[i].mvy);
        remove_foe(&foe[i]);
    }
}

/* ---- Zeichnen ----------------------------------------------------------- */
static const uint8_t foe_col[4][2] = {
    {CL(CG_CYAN, 13),   CL(CG_AZURE, 7)},
    {CL(CG_MAGENTA, 13), CL(CG_VIOLET, 7)},
    {CL(CG_LIME, 13),   CL(CG_GREEN, 7)},
    {CL(CG_INK, 14),  CL(CG_AMBER, 9)}
};
static const uint8_t bullet_col[3][2] = {
    {CL(CG_ORANGE, 15), CL(CG_RED, 9)},
    {CL(CG_YELLOW, 15), CL(CG_ORANGE, 9)},
    {CL(CG_ROSE, 15),   CL(CG_MAGENTA, 9)}
};

static void draw_foes(void)
{
    int i, j;
    for (i = 0; i < FOE_MAX; i++) {
        Foe *fe = &foe[i];
        int x, y, sz, d, md;
        uint8_t c1, c2;
        if (fe->spc != SPC_FOE) continue;
        x = TO_SCR((int)(fe->x >> 8));
        y = TO_SCR((int)(fe->y >> 8));
        sz = TO_SCR(fe->cnt < 16 ? (foe_size[fe->type] * fe->cnt) >> 4 : foe_size[fe->type]);
        c1 = foe_col[fe->type][0];
        c2 = foe_col[fe->type][1];
        if (fe->hit) c1 = CL(CG_INK, 15);
        draw_box(x, y, sz, sz, c1, c2);
        if (fe->shield <= 0) continue;
        d = (fe->cnt * 8) << 4;
        md = (DIV << 4) / fe->shield;
        sz /= 3;
        if (sz < 2) sz = 2;
        for (j = 0; j < fe->shield && j < 24; j++, d += md) {
            int di = (d >> 4) & (DIV - 1);
            draw_box(x + ((sctbl[di] * sz) >> 7), y + ((sctbl[di + DIV / 4] * sz) >> 7),
                     sz, sz, c1, c2);
        }
    }
}

static void draw_bullets_wake(void)
{
    int i;
    for (i = 0; i < FOE_MAX; i++) {
        Foe *fe = &foe[i];
        if (fe->spc == NOT_EXIST || fe->spc == SPC_FOE || fe->cnt >= 64) continue;
        draw_line(TO_SCR((int)(fe->x >> 8)), TO_SCR((int)(fe->y >> 8)),
                  TO_SCR((int)(fe->sx >> 8)), TO_SCR((int)(fe->sy >> 8)),
                  CL(CG_AZURE, 2 + (12 - fe->cnt / 5 > 0 ? 12 - fe->cnt / 5 : 0) / 3));
    }
}

#define BULLET_WIDTH 5

static void draw_bullets(void)
{
    int i;
    for (i = 0; i < FOE_MAX; i++) {
        Foe *fe = &foe[i];
        int bc;
        if (fe->spc == NOT_EXIST || fe->spc == SPC_FOE) continue;
        bc = fe->color % 3;
        draw_thick_line(TO_SCR((int)(fe->x >> 8)), TO_SCR((int)(fe->y >> 8)),
                        TO_SCR((int)(fe->px >> 8)), TO_SCR((int)(fe->py >> 8)),
                        bullet_col[bc][0], bullet_col[bc][1], TO_SCR(BULLET_WIDTH));
    }
}

static void draw_shots(void)
{
    int i;
    for (i = 0; i < SHOT_MAX; i++) {
        int x, y, d;
        if (shot[i].cnt < 0) continue;
        x = TO_SCR((int)(shot[i].x >> 8));
        y = TO_SCR((int)(shot[i].y >> 8));
        d = (shot[i].cnt * 16) & (DIV / 8 - 1);
        draw_box(x + ((sctbl[d] * TO_SCR(SHOT_WIDTH)) >> 8), y,
                 TO_SCR(SHOT_WIDTH), TO_SCR(SHOT_HEIGHT), CL(CG_CYAN, 10), CL(CG_INK, 14));
        draw_box(x - ((sctbl[d] * TO_SCR(SHOT_WIDTH)) >> 8), y,
                 TO_SCR(SHOT_WIDTH), TO_SCR(SHOT_HEIGHT), CL(CG_CYAN, 10), CL(CG_INK, 14));
    }
}

/* Mittelkasten mit vier umlaufenden Trommeln, wie in der Vorlage; waehrend der
 * Unverwundbarkeit blinkt nur der Kern. */
#define SHIP_DRAW_W  TO_SCR(6)
#define SHIP_DRUM_W  15
#define SHIP_DRUM_SZ TO_SCR(4)

static void draw_ship(void)
{
    int x = TO_SCR((int)(ship.x >> 8)), y = TO_SCR((int)(ship.y >> 8));
    int d = ((ship.cnt * 8) & (DIV / 8 - 1)) - DIV / 4;
    int i, ic = ship.inv & 31;

    if (ic > 0 && ic < 16) {
        draw_box(x, y, SHIP_DRAW_W, SHIP_DRAW_W, CL(CG_CYAN, 9), CL(CG_BLUE, 6));
        return;
    }
    for (i = 0; i < 4; i++) {
        d &= (DIV - 1);
        draw_box(x + ((sctbl[d] * TO_SCR(SHIP_DRUM_W)) >> 8),
                 y - ((sctbl[d + DIV / 4] * TO_SCR(SHIP_DRUM_W)) >> 10),
                 SHIP_DRUM_SZ, TO_SCR(SHIP_DRUM_W * 2), CL(CG_BLUE, 7), CL(CG_AZURE, 5));
        d += DIV / 8;
    }
    draw_box(x, y, SHIP_DRAW_W, SHIP_DRAW_W, CL(CG_INK, 15), CL(CG_CYAN, 12));
    for (i = 0; i < 4; i++) {
        d &= (DIV - 1);
        draw_box(x + ((sctbl[d] * TO_SCR(SHIP_DRUM_W)) >> 8),
                 y - ((sctbl[d + DIV / 4] * TO_SCR(SHIP_DRUM_W)) >> 10),
                 SHIP_DRUM_SZ, TO_SCR(SHIP_DRUM_W * 2), CL(CG_CYAN, 11), CL(CG_INK, 13));
        d += DIV / 8;
    }
}

static void draw_frags(void)
{
    static const uint8_t fc[3][2] = {
        {CL(CG_AMBER, 12), CL(CG_RED, 8)},
        {CL(CG_ORANGE, 12), CL(CG_AMBER, 8)},
        {CL(CG_GREY, 10), CL(CG_INK, 12)}
    };
    int i;
    for (i = 0; i < FRAG_MAX; i++) {
        if (frag[i].cnt <= 0) continue;
        draw_box((int)frag[i].x, (int)frag[i].y, frag[i].w, frag[i].h,
                 fc[frag[i].spc][0], fc[frag[i].spc][1]);
    }
}

static void draw_bonuses(void)
{
    int i;
    for (i = 0; i < BONUS_MAX; i++) {
        int x, y, d, ox, oy;
        if (bonus[i].cnt < 0) continue;
        d = (bonus[i].cnt * 8) & (DIV - 1);
        x = TO_SCR((int)(bonus[i].x >> 8));
        y = TO_SCR((int)(bonus[i].y >> 8));
        ox = sctbl[d] >> 6;
        oy = sctbl[d + DIV / 4] >> 6;
        draw_box(x + ox, y + oy, BONUS_DRAW_W, BONUS_DRAW_W, CL(CG_GREEN, 13), CL(CG_MINT, 9));
        draw_box(x - ox, y - oy, BONUS_DRAW_W, BONUS_DRAW_W, CL(CG_GREEN, 13), CL(CG_MINT, 9));
        draw_box(x + oy, y - ox, BONUS_DRAW_W, BONUS_DRAW_W, CL(CG_GREEN, 13), CL(CG_MINT, 9));
        draw_box(x - oy, y + ox, BONUS_DRAW_W, BONUS_DRAW_W, CL(CG_GREEN, 13), CL(CG_MINT, 9));
    }
}

/* ---- Wellensteuerung ----------------------------------------------------
 * Die Vorlage haelt je Typ eine Warteschlange von Mustern, mischt sie zu
 * Spielbeginn und schiebt sie beim Szenenwechsel weiter.  Das ist hier
 * unveraendert uebernommen, nur mit Indizes statt Zeigern. */
#define BARRAGE_TYPES 3
#define BARRAGE_MAX   16
#define PATTERN_MAX   40

typedef struct { uint8_t brg; int32_t max_rank, rank; uint8_t type, frq; } Barrage;

static Barrage pattern[BARRAGE_TYPES][PATTERN_MAX];
static uint8_t queue[BARRAGE_TYPES][PATTERN_MAX];
static int pattern_n[BARRAGE_TYPES];
static uint8_t barrage[BARRAGE_MAX];
static int barrage_n, boss_mode, scene_cnt, zako_cnt, quick_type;
static int32_t level, level_inc;
static int pax, pay;

#define STAGE_NUM 10
#define ENDLESS_NUM 4
static const int16_t stage_prm[STAGE_NUM + ENDLESS_NUM][3] = {
    /* Startwert, Anfangsstufe*10, Zuwachs*100 */
    { 13,   5,  12}, {  2,  18,  15}, {  3,  32,  10}, { 90,  60,  30}, {  5,  50,  60},
    {  6, 100,  60}, {  7,  50, 220}, { 98, 120, 150}, {  9, 100, 200}, { 79, 210, 150},
    { -3,  50,  70}, { -1, 100, 120}, { -4, 150, 180}, { -2, 160, 180},
};

static void init_patterns(void)
{
    int i, t;
    for (t = 0; t < BARRAGE_TYPES; t++) pattern_n[t] = 0;
    for (i = 0; i < bml_brg_num; i++) {
        t = bml_brg[i].type;
        if (t < BARRAGE_TYPES && pattern_n[t] < PATTERN_MAX) {
            pattern[t][pattern_n[t]].brg = (uint8_t)i;
            pattern[t][pattern_n[t]].type = (uint8_t)t;
            pattern_n[t]++;
        }
    }
}

static void init_barrages(int seed, int32_t start_level, int32_t li)
{
    int t, j, n1, n2, rn;

    for (t = 0; t < BARRAGE_TYPES; t++)
        for (j = 0; j < pattern_n[t]; j++)
            queue[t][j] = (uint8_t)j;

    slowdown_bullets = DEFAULT_SLOWDOWN_BULLETS;
    if (seed >= 0) {
        rng = (uint32_t)seed * 2654435761u + 1u;
        noiz.endless = 0;
        noiz.insane = 0;
    } else {
        noiz.endless = 1;
        noiz.insane = (seed == -2);
        if (seed == -3) slowdown_bullets = EASY_SLOWDOWN_BULLETS;
        else if (seed == -4) slowdown_bullets = HARD_SLOWDOWN_BULLETS;
    }
    for (t = 0; t < BARRAGE_TYPES; t++) {
        int bn = pattern_n[t];
        if (bn <= 0) continue;
        rn = 60 + randN(4);
        for (j = 0; j < rn; j++) {
            uint8_t tb;
            n1 = randN(bn); n2 = randN(bn);
            tb = queue[t][n1]; queue[t][n1] = queue[t][n2]; queue[t][n2] = tb;
        }
        for (j = 0; j < bn; j++) {
            pattern[t][queue[t][j]].max_rank = (randN(70) * 65536 / 100) + (3 * 65536 / 10);
            pattern[t][queue[t][j]].frq = 1;
        }
    }
    noiz.scene = -1;
    scene_cnt = 0;
    level = start_level;
    level_inc = li;
    barrage_n = 0;
    boss_mode = 0;
}

static void roll_queue(int t)
{
    int bn = pattern_n[t], i;
    int n = (bn * 32) / (randN(32) + 32) ;
    uint8_t tq;
    if (bn <= 1) return;
    if (n <= 0) return;
    if (n > bn) n = bn;
    tq = queue[t][0];
    for (i = 0; i < n - 1; i++) queue[t][i] = queue[t][i + 1];
    queue[t][n - 1] = tq;
    pattern[t][queue[t][0]].max_rank *= 2;
    while (pattern[t][queue[t][0]].max_rank > 65536)
        pattern[t][queue[t][0]].max_rank -= 7 * 65536 / 10;
}

static void set_barrages(int32_t lv, int bm, int mid_mode)
{
    int bpn, bn, barrage_max, add_frq = 0;

    barrage_n = 0;
    barrage_max = randN(3) + 4;
    boss_mode = bm;
    bpn = mid_mode ? 1 : 0;
    quick_type = bpn;

    for (bn = 0; ; bn++) {
        if (bn == 0 && lv < 0) break;
        if (boss_mode) {
            bpn = (bn == 0) ? 0 : 2;
            if (bn >= BARRAGE_MAX) break;
        } else if (bn >= barrage_max) {
            bn = 0;
            add_frq = 1;
        }
        if (add_frq) {
            Barrage *b = &pattern[bml_brg[barrage[bn]].type][0];
            int t = bml_brg[barrage[bn]].type, k;
            for (k = 0; k < pattern_n[t]; k++)
                if (pattern[t][k].brg == barrage[bn]) { b = &pattern[t][k]; break; }
            if (b->frq < 200) b->frq++;
            lv -= 65536 + b->rank;
            if (lv < 0) break;
        } else {
            Barrage *b;
            if (pattern_n[bpn] <= 0) break;
            barrage_n++;
            roll_queue(bpn);
            b = &pattern[bpn][queue[bpn][0]];
            barrage[bn] = b->brg;
            b->frq = 1;
            if (lv < b->max_rank) {
                b->rank = lv < 0 ? 0 : lv;
                if (!boss_mode || bn > 0) break;
            }
            b->rank = b->max_rank;
            if (!boss_mode) lv -= 65536 + b->max_rank;
            else if (bn > 0) lv -= 4 * 65536 + b->max_rank * 6;
            bpn++;
            if (bpn >= BARRAGE_TYPES) bpn = mid_mode ? 1 : 0;
        }
        if (bn >= BARRAGE_MAX - 1) break;
    }

    pax = randN(SCAN_W8 * 2 / 3) + SCAN_W8 / 6;
    pay = randN(SCAN_H8 / 6) + SCAN_H8 / 10;
    noiz.scene++;
}

static Barrage *find_barrage(int bi)
{
    int t = bml_brg[bi].type, k;
    for (k = 0; k < pattern_n[t]; k++)
        if (pattern[t][k].brg == bi) return &pattern[t][k];
    return &pattern[t][0];
}

#define BOSS_SHIELD 96
#define SCENE_TERM 1000
#define SCENE_END_TERM 100
#define ZAKO_APP_TERM 1500
static const int16_t app_freq[3] = {90, 360, 800};
static const int16_t shield_of[3] = {3, 6, 9};

static void add_boss(void)
{
    int i, first = 1;
    for (i = 0; i < barrage_n; i++) {
        Barrage *b;
        if (bml_brg[barrage[i]].type != 2) continue;
        b = find_barrage(barrage[i]);
        if (first) {
            add_foe(SCAN_W8 / 2, SCAN_H8 / 5, b->rank, 512, 0,
                    BOSS_TYPE, BOSS_SHIELD, barrage[i]);
            first = 0;
        } else {
            Foe *fe = add_foe(SCAN_W8 / 2, SCAN_H8 / 5, b->rank, 512, 0,
                              BOSS_TYPE, 0, barrage[i]);
            if (fe) {
                foe_cnt--; en_num[BOSS_TYPE]--;
                fe->spc = SPC_BOSSB;
            }
        }
    }
}

static void set_clear_score(void) { add_score(1000 * (noiz.scene + 1)); }

static void boss_destroyed(void)
{
    if (!noiz.endless) {
        set_clear_score();
        noiz.status = ST_CLEAR;
        noiz.msg[0] = 0;
    }
    clear_foes(0);
    scene_cnt = 180;
    zako_cnt = 0;
}

static void add_bullets(void)
{
    int i;

    scene_cnt--;
    if (scene_cnt < 0) {
        if (!noiz.insane) clear_foes(0);
        if (noiz.scene >= 0 && !noiz.endless) set_clear_score();
        if (noiz.scene % 10 == 8) {
            scene_cnt = 999999;
            zako_cnt = ZAKO_APP_TERM;
            set_barrages(level, 1, 0);
            add_boss();
        } else {
            scene_cnt = SCENE_TERM;
            set_barrages(level, 0, noiz.scene % 10 == 3);
        }
        level += level_inc;
        if (noiz.status != ST_GAME) scene_cnt = 999999;
    }
    if (scene_cnt < SCENE_END_TERM) return;

    for (i = 0; i < barrage_n; i++) {
        Barrage *b = find_barrage(barrage[i]);
        int type = bml_brg[barrage[i]].type, frq;
        if (boss_mode) {
            if (i > 0) break;
            if (zako_cnt <= 0) break;
            zako_cnt--;
        }
        if (type == quick_type && en_num[type] == 0)
            add_foe(pax, pay, b->rank, 512, 0, type, shield_of[type], barrage[i]);
        frq = app_freq[type] / (b->frq ? b->frq : 1);
        if (frq < 2) frq = 2;
        if (randN(frq) == 0) {
            int x = randN(SCAN_W8 * 2 / 3) + SCAN_W8 / 6;
            int y = randN(SCAN_H8 / 6) + SCAN_H8 / 10;
            if (type == quick_type) { pax = x; pay = y; }
            add_foe(x, y, b->rank, 512, 0, type, shield_of[type], barrage[i]);
        }
    }
}

/* ---- Spieler ------------------------------------------------------------ */
static int gameover_cnt, clear_cnt;

static void destroy_ship(void)
{
    if (ship.inv > 0 || noiz.status != ST_GAME) return;
    add_ship_frag();
    noiz.audio.ev |= EV_SHIP_DIE;
    reset_bonus_score();
    noiz.left--;
    if (noiz.left < 0) {
        noiz.status = ST_GAMEOVER;
        gameover_cnt = 0;
        noiz.msg[0] = 0;
    } else {
        /* Nur die kleinen Gegner raeumen -- der Boss bleibt stehen. */
        ship.inv = SHIP_INV_BASE * (100 - noiz.scene) / 100;
        if (ship.inv < 0) ship.inv = 0;
        clear_foes(1);
    }
}

static void move_ship(uint16_t pad)
{
    int sd = -1;
    if (pad & PAD_RI) sd = 2;
    if (pad & PAD_LF) sd = 6;
    if (pad & PAD_DN) sd = (sd == 2) ? 3 : (sd == 6 ? 5 : 4);
    if (pad & PAD_UP) sd = (sd == 2) ? 1 : (sd == 6 ? 7 : 0);

    if (pad & PAD_B) {
        if (ship.shot_cnt < 0 && noiz.status == ST_GAME) {
            add_shot();
            ship.shot_cnt = SHOT_INTERVAL;
        }
    }
    ship.shot_cnt--;
    if (pad & PAD_A) {
        if (ship.speed > SHIP_SLOW_SPEED) ship.speed -= SHIP_SLOW_DOWN;
    } else {
        if (ship.speed < SHIP_SPEED) ship.speed += SHIP_SLOW_DOWN;
    }

    if (sd >= 0) {
        ship.x += (ship.speed * ship_mv[sd][0]) >> 8;
        ship.y += (ship.speed * ship_mv[sd][1]) >> 8;
        if (ship.x < SHIP_EDGE) ship.x = SHIP_EDGE;
        else if (ship.x > SCAN_W8 - SHIP_EDGE) ship.x = SCAN_W8 - SHIP_EDGE;
        if (ship.y < SHIP_EDGE) ship.y = SHIP_EDGE;
        else if (ship.y > SCAN_H8 - SHIP_EDGE) ship.y = SCAN_H8 - SHIP_EDGE;
    }
    ship.cnt++;
    if (ship.inv > 0) ship.inv--;
}

/* ---- Nachglühen ---------------------------------------------------------
 * Die Vorlage mischt zwei 8-Bit-Ebenen ueber eine 64-KB-Tabelle und laesst
 * sie ueber eine Diffusionstabelle ausklingen.  Hier reicht ein Puffer, dessen
 * Helligkeit je Bild um eine Stufe faellt: Objekte, die jedes Bild neu
 * gezeichnet werden, bleiben hell, alles andere zieht eine Spur.
 * Vier Bildpunkte je Wort, ohne Verzweigung. */
static void decay_fb(void)
{
    uint32_t *p = (uint32_t *)(void *)fb_glow;
    uint32_t *e = p + (SCR_W * SCR_H) / 4;
    for (; p < e; p++) {
        uint32_t v = *p;
        uint32_t lo = v & 0x0F0F0F0Fu;
        uint32_t nz = lo | (lo >> 1);
        nz |= nz >> 2;
        nz &= 0x01010101u;
        *p = (v & 0xF0F0F0F0u) | (lo - nz);
    }
}

/* ---- Titelbild ---------------------------------------------------------- */
static int title_cnt, title_stage;

static void init_title_stage(int stg)
{
    init_foes();
    init_barrages(stage_prm[stg][0], (int32_t)stage_prm[stg][1] * 65536 / 10,
                  (int32_t)stage_prm[stg][2] * 65536 / 100);
}

static void init_title(void)
{
    noiz.status = ST_TITLE;
    title_cnt = 0;
    init_ship();
    init_shots();
    init_frags();
    init_bonuses();
    set_background(1);
    init_title_stage(title_stage);
    noiz.stage = title_stage;
    noiz.msg[0] = 0;
    noiz.msg2[0] = 0;
}

static void start_game(int stg)
{
    noiz.status = ST_GAME;
    noiz.stage = stg;
    noiz.score = 0;
    noiz.left = 2;
    init_ship();
    init_shots();
    init_foes();
    init_frags();
    init_bonuses();
    init_barrages(stage_prm[stg][0], (int32_t)stage_prm[stg][1] * 65536 / 10,
                  (int32_t)stage_prm[stg][2] * 65536 / 100);
    set_background(stg < STAGE_NUM ? stg % 5 + 1 : (noiz.insane ? 5 : 0));
    noiz.msg[0] = 0;
    noiz.msg2[0] = 0;
    noiz.audio.ev |= EV_UI;
}

/* ---- ein Bild ----------------------------------------------------------- */
void noiz_tick(uint16_t pad)
{
    noiz.audio.ev = 0;

    switch (noiz.status) {
    case ST_TITLE:
        title_cnt++;
        if (pad & (TRG_LF | TRG_UP)) {
            title_stage = (title_stage + STAGE_NUM + ENDLESS_NUM - 1) % (STAGE_NUM + ENDLESS_NUM);
            init_title_stage(title_stage);
            noiz.stage = title_stage;
            noiz.audio.ev |= EV_UI;
        } else if (pad & (TRG_RI | TRG_DN)) {
            title_stage = (title_stage + 1) % (STAGE_NUM + ENDLESS_NUM);
            init_title_stage(title_stage);
            noiz.stage = title_stage;
            noiz.audio.ev |= EV_UI;
        } else if (pad & (TRG_A | TRG_B)) {
            start_game(title_stage);
            break;
        }
        move_background();
        add_bullets();
        move_foes();
        break;

    case ST_GAME:
        /* Kein Pausenknopf: im Spiel sind Joystick, A (langsam) und B (Feuer)
         * dauernd in Gebrauch, und die Joystickmitte kann beim Ausweichen
         * unbeabsichtigt ausloesen -- eine Pause mitten im Kugelvorhang waere
         * schlimmer als keine.  Siehe doc/PORT.md. */
        move_background();
        add_bullets();
        move_shots();
        move_ship(pad);
        move_foes();
        move_frags();
        move_bonuses();
        break;

    case ST_GAMEOVER:
        if (noiz.msg[0] == 0) strcpy(noiz.msg, "GAME OVER");
        gameover_cnt++;
        if (gameover_cnt > 60 && (pad & (TRG_A | TRG_B | TRG_C))) {
            init_title();
            return;
        }
        move_background();
        add_bullets();
        move_shots();
        move_foes();
        move_frags();
        break;

    case ST_CLEAR:
        if (noiz.msg[0] == 0) {
            strcpy(noiz.msg, "STAGE CLEAR");
            clear_cnt = 0;
        }
        clear_cnt++;
        if (clear_cnt > 240) {
            noiz.stage++;
            if (noiz.stage >= STAGE_NUM) { init_title(); return; }
            start_game(noiz.stage);
            return;
        }
        move_background();
        move_shots();
        move_ship(pad);
        move_frags();
        move_bonuses();
        break;
    }

    /* --- Bild aufbauen --- */
    decay_fb();
    dtgt = fb_glow;
    draw_background();
    if (noiz.status == ST_GAME || noiz.status == ST_CLEAR)
        draw_bonuses();
    draw_foes();
    draw_bullets_wake();
    draw_frags();

    memset(fb_top, 0, sizeof fb_top);
    dtgt = fb_top;
    draw_shots();
    if (noiz.status == ST_GAME || noiz.status == ST_CLEAR)
        draw_ship();
    draw_bullets();
    dtgt = fb_glow;

    noiz.ship_x = TO_SCR((int)(ship.x >> 8));
    noiz.ship_y = TO_SCR((int)(ship.y >> 8));
    noiz.ship_inv = ship.inv;
    noiz.audio.bullets = (uint16_t)noiz.n_bullet;
}

/* ---- Start -------------------------------------------------------------- */
void noiz_init(uint32_t seed, int hiscore)
{
    memset(&noiz, 0, sizeof noiz);
    memset(fb_glow, 0, sizeof fb_glow);
    memset(fb_top, 0, sizeof fb_top);
    noiz.fb = fb_glow;
    noiz.fb_top = fb_top;
    noiz.hiscore = hiscore;
    noiz.interval = INTERVAL_BASE;
    rng = seed ? seed : 0x1234567u;
    init_patterns();
    title_stage = 0;
    init_title();
}

/* ---- Selbsttest ---------------------------------------------------------- */
int noiz_selfcheck(void)
{
    int i, n_foe = 0, n_run = 0, en[4] = {0, 0, 0, 0};
    for (i = 0; i < FOE_MAX; i++) {
        int r;
        if (foe[i].spc == NOT_EXIST) {
            if (foe[i].nrun != 0) return 1;
            continue;
        }
        if (foe[i].spc == SPC_FOE) { n_foe++; en[foe[i].type]++; }
        if (foe[i].nrun > 3) return 2;
        for (r = 0; r < foe[i].nrun; r++) {
            if (foe[i].runner[r] < -1 || foe[i].runner[r] >= BML_MAX_RUNNER) return 3;
            if (foe[i].runner[r] >= 0) n_run++;
        }
        if (foe[i].x < -SCAN_W8 || foe[i].x > 2 * SCAN_W8) return 4;
        if (foe[i].y < -SCAN_H8 || foe[i].y > 2 * SCAN_H8) return 5;
    }
    if (n_foe != foe_cnt) return 6;
    for (i = 0; i < 4; i++)
        if (en[i] != en_num[i]) return 7;
    if (n_run > bml_runner_used()) return 8;
    return 0;
}
