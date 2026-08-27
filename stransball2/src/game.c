/* game.c -- Super Transball 2, Spielkern.
 *
 * Portiert aus Super Transball 2 von Santiago Ontanon (GPL-2.0, siehe
 * COPYING).  Uebernommen sind Schiffsphysik, Ballphysik am Traktorstrahl,
 * Kollision, Gegnerverhalten, Tueren und Schalter; die Anpassungen fuer den
 * PicoBoy Color Plus stehen in doc/PORT.md.
 */
#include <string.h>
#include "game.h"
#include "trig.h"
#include "color565.h"
#include "segfont.h"

stb_state_t stb;
stb_render_cfg_t stb_render;

static uint32_t rng = 0x2545F491u;

uint32_t stb_rand(void)
{
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    return rng;
}
static int randN(int n) { return n > 0 ? (int)(stb_rand() % (uint32_t)n) : 0; }

/* ---- Karte ------------------------------------------------------------- */
static int16_t map_cell[MAP_MAX_CELLS];
static int map_sx, map_sy;
static int anim_timer, anim_flag;

typedef struct { int16_t x, y; int8_t state, action, event; } DOOR;
typedef struct { int16_t x, y; uint8_t number, state; } SWITCH;
typedef struct { int16_t x, y; } FUELR;
typedef struct {
    uint8_t type, dir, alive;
    int16_t life, tile;
    int32_t x, y, sx, sy;
    int16_t state, state2;
    int16_t tank_angle, turret_angle;
} ENEMY;
typedef struct { int32_t x, y, sx, sy; int16_t state; uint8_t alive; } BULLET;

static ENEMY enemy[MAX_ENEMIES];
static BULLET bullet[MAX_BULLETS];
static DOOR door[MAX_DOORS];
static SWITCH swtch[MAX_SWITCHES];
static FUELR fuelr[MAX_FUEL];
static int n_door, n_switch, n_fuel;

/* ---- Schiff und Ball ---------------------------------------------------- */
static int32_t ship_x, ship_y, ship_sx, ship_sy;
static int ship_angle, ship_anim, ship_state, ship_atractor;
static int32_t ball_x, ball_y, ball_sx, ball_sy;
static int atr_n;
static int32_t atr_x[MAX_ATRACT_P], atr_y[MAX_ATRACT_P];
static uint8_t atr_sp[MAX_ATRACT_P];

/* Gedrehte Schiffsmaske des laufenden Bildes (32x32 Bit) */
static uint8_t ship_rmask[SHIP_W * SHIP_H / 8];
static uint16_t ship_rpix[SHIP_W * SHIP_H];

static const int16_t shot_fuel[3] = { 2, 4, 8 };
static const int16_t thrust_pow[3] = { 24, 18, 11 };
static const int16_t shot_speed[3] = { 4, 3, 2 };
static const int16_t shot_strength[3] = { 1, 2, 4 };

/* ---- Kachelzugriff ------------------------------------------------------ */
static inline int cell_at(int tx, int ty)
{
    if (tx < 0 || ty < 0 || tx >= map_sx || ty >= map_sy)
        return -1;
    return map_cell[ty * map_sx + tx];
}

/* Animierte Kacheln.
 *
 * Nur bestimmte Kachelnummern laufen, und zwar nach festen Regeln aus
 * animtimer (0..24) und animflag (0..7).  Ein Rueckgabewert von -1 heisst
 * "in diesem Moment nicht zeichnen".  Reihenfolge und Schwellen entsprechen
 * der Vorlage; wer hier raet, bekommt blinkende Rohre und stillstehende
 * Lampen.
 */
static int anim_piece(int p)
{
    int f1 = anim_flag & 1, f3 = anim_flag & 3;

    if (p < 0) return p;

    if (p == 110) return anim_timer > 16 ? 111 : (anim_timer > 8 ? 112 : p);
    if (p == 64)  return anim_timer > 16 ? 66  : (anim_timer > 8 ? 65  : p);

    if (p == 67 && f1) return 69;
    if (p == 68 && f1) return 70;

    if (p == 26  && anim_timer > 12) return f1 ? 25 : 24;
    if (p == 146 && anim_timer > 12) return f1 ? 145 : 144;

    if (p == 27  && f1) return 28;
    if (p == 147 && f1) return 148;

    if ((p == 115 || p == 130) && anim_flag > 3) return -1;

    if (anim_flag > 3) {
        if (p == 32)  return 30;
        if (p == 33)  return 31;
        if (p == 36)  return 34;
        if (p == 37)  return 35;
        if (p == 422) return 420;
        if (p == 423) return 421;
        if (p == 162) return 160;
        if (p == 163) return 161;
        if (p == 166) return 164;
        if (p == 167) return 165;
    }

    /* Zwei Vierertakte mit eigenem Muster je Viertel */
    if (p == 76 || p == 150) {
        int base = (p == 76) ? 76 : 150;   /* Folge: base+1 .. base+3 */
        int late = anim_timer > 12;
        switch (f3) {
        case 0: return late ? base + 3 : -1;
        case 1: return late ? base + 1 : base + 2;
        case 2: return late ? base + 1 : p;
        default: return late ? base + 3 : base + 2;
        }
    }

    return p;
}

/* Was steht wirklich in einem Feld?
 *
 * Zeichnen und Kollision muessen dieselbe Antwort bekommen -- in der Vorlage
 * baut die Trefferpruefung ihre Maskenflaeche mit derselben Zeichenroutine.
 * Daraus folgt unmittelbar: ein gerade ausgeschalteter Laser ist auch nicht
 * fest, und eine offene Tuer laesst durch.
 *
 * Rueckgabe ist die Kachel (oder -1 fuer "nichts"), *off bekommt den
 * waagerechten Versatz der Tuerhaelften.  Ein Bildpunkt bei dst_x holt seinen
 * Wert aus src_x = dst_x - off; liegt der ausserhalb 0..15, ist dort nichts.
 */
static int tile_at(int tx, int ty, int *off)
{
    int c = cell_at(tx, ty);
    int i;

    *off = 0;
    if (c < 0)
        return -1;
    c = anim_piece(c);
    if (c < 0)
        return -1;

    if (c == 113 || c == 114) {            /* Schiebetuer aus zwei Haelften */
        for (i = 0; i < n_door; i++) {
            if (door[i].y != ty)
                continue;
            if (door[i].x == tx)      { *off = (c == 113) ? -door[i].state : door[i].state; break; }
            if (door[i].x == tx - 1)  { *off = (c == 113) ? -door[i].state : door[i].state; break; }
        }
        return c;
    }

    if ((c >= 116 && c < 120) || (c >= 136 && c < 140) || (c >= 156 && c < 160)) {
        for (i = 0; i < n_switch; i++)     /* gedrueckter Schalter: +140 */
            if (swtch[i].x == tx && swtch[i].y == ty && swtch[i].state)
                return c + 140;
        return c;
    }

    return c;
}

/* Ist der Punkt (px,py) in Kartenpixeln fest? */
static int solid_at(int px, int py)
{
    int off, c = tile_at(px >> 4, py >> 4, &off);
    int sx = (px & 15) - off;
    if (c < 0 || c >= TILE_NUM || (unsigned)sx >= TILE_W)
        return 0;
    {
        int i = (py & 15) * TILE_W + sx;
        return (stb_mask[c][i >> 3] >> (7 - (i & 7))) & 1;
    }
}

/* ---- Schiffsmaske drehen ------------------------------------------------
 * Die Vorlage dreht das Sprite zur Laufzeit mit SGE und prueft dann Pixel
 * gegen Pixel.  Hier dasselbe, nur von Hand: Rueckwaertsabbildung mit der
 * Winkeltabelle, einmal je Bild statt einmal je Pruefung. */
static void rotate_ship(int frame, int angle)
{
    int c = stb_cos[angle], s = stb_sin[angle];
    int dx, dy;
    memset(ship_rmask, 0, sizeof ship_rmask);
    for (dy = 0; dy < SHIP_H; dy++) {
        for (dx = 0; dx < SHIP_W; dx++) {
            int rx = dx - SHIP_W / 2, ry = dy - SHIP_H / 2;
            int sxp = ((rx * c + ry * s) >> 10) + SHIP_W / 2;
            int syp = ((-rx * s + ry * c) >> 10) + SHIP_H / 2;
            int d = dy * SHIP_W + dx;
            if ((unsigned)sxp < SHIP_W && (unsigned)syp < SHIP_H) {
                int si = syp * SHIP_W + sxp;
                ship_rpix[d] = stb_ship[frame][si];
                if ((stb_shipmask[frame][si >> 3] >> (7 - (si & 7))) & 1)
                    ship_rmask[d >> 3] |= 0x80 >> (d & 7);
            } else {
                ship_rpix[d] = 0;
            }
        }
    }
}

/* Gedrehte Schiffsmaske gegen die Karte pruefen */
static int ship_map_collision(void)
{
    int px = (int)(ship_x / FACTOR) - SHIP_W / 2;
    int py = (int)(ship_y / FACTOR) - SHIP_H / 2;
    int dx, dy;
    for (dy = 0; dy < SHIP_H; dy++) {
        for (dx = 0; dx < SHIP_W; dx++) {
            int d = dy * SHIP_W + dx;
            if (!((ship_rmask[d >> 3] >> (7 - (d & 7))) & 1))
                continue;
            if (solid_at(px + dx, py + dy))
                return 1;
        }
    }
    return 0;
}

/* Eine 16x16-Sondenkachel gegen die Karte pruefen -- so testet die Vorlage
 * die vier Richtungen des Balls (Kacheln 340/342/360/362). */
static int probe_collision(int tile, int px, int py)
{
    int dx, dy;
    if (tile < 0 || tile >= TILE_NUM) return 0;
    for (dy = 0; dy < TILE_H; dy++) {
        for (dx = 0; dx < TILE_W; dx++) {
            int i = dy * TILE_W + dx;
            if (!((stb_mask[tile][i >> 3] >> (7 - (i & 7))) & 1))
                continue;
            if (solid_at(px + dx, py + dy))
                return 1;
        }
    }
    return 0;
}

/* ---- Level aufbauen -----------------------------------------------------
 * Die Vorlage kodiert Gegner, Tueren, Schalter und Tankstellen als bestimmte
 * Kachelnummern in der Karte und sammelt sie beim Laden ein.  Genauso hier. */
static void add_enemy(int type, int x, int y, int dir, int life, int tile)
{
    int i;
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (enemy[i].alive) continue;
        memset(&enemy[i], 0, sizeof enemy[i]);
        enemy[i].alive = 1;
        enemy[i].type = (uint8_t)type;
        enemy[i].dir = (uint8_t)dir;
        enemy[i].x = x;
        enemy[i].y = y;
        enemy[i].life = (int16_t)life;
        enemy[i].tile = (int16_t)tile;
        enemy[i].turret_angle = 90;
        return;
    }
}

static void load_level(int lv)
{
    const stb_mapinfo_t *mi;
    int i, cells;

    if (lv < 0) lv = 0;
    if (lv >= stb_map_num) lv = stb_map_num - 1;
    mi = &stb_map[lv];

    map_sx = mi->sx;
    map_sy = mi->sy + EMPTY_ROWS;
    cells = map_sx * map_sy;
    if (cells > MAP_MAX_CELLS) {           /* Notbremse, sollte nie greifen */
        map_sy = MAP_MAX_CELLS / map_sx;
        cells = map_sx * map_sy;
    }
    for (i = 0; i < map_sx * EMPTY_ROWS; i++)
        map_cell[i] = -1;
    for (i = map_sx * EMPTY_ROWS; i < cells; i++)
        map_cell[i] = stb_mapdata[mi->off + i - map_sx * EMPTY_ROWS];

    memset(enemy, 0, sizeof enemy);
    memset(bullet, 0, sizeof bullet);
    n_door = n_switch = n_fuel = 0;

    for (i = 0; i < cells; i++) {
        int c = map_cell[i];
        int x = (i % map_sx) * 16, y = (i / map_sx) * 16;
        int dir = 0;

        if (c < 0) continue;

        if ((c >= 176 && c < 180) || (c >= 196 && c < 200) ||
            (c >= 216 && c < 220) || (c >= 236 && c < 240) ||
            c == 154 || c == 155 || c == 174 || c == 175 ||
            c == 386 || c == 387 || c == 406 || c == 407) {
            if (c == 176 || c == 216 || c == 236 || c == 196 || c == 154 || c == 386) dir = 0;
            else if (c == 177 || c == 217 || c == 237 || c == 197 || c == 155 || c == 387) dir = 1;
            else if (c == 178 || c == 218 || c == 238 || c == 198 || c == 174 || c == 406) dir = 2;
            else dir = 3;

            if ((c >= 176 && c < 180) || (c >= 216 && c < 220) || (c >= 236 && c < 240))
                add_enemy(1, x, y, dir, 4, c);                 /* Kanone      */
            else if (c >= 196 && c < 200)
                add_enemy(2, x, y, dir, 8, c);                 /* Schnellfeuer*/
            else if (c == 154 || c == 155 || c == 174 || c == 175)
                add_enemy(3, x, y, dir, 6, c);                 /* Drehkanone  */
            else
                add_enemy(7, x, y, dir, 6, c);                 /* Drehkanone 2*/
        }

        if (c == 113 && n_door < MAX_DOORS) {                  /* Tuer        */
            /* Anfangszustand und Ereignisnummer stehen in der Kartendatei,
             * in derselben Reihenfolge, in der die Zellen gelesen werden.
             * Ereignis 0 heisst "oeffnet, wenn die Kugel genommen wird",
             * sonst ist es die Nummer des zugehoerigen Schalters. */
            door[n_door].x = (int16_t)(i % map_sx);
            door[n_door].y = (int16_t)(i / map_sx);
            door[n_door].action = 0;
            if (n_door < mi->dn) {
                door[n_door].state = (int8_t)stb_doordata[mi->doff + n_door].state;
                door[n_door].event = (int8_t)stb_doordata[mi->doff + n_door].event;
            } else {
                door[n_door].state = 0;
                door[n_door].event = 0;
            }
            n_door++;
        }
        if (((c >= 116 && c < 120) || (c >= 136 && c < 140) ||
             (c >= 156 && c < 160)) && n_switch < MAX_SWITCHES) {
            swtch[n_switch].x = (int16_t)(i % map_sx);
            swtch[n_switch].y = (int16_t)(i / map_sx);
            swtch[n_switch].number = (uint8_t)(n_switch + 1);
            swtch[n_switch].state = 0;
            n_switch++;
        }
        if (c == 132 && n_fuel < MAX_FUEL) {                   /* Tankstelle  */
            fuelr[n_fuel].x = (int16_t)(i % map_sx);
            fuelr[n_fuel].y = (int16_t)(i / map_sx);
            n_fuel++;
        }
        if (c == 110) {                                        /* Ball        */
            ball_x = (int32_t)x * FACTOR;
            ball_y = (int32_t)y * FACTOR;
        }
    }

    /* Panzer stehen nicht in den Zellen, sondern als eigene Liste am Ende der
     * Kartendatei -- (x, y, Typ) je Panzer. */
    for (i = 0; i < mi->tn; i++) {
        const stb_tank_t *t = &stb_tank[mi->toff + i];
        add_enemy(4, t->x * 16, (t->y + EMPTY_ROWS) * 16, t->type, 10, 0);
        }

    /* Startpunkt wie in der Vorlage: waagerecht mittig, senkrecht 32 Punkte
     * unter dem oberen Rand -- also im leeren Vorlauf ueber der Hoehle. */
    ship_x = (int32_t)(map_sx * 8) * FACTOR;
    ship_y = (int32_t)32 * FACTOR;
    ship_sx = ship_sy = 0;
    ship_angle = 0;
    ship_anim = 0;
    ship_state = 0;
    ship_atractor = 0;
    atr_n = 0;
    ball_sx = ball_sy = 0;
    stb.ball_state = -32;
    stb.fuel = stb.fuel_max;
    anim_timer = anim_flag = 0;
}

/* ---- Schuesse des Schiffs ----------------------------------------------- */
static void add_bullet(int32_t x, int32_t y, int32_t sx, int32_t sy)
{
    int i;
    for (i = 0; i < MAX_BULLETS; i++) {
        if (bullet[i].alive) continue;
        bullet[i].alive = 1;
        bullet[i].x = x; bullet[i].y = y;
        bullet[i].sx = sx; bullet[i].sy = sy;
        bullet[i].state = 0;
        return;
    }
}

static int enemy_hit(int px, int py, int strength);

static void cycle_bullets(void)
{
    int i;
    for (i = 0; i < MAX_BULLETS; i++) {
        if (!bullet[i].alive) continue;
        bullet[i].x += bullet[i].sx;
        bullet[i].y += bullet[i].sy;
        bullet[i].state++;
        {
            int px = (int)(bullet[i].x / FACTOR) + 8;
            int py = (int)(bullet[i].y / FACTOR) + 8;
            if (px < 0 || py < 0 || px >= map_sx * 16 || py >= map_sy * 16 ||
                bullet[i].state > 120) {
                bullet[i].alive = 0;
                continue;
            }
            if (enemy_hit(px, py, shot_strength[stb.ship_type])) {
                bullet[i].alive = 0;
                continue;
            }
            if (solid_at(px, py)) {
                bullet[i].alive = 0;
                continue;
            }
        }
    }
}

/* ---- Ball --------------------------------------------------------------- */
static void ball_hits_map(int px, int py);

static void cycle_ball(void)
{
    int bx, by, sxp, syp;

    if (ball_sx > 0) ball_sx--;
    if (ball_sx < 0) ball_sx++;

    bx = (int)(ball_x / FACTOR) + 8;
    by = (int)(ball_y / FACTOR) + 8;
    sxp = (int)(ship_x / FACTOR);
    syp = (int)(ship_y / FACTOR) + 8;

    /* Aufnehmen: der Ball muss im Kegel unter dem Schiff liegen */
    if (ship_atractor != 0 && bx > sxp - 8 && bx < sxp + 8 &&
        by > syp && by < syp + 32 && stb.ball_state < 0) {
        stb.ball_state++;
        if (stb.ball_state == 0) {
            int i;
            stb.audio.ev |= EV_TAKEBALL;
            for (i = 0; i < n_door; i++)   /* Tueren mit Ereignis 0 oeffnen */
                if (door[i].event == 0)
                    door[i].action = door[i].state == 0 ? 1 : -1;
        }
    } else if (stb.ball_state < 0) {
        stb.ball_state = -32;
    }

    if (stb.ball_state == 0) {
        /* Je naeher, desto staerker gezogen -- vier Stufen wie im Original */
        int xd = (int)(ball_x / FACTOR) - (int)(ship_x / FACTOR);
        int yd = (int)(ball_y / FACTOR) - (int)(ship_y / FACTOR);
        int tot = xd * xd + yd * yd;
        int steps = (tot < 100) ? 4 : (tot < 1000) ? 3 : (tot < 4000) ? 2 : (tot < 10000) ? 1 : 0;
        int k;
        for (k = 0; k < steps; k++) {
            if ((ship_x - 8 * FACTOR) < ball_x) ball_sx -= 2; else ball_sx += 2;
            if ((ship_y - 8 * FACTOR) < ball_y) ball_sy -= 2; else ball_sy += 2;
        }
    }

    bx = (int)(ball_x / FACTOR);
    by = (int)(ball_y / FACTOR);

    if (probe_collision(360, bx, by)) {          /* unten */
        if (ball_sy > 0) { ball_sy = -(ball_sy * 3) / 4; ball_hits_map(bx + 8, by + 12); }
        else if (probe_collision(360, bx, by - 1)) ball_sy -= 2;
    } else {
        ball_sy += 2;
    }
    if (probe_collision(340, bx, by)) {          /* oben */
        if (ball_sy < 0) { ball_sy = -(ball_sy * 3) / 4; ball_hits_map(bx + 8, by + 4); }
        else ball_sy += 2;
    }
    if (probe_collision(342, bx, by)) {          /* rechts */
        if (ball_sx > 0) { ball_sx = -(ball_sx * 3) / 4; ball_hits_map(bx + 12, by + 8); }
        else ball_sx -= 2;
    }
    if (probe_collision(362, bx, by)) {          /* links */
        if (ball_sx < 0) { ball_sx = -(ball_sx * 3) / 4; ball_hits_map(bx + 4, by + 8); }
        else ball_sx += 2;
    }

    if (ball_sx >  4 * FACTOR) ball_sx =  4 * FACTOR;
    if (ball_sx < -4 * FACTOR) ball_sx = -4 * FACTOR;
    if (ball_sy >  4 * FACTOR) ball_sy =  4 * FACTOR;
    if (ball_sy < -4 * FACTOR) ball_sy = -4 * FACTOR;
    ball_x += ball_sx;
    ball_y += ball_sy;

    if (ball_x < 0) { ball_x = 0; ball_sx = 0; }
    if (ball_x > (int32_t)((map_sx - 1) * 16) * FACTOR) {
        ball_x = (int32_t)((map_sx - 1) * 16) * FACTOR;
        ball_sx = 0;
    }
    if (ball_y < 0 && stb.ball_state >= 0) {     /* oben raus = geschafft */
        ball_sy = -FACTOR;
        stb.ball_state++;
        if (stb.ball_state >= 32) {
            stb.status = ST_LEVELDONE;
            stb.audio.ev |= EV_LEVELDONE;
            strcpy(stb.msg, "LEVEL DONE");
        }
    }
}

/* ---- Treffer ------------------------------------------------------------ */
static void add_explosion(int32_t x, int32_t y)
{
    add_enemy(6, (int)x, (int)y, 0, 1, 0);
}

/* Trifft ein Schiffsschuss einen Gegner?  Gibt 1 zurueck, wenn ja. */
static int enemy_hit(int px, int py, int strength)
{
    int i;
    for (i = 0; i < MAX_ENEMIES; i++) {
        ENEMY *e = &enemy[i];
        int ex, ey, w = 16;
        if (!e->alive || e->type == 0 || e->type == 6 || e->type == 5)
            continue;
        ex = (e->type >= 1 && e->type <= 3) || e->type == 7 ? (int)e->x : (int)e->x;
        ey = (int)e->y;
        if (e->type == 4) w = 32;
        if (px < ex || px >= ex + w || py < ey || py >= ey + w)
            continue;
        stb.hits++;
        e->life -= strength;
        if (e->life <= 0) {
            stb.destroyed++;
            stb.audio.ev |= EV_EXPLODE;
            add_explosion(e->x, e->y);
            if (e->type == 4) {
                e->type = 5;               /* zerstoerter Panzer bleibt liegen */
                e->life = 1;
            } else {
                /* Kachel aus der Karte nehmen, sonst steht die Kanone weiter */
                int tx = (int)e->x / 16, ty = (int)e->y / 16;
                if (tx >= 0 && ty >= 0 && tx < map_sx && ty < map_sy)
                    map_cell[ty * map_sx + tx] = -1;
                e->alive = 0;
            }
        } else {
            stb.audio.ev |= EV_HIT;
        }
        return 1;
    }
    return 0;
}

/* Der Ball stoesst an -- schaltet Schalter um. */
static void ball_hits_map(int px, int py)
{
    int i, tx = px / 16, ty = py / 16;
    for (i = 0; i < n_switch; i++) {
        if (swtch[i].x != tx || swtch[i].y != ty || swtch[i].state)
            continue;
        swtch[i].state = 1;
        stb.audio.ev |= EV_SWITCH;
        {
            int j;
            for (j = 0; j < n_door; j++)
                if (door[j].event == swtch[i].number)
                    door[j].action = door[j].state == 0 ? 1 : -1;
        }
    }
}

/* ---- Gegnerschuesse ----------------------------------------------------- */
static void enemy_shot(int32_t x, int32_t y, int32_t sx, int32_t sy, int life)
{
    int i;
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (enemy[i].alive) continue;
        memset(&enemy[i], 0, sizeof enemy[i]);
        enemy[i].alive = 1;
        enemy[i].type = 0;
        enemy[i].x = x; enemy[i].y = y;
        enemy[i].sx = sx; enemy[i].sy = sy;
        enemy[i].life = (int16_t)life;
        enemy[i].state = 12;
        enemy[i].tile = 344;
        stb.audio.ev |= EV_ENEMYSHOT;
        return;
    }
}

/* Bodenabstand unter (px,py), hoechstens 32 -- fuer die Panzer */
static int ground_dist(int px, int py)
{
    int d;
    for (d = 0; d < 32; d++)
        if (solid_at(px, py + d))
            return d;
    return 32;
}

static void cycle_enemies(void)
{
    int i;
    int sxp = (int)(ship_x / FACTOR), syp = (int)(ship_y / FACTOR);

    for (i = 0; i < MAX_ENEMIES; i++) {
        ENEMY *e = &enemy[i];
        if (!e->alive) continue;

        switch (e->type) {
        case 0: {                          /* Geschoss */
            int px, py;
            e->x += e->sx;
            e->y += e->sy;
            px = (int)(e->x / FACTOR);
            py = (int)(e->y / FACTOR);
            if (px < 0 || py < 0 || px >= map_sx * 16 || py >= map_sy * 16) {
                e->alive = 0;
                break;
            }
            if (solid_at(px, py)) {
                add_explosion(e->x - 8 * FACTOR, e->y - 8 * FACTOR);
                e->alive = 0;
                break;
            }
            /* Trifft der Schuss das Schiff? */
            if (ship_state == 0) {
                int dx = px - sxp, dy = py - syp;
                if (dx > -10 && dx < 10 && dy > -10 && dy < 10) {
                    ship_state = 1;
                    ship_anim = 0;
                    stb.audio.ev |= EV_DIE;
                    e->alive = 0;
                }
            }
            break;
        }

        case 1:                            /* Kanone */
        case 2: {                          /* Schnellfeuerkanone */
            int reload = (e->type == 1) ? 128 : 48;
            int reach = (e->type == 1) ? 160 : 200;
            if (e->state == 0) {
                int fire = 0;
                int32_t bx = 0, by = 0, bsx = 0, bsy = 0;
                switch (e->dir) {
                case 0: if (sxp >= e->x - 8 && sxp <= e->x + 24 &&
                            syp < e->y && syp > e->y - reach) {
                            fire = 1; bx = (e->x + 8) * FACTOR; by = e->y * FACTOR;
                            bsy = -FACTOR; } break;
                case 1: if (sxp >= e->x - 8 && sxp <= e->x + 24 &&
                            syp > e->y && syp < e->y + reach) {
                            fire = 1; bx = (e->x + 8) * FACTOR; by = (e->y + 16) * FACTOR;
                            bsy = FACTOR; } break;
                case 2: if (syp >= e->y - 8 && syp <= e->y + 24 &&
                            sxp > e->x && sxp < e->x + reach) {
                            fire = 1; bx = (e->x + 16) * FACTOR; by = (e->y + 7) * FACTOR;
                            bsx = FACTOR; } break;
                default: if (syp >= e->y - 8 && syp <= e->y + 24 &&
                             sxp < e->x && sxp > e->x - reach) {
                            fire = 1; bx = e->x * FACTOR; by = (e->y + 7) * FACTOR;
                            bsx = -FACTOR; } break;
                }
                if (fire) {
                    enemy_shot(bx, by, bsx, bsy, 1);
                    e->state = (int16_t)reload;
                }
            } else if (e->state > 0) {
                e->state--;
            }
            break;
        }

        case 3:                            /* Drehkanone */
        case 7: {                          /* Drehkanone, zweite Bauart */
            int dx = sxp - ((int)e->x + 8), dy = syp - ((int)e->y + 8);
            int want = stb_atan2(dy, dx);
            int a = want;
            /* Der Turm kann nur in seinem Halbkreis schwenken */
            switch (e->dir) {
            case 0: if (a >= 345 || a < 90) a = 345; if (a < 205) a = 205; break;
            case 1: if (a < 15 || a >= 270) a = 15;  if (a >= 175) a = 175; break;
            case 2:
                if (a >= 75 && a < 180) a = 75;
                if (a >= 180 && a < 285) a = 285;
                break;
            default: if (a < 105) a = 105;  if (a >= 255) a = 255; break;
            }
            e->turret_angle = (int16_t)a;
            e->state++;
            if (e->state >= (e->type == 3 ? 128 : 96)) {
                if (a == want && (dx * dx + dy * dy) < 30000) {
                    int32_t bsx = (int32_t)stb_cos[a] * FACTOR / 1024;
                    int32_t bsy = (int32_t)stb_sin[a] * FACTOR / 1024;
                    enemy_shot((e->x + 8) * FACTOR + bsx * 8,
                               (e->y + 8) * FACTOR + bsy * 8, bsx, bsy, 1);
                    e->state = 0;
                }
            }
            break;
        }

        case 4: {                          /* Panzer */
            int g1 = ground_dist((int)e->x + 4, (int)e->y + 16);
            int g2 = ground_dist((int)e->x + 28, (int)e->y + 16);
            int old = e->tank_angle, want;
            if (g1 > g2)      want = -stb_atan2((g1 - g2) * 4, 64);
            else if (g1 < g2) want =  stb_atan2((g2 - g1) * 4, 64);
            else              want = 0;
            if (want > 180) want -= 360;
            if (old - want > 2)  want = old - 2;
            if (want - old > 2)  want = old + 2;
            e->tank_angle = (int16_t)want;

            if ((g1 + g2) / 2 > 0) { e->y++; g1--; g2--; }
            if ((g1 + g2) / 2 < 0) { e->y--; g1++; g2++; }
            e->state2++;

            if ((g1 + g2) / 2 == 0 && (e->state2 & 3) == 0) {
                /* Fahren: an Waenden umkehren */
                int rcol = solid_at((int)e->x + 33, (int)e->y + 8);
                int lcol = solid_at((int)e->x - 1, (int)e->y + 8);
                if (e->state >= 0) {
                    if (rcol) { e->state = -1; e->x--; } else e->x++;
                } else {
                    if (lcol) { e->state = 1; e->x++; } else e->x--;
                }
            }
            /* Turm zielt aufs Schiff */
            {
                int dx = sxp - ((int)e->x + 16), dy = syp - ((int)e->y + 8);
                int want2 = stb_atan2(dy, dx);
                if (want2 > 180 && want2 < 350) want2 = 350;
                if (want2 <= 180 && want2 > 190) want2 = 190;
                e->turret_angle = (int16_t)want2;
                if ((e->state2 & 127) == 0 && (dx * dx + dy * dy) < 40000) {
                    int32_t bsx = (int32_t)stb_cos[want2] * FACTOR / 1024;
                    int32_t bsy = (int32_t)stb_sin[want2] * FACTOR / 1024;
                    enemy_shot((e->x + 16) * FACTOR + bsx * 12,
                               (e->y + 8) * FACTOR + bsy * 12, bsx, bsy, 1);
                }
            }
            break;
        }

        case 5:                            /* zerstoerter Panzer: bleibt liegen */
            break;

        case 6:                            /* Explosion: sechs Bilder */
            e->state++;
            if (e->state > 47) e->alive = 0;
            break;

        default:
            e->alive = 0;
            break;
        }
    }
}

/* ---- Tueren, Schalter, Tankstellen -------------------------------------- */
static void cycle_doors(void)
{
    int i;
    for (i = 0; i < n_door; i++) {
        DOOR *d = &door[i];
        if (d->action == 0) continue;
        d->state = (int8_t)(d->state + d->action);
        if (d->state >= 14) { d->state = 14; d->action = 0; }
        if (d->state <= 0)  { d->state = 0;  d->action = 0; }
    }
}

/* Steht das Schiff auf einer Tankstelle?  Die Vorlage prueft ein Feld von
 * 32x32 Punkten ab der Kachelecke -- also zwei Kacheln in jede Richtung. */
static int in_fuel_recharge(void)
{
    int i, px = (int)(ship_x / FACTOR), py = (int)(ship_y / FACTOR);
    for (i = 0; i < n_fuel; i++) {
        int fx = fuelr[i].x * 16, fy = fuelr[i].y * 16;
        if (px >= fx && px < fx + 32 && py >= fy && py < fy + 32)
            return 1;
    }
    return 0;
}

/* ---- Schiff -------------------------------------------------------------- */
static void cycle_ship(uint16_t pad)
{
    int frame, i;

    if (ship_state == 1) {                 /* explodiert */
        ship_anim++;
        if (ship_anim >= 64) {
            stb.lives--;
            if (stb.lives < 0) {
                stb.status = ST_GAMEOVER;
                strcpy(stb.msg, "GAME OVER");
            } else {
                stb.status = ST_DEAD;
            }
        }
        return;
    }

    /* Drehen */
    if (pad & PAD_LF) { ship_angle -= 4; if (ship_angle < 0) ship_angle += 360; }
    if (pad & PAD_RI) { ship_angle += 4; if (ship_angle >= 360) ship_angle -= 360; }

    /* Schub: Richtung ist Winkel minus 90 Grad, wie in der Vorlage */
    stb.audio.thrust = 0;
    if ((pad & PAD_B) && stb.fuel > 0) {
        int a = (ship_angle + 270) % 360;
        int p = thrust_pow[stb.ship_type];
        ship_sx += stb_cos[a] * p / 1024;
        ship_sy += stb_sin[a] * p / 1024;
        stb.fuel--;
        stb.fuel_used++;
        stb.audio.thrust = 1;
        ship_anim++;
        if (ship_anim >= stb_ship_nanim[stb.ship_type]) ship_anim = 1;
    } else {
        ship_anim = 0;
    }

    /* Traktorstrahl */
    if (pad & PAD_A) {
        ship_atractor++;
        if (ship_atractor > 4) ship_atractor = 1;
        if (atr_n < MAX_ATRACT_P) {
            atr_x[atr_n] = ship_x + (int32_t)randN(16 * FACTOR) - 8 * FACTOR;
            atr_y[atr_n] = ship_y + (int32_t)randN(16 * FACTOR) + 16 * FACTOR;
            atr_sp[atr_n] = (uint8_t)(5 + randN(5));
            atr_n++;
        }
    } else {
        ship_atractor = 0;
        atr_n -= 8;
        if (atr_n < 0) atr_n = 0;
    }
    for (i = 0; i < atr_n; i++) {
        int sp = atr_sp[i];
        atr_x[i] += ship_sx * 9 / 10;
        atr_y[i] += ship_sy * 9 / 10;
        atr_x[i] = (ship_x * (10 - sp) + atr_x[i] * sp) / 10;
        atr_y[i] = (ship_y * (10 - sp) + atr_y[i] * sp) / 10;
        if ((ship_x - atr_x[i] < 2 * FACTOR && atr_x[i] - ship_x < 2 * FACTOR) &&
            (ship_y - atr_y[i] < 2 * FACTOR && atr_y[i] - ship_y < 2 * FACTOR)) {
            atr_x[i] = ship_x + (int32_t)randN(16 * FACTOR) - 8 * FACTOR;
            atr_y[i] = ship_y + (int32_t)randN(16 * FACTOR) + 16 * FACTOR;
            atr_sp[i] = (uint8_t)(5 + randN(5));
        }
    }

    /* Feuern */
    if ((pad & TRG_C) && stb.fuel >= shot_fuel[stb.ship_type]) {
        int a = (ship_angle + 270) % 360;
        int sp = shot_speed[stb.ship_type];
        stb.shots++;
        stb.fuel -= shot_fuel[stb.ship_type];
        stb.fuel_used += shot_fuel[stb.ship_type];
        add_bullet(ship_x - 8 * FACTOR, ship_y - 8 * FACTOR,
                   (int32_t)stb_cos[a] * sp * FACTOR / 1024,
                   (int32_t)stb_sin[a] * sp * FACTOR / 1024);
        stb.audio.ev |= EV_SHOT;
    }

    /* Bewegung: Reibung waagerecht, Schwerkraft senkrecht */
    if (ship_sx > 0) ship_sx--;
    if (ship_sx < 0) ship_sx++;
    ship_sy += 2;
    if (ship_sx >  4 * FACTOR) ship_sx =  4 * FACTOR;
    if (ship_sx < -4 * FACTOR) ship_sx = -4 * FACTOR;
    if (ship_sy >  4 * FACTOR) ship_sy =  4 * FACTOR;
    if (ship_sy < -4 * FACTOR) ship_sy = -4 * FACTOR;
    ship_x += ship_sx;
    ship_y += ship_sy;

    if (ship_x < 0) { ship_x = 0; ship_sx = 0; }
    if (ship_y < 0) { ship_y = 0; ship_sy = 0; }
    if (ship_x > (int32_t)(map_sx * 16) * FACTOR) {
        ship_x = (int32_t)(map_sx * 16) * FACTOR; ship_sx = 0;
    }
    if (ship_y > (int32_t)(map_sy * 16) * FACTOR) {
        ship_y = (int32_t)(map_sy * 16) * FACTOR; ship_sy = 0;
    }

    /* Tanken */
    if (in_fuel_recharge()) {
        if (stb.fuel < stb.fuel_max) {
            stb.fuel += 4;
            if (stb.fuel > stb.fuel_max) stb.fuel = stb.fuel_max;
            if ((stb_render.blink & 7) == 0) stb.audio.ev |= EV_FUEL;
        }
    }

    /* Kollision mit der Karte -- gedrehte Maske gegen Kachelmasken */
    frame = stb_ship_base[stb.ship_type] + ship_anim;
    if (frame >= SHIP_FRAMES) frame = stb_ship_base[stb.ship_type];
    rotate_ship(frame, ship_angle);
    if (ship_map_collision()) {
        ship_state = 1;
        ship_anim = 0;
        stb.audio.ev |= EV_DIE;
    }
}

/* ---- Ansicht mitfuehren -------------------------------------------------- */
static void follow_ship(void)
{
    int px = (int)(ship_x / FACTOR), py = (int)(ship_y / FACTOR);
    int mx = px - SCR_W / 2, my = py - VIEW_H / 2;
    int maxx = map_sx * 16 - SCR_W, maxy = map_sy * 16 - VIEW_H;
    if (maxx < 0) maxx = 0;
    if (maxy < 0) maxy = 0;
    if (mx < 0) mx = 0;
    if (my < 0) my = 0;
    if (mx > maxx) mx = maxx;
    if (my > maxy) my = maxy;
    stb.map_x = mx;
    stb.map_y = my;
}

/* ---- Zustandsautomat ----------------------------------------------------- */
static int menu_sel, wait_cnt;

void stb_init(uint32_t seed, int start_level)
{
    memset(&stb, 0, sizeof stb);
    rng = seed ? seed : 0x2545F491u;
    stb.status = ST_TITLE;
    stb.ship_type = 1;
    stb.lives = 3;
    stb.fuel_max = 2000;
    stb.level = (start_level > 0 && start_level < stb_map_num) ? start_level : 0;
    menu_sel = 0;
    wait_cnt = 0;
    load_level(stb.level);
}

void stb_tick(uint16_t pad)
{
    stb.audio.ev = 0;
    stb.audio.thrust = 0;

    switch (stb.status) {
    case ST_TITLE:
        if (pad & (TRG_LF | TRG_UP)) {
            stb.level = (stb.level + stb_map_num - 1) % stb_map_num;
            load_level(stb.level);
            stb.audio.ev |= EV_UI;
        } else if (pad & (TRG_RI | TRG_DN)) {
            stb.level = (stb.level + 1) % stb_map_num;
            load_level(stb.level);
            stb.audio.ev |= EV_UI;
        } else if (pad & (TRG_A | TRG_B)) {
            stb.status = ST_SHIPSEL;
            menu_sel = stb.ship_type;
            stb.audio.ev |= EV_UI;
        }
        break;

    case ST_SHIPSEL:
        if (pad & (TRG_LF | TRG_UP)) { menu_sel = (menu_sel + 2) % 3; stb.audio.ev |= EV_UI; }
        else if (pad & (TRG_RI | TRG_DN)) { menu_sel = (menu_sel + 1) % 3; stb.audio.ev |= EV_UI; }
        else if (pad & (TRG_A | TRG_B)) {
            stb.ship_type = menu_sel;
            stb.lives = 3;
            stb.shots = stb.hits = stb.destroyed = stb.fuel_used = 0;
            load_level(stb.level);
            stb.status = ST_GAME;
            stb.msg[0] = 0;
            stb.audio.ev |= EV_UI;
        }
        break;

    case ST_GAME:
        anim_timer++;
        if (anim_timer > 24) { anim_timer = 0; anim_flag = (anim_flag + 1) & 7; }
        cycle_ship(pad);
        if (stb.status != ST_GAME) break;
        cycle_ball();
        cycle_bullets();
        cycle_enemies();
        cycle_doors();
        follow_ship();
        break;

    case ST_DEAD:
        wait_cnt++;
        if (wait_cnt > 40) {
            wait_cnt = 0;
            load_level(stb.level);
            stb.status = ST_GAME;
        }
        break;

    case ST_LEVELDONE:
        wait_cnt++;
        if (wait_cnt > 90) {
            wait_cnt = 0;
            stb.level++;
            stb.msg[0] = 0;
            if (stb.level >= stb_map_num) {
                stb.status = ST_WON;
                strcpy(stb.msg, "ALL DONE");
            } else {
                load_level(stb.level);
                stb.status = ST_GAME;
            }
        }
        break;

    case ST_GAMEOVER:
    case ST_WON:
        wait_cnt++;
        if (wait_cnt > 60 && (pad & (TRG_A | TRG_B | TRG_C))) {
            wait_cnt = 0;
            stb.msg[0] = 0;
            stb.status = ST_TITLE;
            stb.lives = 3;
            load_level(stb.level);
        }
        break;
    }

    stb.ship_x = (int)(ship_x / FACTOR);
    stb.ship_y = (int)(ship_y / FACTOR);
    stb.ship_angle = ship_angle;
    {
        int i, ne = 0, nb = 0;
        for (i = 0; i < MAX_ENEMIES; i++) if (enemy[i].alive) ne++;
        for (i = 0; i < MAX_BULLETS; i++) if (bullet[i].alive) nb++;
        stb.n_enemies = ne;
        stb.n_bullets = nb;
    }
}

/* ---- Selbsttest ---------------------------------------------------------- */
int stb_selfcheck(void)
{
    int i;
    if (map_sx <= 0 || map_sy <= 0) return 1;
    if (map_sx * map_sy > MAP_MAX_CELLS) return 2;
    if (n_door > MAX_DOORS || n_switch > MAX_SWITCHES || n_fuel > MAX_FUEL) return 3;
    if (ship_x < 0 || ship_y < 0) return 4;
    if (ship_x > (int32_t)(map_sx * 16) * FACTOR) return 5;
    if (ship_y > (int32_t)(map_sy * 16) * FACTOR) return 6;
    if (stb.fuel < 0 || stb.fuel > stb.fuel_max) return 7;
    if (atr_n < 0 || atr_n > MAX_ATRACT_P) return 8;
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (!enemy[i].alive) continue;
        if (enemy[i].type > 7) return 9;
    }
    return 0;
}

/* =========================================================================
 * Zeichnen
 *
 * Zeilenweise direkt ins Panelformat: die Kacheln liegen schon als RGB565 in
 * der richtigen Bitfolge vor, ein Zwischenpuffer waere reine Verschwendung.
 * Reihenfolge wie in der Vorlage: Hintergrund mit Parallaxe, Karte, Tueren,
 * Gegner, Ball, Traktorstrahl, Schiff -- zuletzt die Anzeige.
 * ========================================================================= */
static uint16_t col_hud_bg, col_hud_fg, col_hud_dim, col_fuel, col_fuel_low, col_ball;

void stb_render_init(void)
{
    col_hud_bg   = PBC_RGB(0x08, 0x0A, 0x10);
    col_hud_fg   = PBC_RGB(0xE8, 0xEC, 0xF4);
    col_hud_dim  = PBC_RGB(0x70, 0x7A, 0x8C);
    col_fuel     = PBC_RGB(0x30, 0xC8, 0x60);
    col_fuel_low = PBC_RGB(0xE0, 0x40, 0x20);
    col_ball     = PBC_RGB(0xFF, 0xD0, 0x40);
}

/* Kachelzeile mit waagerechtem Versatz (Tuerhaelften). */
static void blit_tile_row_off(uint16_t *row, int tile, int sx, int srow, int off)
{
    const uint16_t *s;
    int i;
    if (tile < 0 || tile >= TILE_NUM) return;
    s = &stb_tile[tile][srow * TILE_W];
    for (i = 0; i < TILE_W; i++) {
        int src = i - off, x = sx + i;
        uint16_t v;
        if ((unsigned)src >= TILE_W || (unsigned)x >= SCR_W) continue;
        v = s[src];
        if (v) row[x] = v;
    }
}

/* Eine Kachelzeile in die Bildzeile kopieren, 0 = durchsichtig. */
static void blit_tile_row(uint16_t *row, int tile, int sx, int srow)
{
    const uint16_t *s;
    int x0 = 0, n = TILE_W;
    if (tile < 0 || tile >= TILE_NUM) return;
    s = &stb_tile[tile][srow * TILE_W];
    if (sx < 0) { x0 = -sx; n += sx; sx = 0; }
    if (sx + n > SCR_W) n = SCR_W - sx;
    {
        int i;
        for (i = 0; i < n; i++) {
            uint16_t v = s[x0 + i];
            if (v) row[sx + i] = v;
        }
    }
}

/* Wie oben, aber deckend (fuer den Hintergrund) */
static void blit_tile_row_opaque(uint16_t *row, int tile, int sx, int srow)
{
    const uint16_t *s;
    int x0 = 0, n = TILE_W, i;
    if (tile < 0 || tile >= TILE_NUM) return;
    s = &stb_tile[tile][srow * TILE_W];
    if (sx < 0) { x0 = -sx; n += sx; sx = 0; }
    if (sx + n > SCR_W) n = SCR_W - sx;
    for (i = 0; i < n; i++)
        row[sx + i] = s[x0 + i];
}

/* Rechteck aus einem 32x32-Feld (Schiff) */
static void blit_ship_row(uint16_t *row, int sx, int srow)
{
    int i;
    if (srow < 0 || srow >= SHIP_H) return;
    for (i = 0; i < SHIP_W; i++) {
        int x = sx + i;
        uint16_t v = ship_rpix[srow * SHIP_W + i];
        if (v && (unsigned)x < SCR_W)
            row[x] = v;
    }
}

static void put_px(uint16_t *row, int x, uint16_t c)
{
    if ((unsigned)x < SCR_W) row[x] = c;
}

/* ---- Hintergrund mit Parallaxe ------------------------------------------ */
static void background_row(uint16_t *row, int py)
{
    int bg = stb_map[stb.level].bg;
    int wy = (stb.map_y * 3) / 4 + py;     /* Parallaxe 0,75 wie im Original */
    int j = wy >> 4, srow = wy & 15;
    int ta, tb, t, i;
    int bx = (stb.map_x * 3) / 4;

    switch (bg) {
    case 1:  ta = 295; tb = 315; break;
    case 2:  ta = 335; tb = 275; break;
    default: ta = 294; tb = 314; break;
    }
    if (j < 10) return;                    /* darueber ist leerer Raum */
    t = (j == 10) ? ta : tb;
    for (i = -(bx & 15); i < SCR_W; i += TILE_W)
        blit_tile_row_opaque(row, t, i, srow);
}

/* ---- Karte --------------------------------------------------------------- */
static void map_row(uint16_t *row, int py)
{
    int wy = stb.map_y + py;
    int ty = wy >> 4, srow = wy & 15;
    int tx0 = stb.map_x >> 4, ox = -(stb.map_x & 15);
    int tx;

    if (ty < 0 || ty >= map_sy) return;
    for (tx = tx0; tx <= tx0 + SCR_W / TILE_W + 1; tx++) {
        int off, c = tile_at(tx, ty, &off);
        if (c < 0) continue;
        if (off)
            blit_tile_row_off(row, c, ox + (tx - tx0) * TILE_W, srow, off);
        else
            blit_tile_row(row, c, ox + (tx - tx0) * TILE_W, srow);
    }
}

/* ---- Gegner, Ball, Traktorstrahl, Schiff --------------------------------- */
static void sprites_row(uint16_t *row, int py)
{
    int wy = stb.map_y + py, i;

    /* Gegner */
    for (i = 0; i < MAX_ENEMIES; i++) {
        ENEMY *e = &enemy[i];
        int ex, ey, t = -1, w = 16;
        if (!e->alive) continue;
        switch (e->type) {
        case 0: ex = (int)(e->x / FACTOR) - 8; ey = (int)(e->y / FACTOR) - 8;
                t = e->tile; break;
        case 3: case 7:
            /* Die Vorlage zeichnet einen Sockel plus gedrehten Turm.  Der
             * Kanonenkoerper steht ohnehin als Kachel in der Karte, der Turm
             * entfaellt hier -- Kacheln je Bild zu drehen waere teuer. */
            continue;
        case 4: {                          /* Panzer: zwei Haelften, 32x16 */
            int tmp = (e->state2 & 8) ? 0 : 2;
            int tt = e->dir < 3 ? e->dir : 0;
            ex = (int)e->x; ey = (int)e->y;
            t = 282 + 4 * tt + tmp; w = 32;
            break;
        }
        case 5: ex = (int)e->x; ey = (int)e->y; t = 461; w = 32; break;
        case 6: {                          /* Explosion */
            static const int16_t fr[6] = { 240, 241, 260, 261, 280, 281 };
            ex = (int)e->x; ey = (int)e->y;
            t = fr[(e->state >> 3) % 6];
            break;
        }
        default: continue;                 /* Kanonen stehen in der Karte     */
        }
        if (t < 0) continue;
        if (w == 32) {                     /* Panzer: 32 breit, 16 hoch */
            int r = wy - ey;
            if (r < 0 || r >= 16) continue;
            blit_tile_row(row, t, ex - stb.map_x, r);
            blit_tile_row(row, t + 1, ex - stb.map_x + 16, r);
        } else {
            if (wy < ey || wy >= ey + 16) continue;
            blit_tile_row(row, t, ex - stb.map_x, wy - ey);
        }
    }

    /* Ball */
    {
        int bx = (int)(ball_x / FACTOR), by = (int)(ball_y / FACTOR);
        /* 320 solange er liegt, 321 sobald er am Traktorstrahl haengt */
        if (wy >= by && wy < by + 16)
            blit_tile_row(row, stb.ball_state < 0 ? 320 : 321, bx - stb.map_x, wy - by);
    }

    /* Traktorstrahl */
    for (i = 0; i < atr_n; i++) {
        int ax = (int)(atr_x[i] / FACTOR) - stb.map_x;
        int ay = (int)(atr_y[i] / FACTOR);
        if (ay == wy) put_px(row, ax, col_ball);
    }

    /* Schiff */
    if (ship_state == 0 || (ship_state == 1 && (ship_anim & 2))) {
        int sxp = (int)(ship_x / FACTOR) - SHIP_W / 2 - stb.map_x;
        int syp = (int)(ship_y / FACTOR) - SHIP_H / 2;
        if (wy >= syp && wy < syp + SHIP_H)
            blit_ship_row(row, sxp, wy - syp);
    }
}

/* ---- Anzeige -------------------------------------------------------------- */
static void num_str(char *dst, unsigned v, int digits, char pad)
{
    char tmp[12];
    int n = 0;
    do { tmp[n++] = (char)('0' + v % 10u); v /= 10u; } while (v && n < 11);
    while (n < digits) tmp[n++] = pad;
    while (n--) *dst++ = tmp[n];
    *dst = 0;
}

static void hud_row(uint16_t *row, int py)
{
    int y = py - VIEW_H, x;
    char s[24];

    for (x = 0; x < SCR_W; x++)
        row[x] = col_hud_bg;
    if (y == 0) {
        for (x = 0; x < SCR_W; x++)
            row[x] = col_hud_dim;
        return;
    }

    /* Treibstoffbalken.  Linke Kante bei 21 statt 16 und die Schiffszahl
     * entsprechend weiter nach innen: die unteren Ecken des Panels sind
     * gerundet, dort verschwindet sonst je ein Stueck (CLAUDE.md Abschnitt 3,
     * sicherer Rand 20 px). */
    if (y >= 5 && y < 15) {
        int w = stb.fuel_max ? stb.fuel * 113 / stb.fuel_max : 0;
        uint16_t c = (stb.fuel * 4 < stb.fuel_max) ? col_fuel_low : col_fuel;
        for (x = 21; x < 137; x++)
            row[x] = (x == 21 || x == 136) ? col_hud_dim : col_hud_bg;
        for (x = 22; x < 22 + w && x < 136; x++)
            row[x] = c;
    }
    segfont_row(row, SCR_W, py, 145, VIEW_H + 4, &segfont_s, "SHIPS", col_hud_dim);
    s[0] = 0; num_str(s, (unsigned)(stb.lives < 0 ? 0 : stb.lives), 1, ' ');
    segfont_row(row, SCR_W, py, 211, VIEW_H + 4, &segfont_s, s, col_hud_fg);
}

/* ---- Titel und Schiffswahl ----------------------------------------------- */
static const char *const ship_name[3] = { "SHADOW RUNNER", "V-PANTHER 2", "X-TERMINATOR" };

static void overlay_row(uint16_t *row, int py)
{
    char s[32];
    int x;

    if (stb.status == ST_TITLE) {
        /* Der volle Name passt in der Segmentschrift nicht in eine Zeile.
         * Also "SUPER" klein darueber und die 2 kleiner hinter "TRANSBALL" --
         * beides bleibt im sicheren Rand von 20 px. */
        int wbig = segfont_text_w(&segfont_m, "TRANSBALL");
        int wtwo = segfont_text_w(&segfont_s, "2");
        int x1 = (SCR_W - (wbig + 6 + wtwo)) / 2;

        if (py >= 52 && py < 152) {
            for (x = 16; x < 224; x++)
                row[x] = (py == 52 || py == 151 || x == 16 || x == 223)
                         ? col_hud_dim : col_hud_bg;
        }
        segfont_row(row, SCR_W, py,
                    (SCR_W - segfont_text_w(&segfont_s, "SUPER")) / 2, 60,
                    &segfont_s, "SUPER", col_hud_dim);
        segfont_row(row, SCR_W, py, x1, 78, &segfont_m, "TRANSBALL", col_hud_fg);
        segfont_row(row, SCR_W, py, x1 + wbig + 6, 78, &segfont_s, "2", col_hud_fg);

        s[0] = 0; num_str(s, (unsigned)(stb.level + 1), 1, ' ');
        segfont_row(row, SCR_W, py, 62, 118, &segfont_s, "LEVEL", col_hud_dim);
        segfont_row(row, SCR_W, py, 134, 114, &segfont_m, s, col_fuel);

        if ((stb_render.blink & 32) == 0)
            segfont_row(row, SCR_W, py, 30, 200, &segfont_s, "A OR B = START", col_hud_fg);
        return;
    }

    if (stb.status == ST_SHIPSEL) {
        int i;
        for (i = 0; i < 3; i++) {
            int y0 = 70 + i * 44;
            if (py >= y0 - 6 && py < y0 + 26) {
                for (x = 12; x < 228; x++)
                    if (py == y0 - 6 || py == y0 + 25 || x == 12 || x == 227)
                        row[x] = (i == menu_sel) ? col_fuel : col_hud_dim;
            }
            segfont_row(row, SCR_W, py, 20, y0, &segfont_s, ship_name[i],
                        i == menu_sel ? col_hud_fg : col_hud_dim);
        }
        segfont_row(row, SCR_W, py, 40, 224, &segfont_s, "A OR B = CHOOSE", col_hud_dim);
        return;
    }

    if (stb.msg[0]) {
        const segfont_t *mf = (segfont_text_w(&segfont_l, stb.msg) + 24 <= 232)
                              ? &segfont_l : &segfont_m;
        int w = segfont_text_w(mf, stb.msg) + 24;
        int x0 = (SCR_W - w) / 2, y0 = 100;
        if (py >= y0 && py < y0 + 48) {
            for (x = x0; x < x0 + w; x++)
                if ((unsigned)x < SCR_W)
                    row[x] = (py == y0 || py == y0 + 47 || x == x0 || x == x0 + w - 1)
                             ? col_hud_dim : col_hud_bg;
            segfont_row(row, SCR_W, py, x0 + 12, y0 + 9, mf, stb.msg, col_hud_fg);
        }
    }
}

void stb_render_rows(uint16_t *dst, int y0, int rows)
{
    int r;
    for (r = 0; r < rows; r++) {
        int py = y0 + r, x;
        uint16_t *row = dst + (size_t)r * SCR_W;

        if (py < 0 || py >= SCR_H) {
            for (x = 0; x < SCR_W; x++) row[x] = 0;
            continue;
        }
        if (py >= VIEW_H) {
            hud_row(row, py);
            continue;
        }
        for (x = 0; x < SCR_W; x++)
            row[x] = 0;
        background_row(row, py);
        map_row(row, py);
        if (stb.status == ST_GAME || stb.status == ST_DEAD ||
            stb.status == ST_LEVELDONE || stb.status == ST_TITLE)
            sprites_row(row, py);
        overlay_row(row, py);
    }
}
