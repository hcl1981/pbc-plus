/* game.c -- the rules of Rushtown: driving, traffic and its lights, people,
 * the police, weapons, the jobs and what money buys. One call of
 * game_tick() is 1/50 s. */
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "game.h"

car_t cars[CARS];
ped_t peds[PEDS];
fx_t fxs[FXS];
shot_t shots[SHOTS];
player_t pl;
game_t G;

int rt_lang;

static uint32_t rs = 0x1234567u;
uint32_t rnd(void)
{
    rs ^= rs << 13;
    rs ^= rs >> 17;
    rs ^= rs << 5;
    return rs;
}
static float rndf(void) { return (float)(rnd() & 0xffff) / 65536.0f; }

static uint32_t btn, btn_prev;
#define HELD(b)    (btn & (b))
#define PRESSED(b) (btn & ~btn_prev & (b))

static float wrap(float a)
{
    while (a > PI) a -= 2 * PI;
    while (a < -PI) a += 2 * PI;
    return a;
}
static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static float dist2(float ax, float ay, float bx, float by)
{
    return (ax - bx) * (ax - bx) + (ay - by) * (ay - by);
}
static bool on_screen(float x, float y, int margin)
{
    return x > (float)(G.cam_x - margin) && x < (float)(G.cam_x + SCR_W + margin) &&
           y > (float)(G.cam_y - margin) && y < (float)(G.cam_y + SCR_H + margin);
}
static float px(void) { return pl.car >= 0 ? cars[pl.car].x : pl.x; }
static float py(void) { return pl.car >= 0 ? cars[pl.car].y : pl.y; }
static float pspeed(void)
{
    return pl.car >= 0 ? sqrtf(cars[pl.car].vx * cars[pl.car].vx + cars[pl.car].vy * cars[pl.car].vy) : 0;
}
static const float DX[4] = { 0, 1, 0, -1 }, DY[4] = { -1, 0, 1, 0 };

/* ---- small things ---------------------------------------------------------- */

static void message(const char *s, px_t col, int ticks)
{
    snprintf(G.msg, sizeof G.msg, "%s", s);
    G.msg_col = col;
    G.msg_t = ticks;
}

static void note(const char *s)
{
    snprintf(G.note, sizeof G.note, "%s", s);
    G.note_t = 150;
}

static void fx(int kind, float x, float y, float vx, float vy, int life)
{
    for (int i = 0; i < FXS; i++)
        if (!fxs[i].used) {
            fxs[i] = (fx_t){ 1, (uint8_t)kind, x, y, vx, vy, (int16_t)life };
            return;
        }
}

static void burst(int kind, float x, float y, int n, float speed)
{
    for (int i = 0; i < n; i++) {
        float a = rndf() * 2 * PI, v = speed * (0.3f + rndf());
        fx(kind, x, y, sinf(a) * v, cosf(a) * v, 14 + rnd_n(18));
    }
}

static bool in_yard(float x, float y)
{
    int tx = (int)x >> 4, ty = (int)y >> 4;
    return tx >= w_yard[0] && tx <= w_yard[2] && ty >= w_yard[1] && ty <= w_yard[3];
}

static void save(void)
{
    save_t s = { SAVE_MAGIC, G.cash, G.earned, G.tokens, (uint16_t)G.jobs, (uint16_t)G.bumped,
                 (uint16_t)pl.ammo, G.sound, (uint8_t)G.owns, (uint8_t)G.story, (uint8_t)rt_lang, { { 0 } }, (uint8_t)G.fails, (uint8_t)G.lives };
    int n = 0;
    for (int i = 0; i < CARS && n < 4; i++)     /* what stands in the garage stays there */
        if (cars[i].used && cars[i].keep == 2 && cars[i].hp > 0) {
            s.yard[n][0] = (uint8_t)(cars[i].model + 1);
            s.yard[n][1] = cars[i].colour;
            n++;
        }
    memcpy(G.yard, s.yard, sizeof G.yard);
    G.slot[G.mode] = s;
    plat_save(&s, G.mode);
}

const char *const rank_name[RANKS] = { "ROOKIE", "DRIVER", "WHEELMAN", "OUTLAW", "KINGPIN" };
int game_rank(void)
{
    static const int need[RANKS] = { 0, 400, 1500, 4000, 10000 };
    int r = 0;
    while (r + 1 < RANKS && G.earned >= need[r + 1])
        r++;
    return r;
}

static void pay(int amount)
{
    int before = game_rank();
    G.cash += amount;
    if (amount > 0)
        G.earned += amount;
    if (G.cash < 0)
        G.cash = 0;
    if (game_rank() > before) {
        char s[48];
        if (G.mode == MODE_FREE)                /* the free game's levels are its ranks */
            G.lives = LIVES;
        snprintf(s, sizeof s, L("NEW RANK: %s", "NEUER RANG: %s"), rank_name[game_rank()]);
        note(s);
        snd_sfx(SFX_RANK, 255);
    }
}

static void offence(float heat)
{
    int before = G.stars;
    G.heat += heat;
    if (G.heat > 520)
        G.heat = 520;
    G.unseen = 0;
    G.stars = G.heat >= 400 ? 4 : G.heat >= 220 ? 3 : G.heat >= 100 ? 2 : G.heat >= 30 ? 1 : 0;
    if (G.stars > before)
        snd_sfx(SFX_STAR, 255);
}

static void bust(void)
{
    if (G.ev)
        return;
    G.ev = EV_BUSTED;
    G.ev_t = 130;
    snd_sfx(SFX_BUSTED, 255);
    snd_say(SAY_ARREST);
    snd_siren(false);
}

static void hurt_player(float kx, float ky)
{
    if (pl.inv || G.ev)
        return;
    pl.hp--;
    pl.inv = 60;
    pl.kx = kx;
    pl.ky = ky;
    snd_sfx(SFX_BUMP, 255);
    burst(FX_SPARK, pl.x, pl.y, 4, 40);
    if (pl.hp <= 0) {
        G.ev = EV_KO;
        G.ev_t = 130;
        snd_sfx(SFX_FAIL, 255);
    }
}

/* Every kind of car is tuned to a station of its own; a motorbike has no
 * radio, and what belongs to the town listens to the police band. */
int game_station(void)
{
    static const int8_t tuned[CAR_MODELS] = {
        [CAR_COMPACT] = 0, [CAR_SEDAN] = 3, [CAR_SPORT] = 4, [CAR_VAN] = 5, [CAR_TAXI] = 1,
        [CAR_BUS] = 1, [CAR_TRUCK] = 2, [CAR_TOW] = 2, [CAR_BIKE] = -1,
        [CAR_POLICE] = MUSIC_POLICE, [CAR_AMBULANCE] = MUSIC_POLICE,
    };
    if (pl.car < 0 || cars[pl.car].hp <= 0 || !music_stations())
        return -1;
    return G.radio == RADIO_AUTO ? tuned[cars[pl.car].model] : G.radio;
}

/* ---- people: making them ---------------------------------------------------- */

static bool ped_ok(float x, float y)
{
    int t = tile_px(x, y);
    return !t_solid(t) && t != T_WATER;
}

static int ped_new(float x, float y, int look)
{
    for (int i = 0; i < PEDS; i++)
        if (!peds[i].used) {
            memset(&peds[i], 0, sizeof peds[i]);
            peds[i].used = 1;
            peds[i].x = x;
            peds[i].y = y;
            peds[i].look = (uint8_t)look;
            peds[i].dir = (uint8_t)rnd_n(4);
            peds[i].turn = (int16_t)(20 + rnd_n(80));
            return i;
        }
    return -1;
}

static void ped_flee(ped_t *p, float fromx, float fromy, int ticks)
{
    float dx = p->x - fromx, dy = p->y - fromy, d = sqrtf(dx * dx + dy * dy) + 0.01f;
    p->state = P_FLEE;
    p->vx = dx / d * 62;
    p->vy = dy / d * 62;
    p->timer = (int16_t)ticks;
}

/* everybody near runs: a shot, a crash */
static void panic(float x, float y, float r)
{
    for (int i = 0; i < PEDS; i++)
        if (peds[i].used && peds[i].state == P_WALK && dist2(peds[i].x, peds[i].y, x, y) < r * r)
            ped_flee(&peds[i], x, y, 120 + rnd_n(80));
}

/* ---- cars ------------------------------------------------------------------ */

static bool car_hits(const car_t *c, float x, float y, float a)
{
    float s = sinf(a), co = cosf(a);
    float hw = (float)art_car[c->model].w * 0.5f - 1.5f, hl = (float)art_car[c->model].h * 0.5f - 1.5f;
    static const float pt[8][2] = { {-1, 1}, {1, 1}, {-1, -1}, {1, -1}, {0, 1}, {0, -1}, {-1, 0}, {1, 0} };
    for (int i = 0; i < 8; i++) {
        float r = pt[i][0] * hw, f = pt[i][1] * hl;
        if (t_solid(tile_px(x + r * co + f * s, y + r * s - f * co)))
            return true;
    }
    return false;
}

static int car_new(int model, int colour, float x, float y, float a, int ai)
{
    for (int i = 0; i < CARS; i++)
        if (!cars[i].used) {
            memset(&cars[i], 0, sizeof cars[i]);
            cars[i].used = 1;
            cars[i].model = (uint8_t)model;
            cars[i].colour = (uint8_t)colour;
            cars[i].x = x;
            cars[i].y = y;
            cars[i].a = a;
            cars[i].ai = (uint8_t)ai;
            cars[i].hp = 100;
            return i;
        }
    return -1;
}

static bool car_near(float x, float y, float r)
{
    for (int i = 0; i < CARS; i++)
        if (cars[i].used && dist2(cars[i].x, cars[i].y, x, y) < r * r)
            return true;
    return false;
}

static void car_remove(int i)
{
    cars[i].used = 0;
    if (G.job_car == i)
        G.job_car = -1;
}

/* the nearest crossing and the way to it: for a car that left the roads */
static void regrid(car_t *c)
{
    c->direct = 0;
    c->to = (uint8_t)world_node_near(c->x, c->y);
    float dx = (float)nodes[c->to].x - c->x, dy = (float)nodes[c->to].y - c->y;
    c->dir = (uint8_t)(fabsf(dx) > fabsf(dy) ? (dx > 0 ? D_E : D_W) : (dy > 0 ? D_S : D_N));
}

/* whoever drove it runs away */
static void abandon(car_t *c)
{
    if (c->ai != AI_TRAFFIC && c->ai != AI_POLICE)
        return;
    if (c->block != BLOCK_LEFT) {
        int p = ped_new(c->x - cosf(c->a) * 11, c->y - sinf(c->a) * 11, c->model == CAR_POLICE ? LOOK_COP : rnd_n(96));
        if (p >= 0 && ped_ok(peds[p].x, peds[p].y))
            ped_flee(&peds[p], c->x, c->y, 260);
        else if (p >= 0)
            peds[p].used = 0;
    }
    c->ai = AI_NONE;
    c->chase = c->seek = c->svc = c->block = 0;
    c->hail = 0;
}

/* a hit at `speed`; nx, ny points from the car to what it hit */
static void impact(car_t *c, float speed, float nx, float ny)
{
    if (speed < 45)
        return;
    bool mine = c->ai == AI_PLAYER;
    /* which end or side took it */
    float f = nx * sinf(c->a) - ny * cosf(c->a), r = nx * cosf(c->a) + ny * sinf(c->a);
    int zone = fabsf(f) >= fabsf(r) ? (f > 0 ? DMG_FRONT : DMG_REAR) : (r > 0 ? DMG_RIGHT : DMG_LEFT);
    int add = (int)((speed - 35) * (mine ? 0.22f : 0.3f)) + c->dmg[zone];
    c->dmg[zone] = (uint8_t)(add > DMG_MAX ? DMG_MAX : add);
    if (on_screen(c->x, c->y, 20)) {
        snd_sfx(speed > 110 ? SFX_CRASH : SFX_BUMP, mine ? 255 : 120);
        burst(FX_SPARK, c->x, c->y, speed > 110 ? 8 : 3, 50);
    }
    if (c->hp > 0) {
        float frail = c->model == CAR_BIKE ? 2.2f : 1.0f / (0.6f + 0.4f * art_car[c->model].mass);
        c->hp -= (int16_t)((speed - 45) * (mine ? 0.11f : 0.2f) * frail);
        if (c->hp <= 0) {
            c->hp = 0;
            if (mine)
                message(L("WRECKED", "SCHROTT"), RGB(255, 96, 64), 100);
            else
                abandon(c);
        }
    }
}

/* one step of a car: thr -1..1 (engine and brake), steer -1..1, hb handbrake */
static void car_step(car_t *c, float thr, float steer, bool hb)
{
    const car_model_t *m = &art_car[c->model];
    float fx_ = sinf(c->a), fy_ = -cosf(c->a), rx = -fy_, ry = fx_;
    float vf = c->vx * fx_ + c->vy * fy_, vl = c->vx * rx + c->vy * ry;
    float vmax = m->vmax, accel = m->accel;
    int t = tile_px(c->x, c->y);
    bool rough = t_rough(t);

    /* what the dents do, and it is little: only an end that is a wreck
     * costs a tenth of the speed or of the pull, and a bent side makes
     * the car drift that way a touch. Looks worse than it drives. */
    int df = DMG_LEVEL(c->dmg[DMG_FRONT]), dr = DMG_LEVEL(c->dmg[DMG_REAR]);
    int pull = DMG_LEVEL(c->dmg[DMG_RIGHT]) - DMG_LEVEL(c->dmg[DMG_LEFT]);
    if (c->hp <= 0)
        thr = 0;
    else if (c->hp < 35)
        vmax *= 0.7f;
    if (df == 3)
        vmax *= 0.9f;
    if (dr == 3)
        accel *= 0.9f;
    if (fabsf(vf) > 25)
        steer = clampf(steer + (float)pull * 0.03f, -1, 1);
    if (rough)
        vmax *= 0.6f;
    if (thr > 0) {
        if (vf < vmax)
            vf += accel * thr * DT * (vf < 0 ? 2.0f : 1.0f);
    } else if (thr < 0) {
        if (vf > 4)
            vf += 330 * thr * DT;
        else if (vf > -vmax * 0.4f)
            vf += accel * 0.7f * thr * DT;
    }
    /* air and tyres slow it down; more so off the road and without the engine */
    float drag = 0.25f + (rough ? 1.3f : 0) + (thr == 0 ? 0.5f : 0) + (hb ? 1.6f : 0);
    vf -= vf * drag * DT;
    if (thr == 0 || hb) {
        float roll = (hb ? 160 : 35) * DT;
        vf = vf > roll ? vf - roll : vf < -roll ? vf + roll : 0;
    }
    if (vf > vmax)
        vf -= (vf - vmax) * 3 * DT;
    float grip = hb ? 1.8f : rough ? m->grip * 0.5f : m->grip;
    float keep = 1 - grip * DT;
    vl *= keep < 0 ? 0 : keep;

    /* steering needs speed, and turns the other way in reverse */
    float av = fabsf(vf), bite = clampf(av / 55.0f, 0, 1) * (1 - 0.35f * clampf(av / 240.0f, 0, 1));
    float na = c->a + steer * m->turn * bite * (vf < 0 ? -1 : 1) * (hb ? 1.35f : 1) * DT;

    c->vx = fx_ * vf + rx * vl;
    c->vy = fy_ * vf + ry * vl;
    if (!car_hits(c, c->x, c->y, na))
        c->a = wrap(na);
    float nx = c->x + c->vx * DT, ny = c->y + c->vy * DT;
    if (car_hits(c, nx, c->y, c->a)) {
        impact(c, fabsf(c->vx), c->vx > 0 ? 1.0f : -1.0f, 0);
        c->vx *= -0.3f;
        c->vy *= 0.8f;
    } else
        c->x = nx;
    if (car_hits(c, c->x, ny, c->a)) {
        impact(c, fabsf(c->vy), 0, c->vy > 0 ? 1.0f : -1.0f);
        c->vy *= -0.3f;
        c->vx *= 0.8f;
    } else
        c->y = ny;

    /* dust off the road, smoke from a sliding tyre or a sick engine */
    if (on_screen(c->x, c->y, 16)) {
        float back = (float)m->h * 0.4f;
        if (rough && av > 50 && (G.frame & 3) == 0)
            fx(FX_DUST, c->x - fx_ * back, c->y - fy_ * back, 0, 0, 16);
        if (fabsf(vl) > 45 && (G.frame & 1))
            fx(FX_SMOKE, c->x - fx_ * back, c->y - fy_ * back, 0, 0, 12);
        if ((c->hp < 35 || df == 3) && (G.frame & (c->hp <= 0 ? 3 : 7)) == 0)
            fx(FX_SMOKE, c->x + fx_ * back * 0.8f, c->y + fy_ * back * 0.8f, rndf() * 8 - 4, -14, 34);
    }
}

/* two or three circles along the car */
static int car_circles(const car_t *c, float out[3][2], float *r)
{
    const car_model_t *m = &art_car[c->model];
    float rad = (float)m->w * 0.5f, off = (float)m->h * 0.5f - rad;
    float s = sinf(c->a), co = cosf(c->a);
    int n = off > rad * 1.3f ? 3 : 2;
    out[0][0] = c->x + s * off;
    out[0][1] = c->y - co * off;
    out[1][0] = c->x - s * off;
    out[1][1] = c->y + co * off;
    out[2][0] = c->x;
    out[2][1] = c->y;
    *r = rad < 3 ? 3 : rad;
    return n;
}

static void cars_collide(void)
{
    for (int i = 0; i < CARS; i++) {
        if (!cars[i].used)
            continue;
        for (int j = i + 1; j < CARS; j++) {
            car_t *a = &cars[i], *b = &cars[j];
            if (!b->used || dist2(a->x, a->y, b->x, b->y) > 44 * 44)
                continue;
            float ca[3][2], cb[3][2], ra, rb;
            int na = car_circles(a, ca, &ra), nb = car_circles(b, cb, &rb);
            float ma = art_car[a->model].mass, mb = art_car[b->model].mass;
            for (int p = 0; p < na; p++)
                for (int q = 0; q < nb; q++) {
                    float dx = cb[q][0] - ca[p][0], dy = cb[q][1] - ca[p][1];
                    float d2 = dx * dx + dy * dy, rr = ra + rb;
                    if (d2 >= rr * rr || d2 < 0.01f)
                        continue;
                    float d = sqrtf(d2), nx = dx / d, ny = dy / d, over = rr - d;
                    float oa = over * mb / (ma + mb), ob = over * ma / (ma + mb);
                    /* apart, the lighter one more; but never into a wall */
                    if (!car_hits(a, a->x - nx * oa, a->y - ny * oa, a->a)) {
                        a->x -= nx * oa;
                        a->y -= ny * oa;
                    }
                    if (!car_hits(b, b->x + nx * ob, b->y + ny * ob, b->a)) {
                        b->x += nx * ob;
                        b->y += ny * ob;
                    }
                    float vn = (a->vx - b->vx) * nx + (a->vy - b->vy) * ny;
                    if (vn <= 0)
                        continue;
                    float jimp = 1.3f * vn / (1 / ma + 1 / mb);
                    a->vx -= jimp / ma * nx;
                    a->vy -= jimp / ma * ny;
                    b->vx += jimp / mb * nx;
                    b->vy += jimp / mb * ny;
                    bool a_mine = a->ai == AI_PLAYER, b_mine = b->ai == AI_PLAYER;
                    int a_model = a->model, b_model = b->model, a_ai = a->ai, b_ai = b->ai;
                    bool chasing = a_mine ? b->chase : a->chase;
                    impact(a, vn, nx, ny);
                    impact(b, vn, -nx, -ny);
                    if ((a_mine || b_mine) && vn > 60) {
                        int om = a_mine ? b_model : a_model, oai = a_mine ? b_ai : a_ai;
                        if (om == CAR_POLICE && oai == AI_POLICE)
                            offence(chasing ? 12 : 45);
                        else if (oai == AI_TRAFFIC)
                            offence(7);
                    }
                }
        }
    }
}

/* ---- shots ------------------------------------------------------------------- */

static void shoot(float x, float y, float a, bool mine)
{
    for (int i = 0; i < SHOTS; i++)
        if (!shots[i].used) {
            shots[i] = (shot_t){ 1, mine, x + sinf(a) * 5, y - cosf(a) * 5, sinf(a) * 380, -cosf(a) * 380, 22 };
            if (on_screen(x, y, 30))
                snd_sfx(SFX_SHOT, mine ? 255 : 150);
            return;
        }
}

static void shots_step(void)
{
    for (int i = 0; i < SHOTS; i++) {
        shot_t *s = &shots[i];
        if (!s->used)
            continue;
        for (int sub = 0; sub < 2 && s->used; sub++) {
            s->x += s->vx * DT * 0.5f;
            s->y += s->vy * DT * 0.5f;
            if (t_solid(tile_px(s->x, s->y))) {
                burst(FX_SPARK, s->x, s->y, 2, 30);
                s->used = 0;
                break;
            }
            for (int k = 0; k < CARS && s->used; k++) {
                car_t *c = &cars[k];
                if (!c->used || (s->mine && k == pl.car))
                    continue;
                float cc[3][2], r;
                int n = car_circles(c, cc, &r);
                for (int q = 0; q < n; q++)
                    if (dist2(s->x, s->y, cc[q][0], cc[q][1]) < r * r) {
                        impact(c, c->ai == AI_PLAYER ? 85 : 95, -s->vx, -s->vy);
                        if (s->mine && c->ai == AI_TRAFFIC)
                            c->horned = 300;    /* foot down, out of here */
                        s->used = 0;
                        break;
                    }
            }
            if (!s->used)
                break;
            if (s->mine) {
                for (int k = 0; k < PEDS; k++) {
                    ped_t *p = &peds[k];
                    if (!p->used || p->state == P_DEAD || dist2(s->x, s->y, p->x, p->y) > 5 * 5)
                        continue;
                    offence(p->state == P_COP || p->look == LOOK_COP ? 130 : 60);
                    p->state = P_DEAD;
                    p->timer = 900;
                    p->vx = s->vx * 0.1f;
                    p->vy = s->vy * 0.1f;
                    G.bumped++;
                    if (on_screen(p->x, p->y, 10))
                        snd_sfx(SFX_YELP, 220);
                    s->used = 0;
                    break;
                }
            } else if (pl.car < 0 && dist2(s->x, s->y, pl.x, pl.y) < 5 * 5) {
                hurt_player(s->vx * 0.15f, s->vy * 0.15f);
                s->used = 0;
            }
        }
        if (s->used && --s->life <= 0)
            s->used = 0;
    }
}

/* ---- people ---------------------------------------------------------------- */

static void cop_step(ped_t *p)
{
    float dx = px() - p->x, dy = py() - p->y, d2 = dx * dx + dy * dy, d = sqrtf(d2) + 0.01f;
    if (!G.stars || d2 > 260 * 260) {           /* nothing to do: an officer on the beat */
        p->state = P_WALK;
        return;
    }
    float nx = p->x + dx / d * 58 * DT, ny = p->y + dy / d * 58 * DT;
    if (ped_ok(nx, p->y)) p->x = nx;
    if (ped_ok(p->x, ny)) p->y = ny;
    p->a = atan2f(dx, -dy);
    p->anim += 2;
    if (G.ev)
        return;
    if (pl.car < 0) {
        if (d2 < 7 * 7)
            bust();
    } else if (d2 < 15 * 15 && pspeed() < 20) { /* pulls the door open */
        if (++p->timer > 25)
            bust();
    } else
        p->timer = 0;
    /* three stars and more: they shoot */
    if (p->cool > 0)
        p->cool--;
    else if (G.stars >= 3 && d2 < 110 * 110 && d2 > 12 * 12 && world_sight(p->x, p->y, px(), py())) {
        shoot(p->x, p->y, p->a + (rndf() - 0.5f) * 0.25f, false);
        p->cool = 75;
    }
}

static void peds_step(void)
{
    for (int i = 0; i < PEDS; i++) {
        ped_t *p = &peds[i];
        if (!p->used)
            continue;
        if (dist2(p->x, p->y, px(), py()) > 330 * 330) {
            p->used = 0;
            continue;
        }
        if (p->state == P_DEAD || p->state == P_DOWN) {
            /* slides to a stop and lies there */
            float nx = p->x + p->vx * DT, ny = p->y + p->vy * DT;
            if (ped_ok(nx, p->y)) p->x = nx; else p->vx = -p->vx * 0.4f;
            if (ped_ok(p->x, ny)) p->y = ny; else p->vy = -p->vy * 0.4f;
            p->vx *= 0.9f;
            p->vy *= 0.9f;
            p->a += (fabsf(p->vx) + fabsf(p->vy)) * 0.004f;
            if (--p->timer <= 0) {
                if (p->state == P_DEAD)
                    p->used = 0;
                else                            /* up again, and away */
                    ped_flee(p, px(), py(), 250);
            }
            continue;
        }
        if (p->state == P_COP) {
            cop_step(p);
            continue;
        }
        /* a fast car coming at them: out of the way */
        for (int k = 0; k < CARS && p->state == P_WALK; k++) {
            const car_t *c = &cars[k];
            if (!c->used)
                continue;
            float sp2 = c->vx * c->vx + c->vy * c->vy;
            if (sp2 < 70 * 70)
                continue;
            float dx = p->x - c->x, dy = p->y - c->y, d2 = dx * dx + dy * dy;
            if (d2 > 44 * 44 || dx * c->vx + dy * c->vy < 0.75f * sqrtf(d2 * sp2))
                continue;
            float sp = sqrtf(sp2), side = dx * -c->vy + dy * c->vx > 0 ? 1.0f : -1.0f;
            p->state = P_FLEE;
            p->vx = -c->vy / sp * side * 75;
            p->vy = c->vx / sp * side * 75;
            p->timer = 22;
            if (c->ai == AI_PLAYER && !G.say_wait && on_screen(p->x, p->y, 0)) {
                snd_say(rnd_n(2) ? SAY_HEY : SAY_WATCH);     /* now and then one of them says so */
                G.say_wait = 150 + rnd_n(200);
            }
        }
        if (p->state == P_FLEE) {
            float nx = p->x + p->vx * DT, ny = p->y + p->vy * DT;
            if (ped_ok(nx, p->y)) p->x = nx; else p->vx = -p->vx;
            if (ped_ok(p->x, ny)) p->y = ny; else p->vy = -p->vy;
            p->a = atan2f(p->vx, -p->vy);
            p->anim += 2;
            if (--p->timer <= 0)
                p->state = P_WALK;
            continue;
        }
        if (p->state == P_WAIT)
            continue;
        /* strolling: straight on while the ground ahead is for walking */
        bool ahead = t_walk(tile_px(p->x + DX[p->dir] * 7, p->y + DY[p->dir] * 7));
        if (!ahead || --p->turn <= 0) {
            int opts[4], n = 0;
            for (int d = 0; d < 4; d++)
                if (d != ((p->dir + 2) & 3) && t_walk(tile_px(p->x + DX[d] * 10, p->y + DY[d] * 10)))
                    opts[n++] = d;
            if (n)
                p->dir = (uint8_t)opts[rnd_n(n)];
            else if (!ahead)
                p->dir = (uint8_t)((p->dir + 2) & 3);
            p->turn = (int16_t)(40 + rnd_n(160));
        }
        float nx = p->x + DX[p->dir] * 17 * DT, ny = p->y + DY[p->dir] * 17 * DT;
        if (ped_ok(nx, ny)) {
            p->x = nx;
            p->y = ny;
        } else
            p->dir = (uint8_t)((p->dir + 2) & 3);
        p->a = (float)p->dir * (PI / 2);
        if (G.frame & 1)
            p->anim++;
    }
}

/* cars against people and against the player on foot */
static void cars_vs_people(void)
{
    for (int k = 0; k < CARS; k++) {
        car_t *c = &cars[k];
        if (!c->used)
            continue;
        float cc[3][2], r, speed = sqrtf(c->vx * c->vx + c->vy * c->vy);
        int n = car_circles(c, cc, &r);
        for (int i = 0; i < PEDS; i++) {
            ped_t *p = &peds[i];
            if (!p->used || p->state == P_WAIT || p->state == P_DEAD)
                continue;
            for (int q = 0; q < n; q++) {
                float dx = p->x - cc[q][0], dy = p->y - cc[q][1], d2 = dx * dx + dy * dy;
                if (d2 >= (r + 3) * (r + 3))
                    continue;
                float d = sqrtf(d2) + 0.01f;
                /* Hit at speed: down, and up again after a while. Hit hard,
                 * or driven over while lying there: that was it. */
                bool cop = p->state == P_COP || p->look == LOOK_COP;
                if (p->state == P_DOWN && speed > 30 && p->timer < 150) {
                    p->state = P_DEAD;
                    p->timer = 900;
                    if (on_screen(p->x, p->y, 10))
                        snd_sfx(SFX_BUMP, 200);
                    if (c->ai == AI_PLAYER)
                        offence(cop ? 110 : 50);
                    break;
                }
                if (speed > 48 && p->state != P_DOWN) {
                    bool run_over = speed > 105;
                    p->state = run_over ? P_DEAD : P_DOWN;
                    p->timer = run_over ? 900 : 170;
                    p->vx = c->vx * 0.7f + dx / d * 30;
                    p->vy = c->vy * 0.7f + dy / d * 30;
                    if (on_screen(p->x, p->y, 10))
                        snd_sfx(SFX_YELP, 200);
                    if (c->ai == AI_PLAYER) {
                        offence((cop ? 90 : 38) + (run_over ? 30 : 0));
                        G.bumped++;
                    }
                    break;
                } else if (ped_ok(cc[q][0] + dx / d * (r + 3), cc[q][1] + dy / d * (r + 3))) {
                    p->x = cc[q][0] + dx / d * (r + 3);
                    p->y = cc[q][1] + dy / d * (r + 3);
                }
            }
        }
        if (pl.car >= 0 || G.ev || G.state != ST_PLAY)
            continue;
        for (int q = 0; q < n; q++) {
            float dx = pl.x - cc[q][0], dy = pl.y - cc[q][1], d2 = dx * dx + dy * dy;
            if (d2 >= (r + 3) * (r + 3))
                continue;
            float d = sqrtf(d2) + 0.01f;
            if (speed > 55 && !pl.inv) {
                hurt_player(c->vx * 0.8f + dx / d * 40, c->vy * 0.8f + dy / d * 40);
                pl.inv = 75;
            } else if (ped_ok(cc[q][0] + dx / d * (r + 3), cc[q][1] + dy / d * (r + 3))) {
                pl.x = cc[q][0] + dx / d * (r + 3);
                pl.y = cc[q][1] + dy / d * (r + 3);
            }
        }
    }
}

/* ---- traffic --------------------------------------------------------------- */

bool game_lit(int node)
{
    const node_t *n = &nodes[node];
    return (n->e[0] != NO_NODE) + (n->e[1] != NO_NODE) + (n->e[2] != NO_NODE) + (n->e[3] != NO_NODE) >= 3;
}

/* 14 s a round: north-south goes for 6 s, a second of yellow, then east-west */
int game_light(int node, int dir)
{
    if (!game_lit(node))
        return 0;
    int t = (G.frame + node * 97) % 700;
    if (dir == D_N || dir == D_S)
        return t < 300 ? 0 : t < 350 ? 1 : 2;
    return t < 350 ? 2 : t < 650 ? 0 : 1;
}

static void lane_target(const car_t *c, float look, float *tx, float *ty)
{
    const node_t *n = &nodes[c->to];
    switch (c->dir) {
    case D_N: *tx = (float)n->x + 8; *ty = c->y - look; break;
    case D_S: *tx = (float)n->x - 8; *ty = c->y + look; break;
    case D_E: *ty = (float)n->y + 8; *tx = c->x + look; break;
    default:  *ty = (float)n->y - 8; *tx = c->x - look; break;
    }
}

/* at a crossing: the road to take next. Traffic wanders; who is heading
 * somewhere takes the road that brings it nearer. */
static void next_road(car_t *c)
{
    const node_t *n = &nodes[c->to];
    int back = (c->dir + 2) & 3, best = -1;
    float best_score = 1e12f;
    for (int d = 0; d < 4; d++) {
        if (n->e[d] == NO_NODE || d == back)
            continue;
        float score;
        if (c->seek)
            score = dist2((float)nodes[n->e[d]].x, (float)nodes[n->e[d]].y, c->tx, c->ty);
        else
            score = rndf() + (d == c->dir ? -0.45f : 0);
        if (score < best_score) {
            best_score = score;
            best = d;
        }
    }
    if (best < 0)
        best = n->e[back] != NO_NODE ? back : c->dir;
    if (n->e[best] == NO_NODE)
        return;
    c->from = c->to;
    c->to = n->e[best];
    c->dir = (uint8_t)best;
}

/* is something in the way within `reach` ahead */
static bool way_blocked(const car_t *c, float reach)
{
    float fx_ = sinf(c->a), fy_ = -cosf(c->a), half = (float)art_car[c->model].h * 0.5f;
    for (int k = 0; k < CARS; k++) {
        const car_t *o = &cars[k];
        if (!o->used || o == c)
            continue;
        float dx = o->x - c->x, dy = o->y - c->y, f = dx * fx_ + dy * fy_, l = -dx * fy_ + dy * fx_;
        if (f > 4 && f < reach + half + (float)art_car[o->model].h * 0.5f - 6 && fabsf(l) < 9)
            return true;
    }
    for (int k = 0; k < PEDS; k++) {
        const ped_t *o = &peds[k];
        if (!o->used || o->state == P_WAIT)
            continue;
        float dx = o->x - c->x, dy = o->y - c->y, f = dx * fx_ + dy * fy_, l = -dx * fy_ + dy * fx_;
        if (f > 4 && f < reach + half - 8 && fabsf(l) < 8)
            return true;
    }
    if (pl.car < 0 && G.state == ST_PLAY) {
        float dx = pl.x - c->x, dy = pl.y - c->y, f = dx * fx_ + dy * fy_, l = -dx * fy_ + dy * fx_;
        if (f > 4 && f < reach + half - 8 && fabsf(l) < 8)
            return true;
    }
    return false;
}

/* drive towards tx, ty at up to `want` */
static void drive_to(car_t *c, float tx, float ty, float want, bool careful)
{
    float diff = wrap(atan2f(tx - c->x, -(ty - c->y)) - c->a);
    float fx_ = sinf(c->a), fy_ = -cosf(c->a), vf = c->vx * fx_ + c->vy * fy_;
    float steer = clampf(diff * 2.6f, -1, 1);
    if (fabsf(diff) > 0.5f)
        want *= 0.6f;
    if (c->reverse > 0) {                       /* backing out of a jam */
        c->reverse--;
        car_step(c, -1, -steer, false);
        return;
    }
    if (c->horned > 0) {                        /* hooted at, or shot at: get a move on */
        c->horned--;
        want *= 1.35f;
        careful = false;
    }
    if (careful) {
        if (way_blocked(c, 22 + vf * 0.45f)) {
            if (++c->blocked < 150 || (c->blocked % 200) > 60)
                want = 0;
            if (c->blocked == 120 && on_screen(c->x, c->y, 0))
                snd_sfx(SFX_HORN, 60);
        } else if (c->blocked > 0)
            c->blocked -= 2;
    }
    if (want > 5 && fabsf(vf) < 5) {
        if (++c->stuck > 110) {
            c->stuck = 0;
            c->reverse = 45;
        }
    } else
        c->stuck = 0;
    float thr = vf < want - 4 ? 0.7f : vf > want + 12 ? -1.0f : vf > want + 3 ? 0.0f : 0.25f;
    if (want <= 0)
        thr = vf > 3 ? -1.0f : 0;
    car_step(c, thr, steer, false);
}

static void follow_roads(car_t *c, float want, bool lawful)
{
    const node_t *n = &nodes[c->to];
    float rem = ((float)n->x - c->x) * DX[c->dir] + ((float)n->y - c->y) * DY[c->dir];
    if (rem < 20)
        next_road(c);
    else if (lawful && rem > 36 && rem < 64 && game_light(c->to, c->dir))
        want = 0;                               /* red or yellow: stop before the zebra */
    float tx, ty;
    lane_target(c, 30, &tx, &ty);
    drive_to(c, tx, ty, want, lawful);
}

/* to c->tx, c->ty: over the roads, and straight at it once it is in sight */
static void seek_step(car_t *c, float want, bool lawful)
{
    float d2 = dist2(c->x, c->y, c->tx, c->ty);
    if (d2 < 120 * 120 && world_sight(c->x, c->y, c->tx, c->ty)) {
        c->direct = 1;
        drive_to(c, c->tx, c->ty, d2 < 34 * 34 ? 0 : d2 < 70 * 70 ? 45 : want, false);
    } else {
        if (c->direct)
            regrid(c);
        follow_roads(c, want * 0.85f, lawful);
    }
}

static void police_step(car_t *c)
{
    float tx = px(), ty = py(), d2 = dist2(c->x, c->y, tx, ty);
    if (c->block == BLOCK_LEFT) {               /* the officer is out on foot */
        car_step(c, 0, 0, true);
        if (!G.stars)
            c->ai = AI_NONE, c->block = 0, c->chase = 0;
        return;
    }
    if (c->block == BLOCK_WAIT) {               /* part of a road block */
        car_step(c, 0, 0, true);
        if (!G.stars) {
            c->block = 0;
            c->chase = 0;
            regrid(c);
        } else if (d2 < 85 * 85) {
            c->block = 0;
            c->chase = 1;
        }
        return;
    }
    if (!c->chase) {
        follow_roads(c, 68, true);
        if (G.stars && d2 < 150 * 150)
            c->chase = 1;
        return;
    }
    if (!G.stars) {                             /* lost: back into the traffic */
        c->chase = 0;
        c->seek = 0;
        regrid(c);
        return;
    }
    c->seek = 1;
    c->tx = tx;
    c->ty = ty;
    if (pl.car >= 0 && d2 < 120 * 120) {        /* where the player will be */
        c->tx += cars[pl.car].vx * 0.3f;
        c->ty += cars[pl.car].vy * 0.3f;
    }
    float want = 150 + 18 * (float)G.stars;
    if (d2 < 120 * 120 && world_sight(c->x, c->y, tx, ty)) {
        c->direct = 1;
        drive_to(c, c->tx, c->ty, d2 < 30 * 30 ? 40 : want, false);
    } else {
        if (c->direct)
            regrid(c);
        follow_roads(c, want * 0.85f, false);
    }
    /* close to a player who is on foot or standing: the officer gets out */
    if (d2 < 46 * 46 && c->vx * c->vx + c->vy * c->vy < 45 * 45 && (pl.car < 0 || pspeed() < 25) && !G.ev) {
        float sx = c->x - cosf(c->a) * 11, sy = c->y - sinf(c->a) * 11;
        if (!ped_ok(sx, sy)) {
            sx = c->x + cosf(c->a) * 11;
            sy = c->y + sinf(c->a) * 11;
        }
        int p = ped_ok(sx, sy) ? ped_new(sx, sy, LOOK_COP) : -1;
        if (p >= 0) {
            peds[p].state = P_COP;
            peds[p].cool = 60;
            snd_say(SAY_STOP);
            c->block = BLOCK_LEFT;
            if (on_screen(c->x, c->y, 0))
                snd_sfx(SFX_DOOR, 200);
        }
    }
}

/* ambulance and tow truck: to who was run over, to what was wrecked */
static void service_step(car_t *c)
{
    bool there = false;
    if (c->model == CAR_AMBULANCE) {
        const ped_t *p = &peds[c->svc_i];
        there = p->used && (p->state == P_DEAD || p->state == P_DOWN);
        if (there) { c->tx = p->x; c->ty = p->y; }
    } else {
        const car_t *w = &cars[c->svc_i];
        there = w->used && w->hp <= 0 && w->ai == AI_NONE;
        if (there) { c->tx = w->x; c->ty = w->y; }
    }
    if (!there && c->svc == SVC_GO) {           /* got up and left, or gone: never mind */
        c->svc = SVC_NONE;
        c->seek = 0;
        regrid(c);
        return;
    }
    if (c->svc == SVC_GO) {
        seek_step(c, 120, false);
        if (dist2(c->x, c->y, c->tx, c->ty) < 40 * 40 && c->vx * c->vx + c->vy * c->vy < 20 * 20) {
            c->svc = SVC_WORK;
            c->svc_t = 110;
        }
        return;
    }
    car_step(c, 0, 0, true);
    if (--c->svc_t > 0)
        return;
    if (there) {
        if (c->model == CAR_AMBULANCE)
            peds[c->svc_i].used = 0;            /* taken along */
        else
            car_remove(c->svc_i);
    }
    c->svc = SVC_NONE;
    c->seek = 0;
    regrid(c);
}

/* a point on a lane, outside the picture but not far: for new cars */
static bool spawn_point(float *x, float *y, float *a, int *from, int *to, int *dir)
{
    for (int tries = 0; tries < 8; tries++) {
        int n = rnd_n(nnodes), d = rnd_n(4);
        if (nodes[n].e[d] == NO_NODE)
            continue;
        const node_t *p = &nodes[n], *q = &nodes[nodes[n].e[d]];
        float t = 0.3f + rndf() * 0.4f;
        float sx = (float)p->x + (float)(q->x - p->x) * t, sy = (float)p->y + (float)(q->y - p->y) * t;
        if (d == D_N) sx += 8; else if (d == D_S) sx -= 8; else if (d == D_E) sy += 8; else sy -= 8;
        float d2 = dist2(sx, sy, px(), py());
        if (d2 > 400 * 400 || on_screen(sx, sy, 40) || car_near(sx, sy, 56))
            continue;
        *x = sx;
        *y = sy;
        *a = (float)d * (PI / 2);
        *from = n;
        *to = nodes[n].e[d];
        *dir = d;
        return true;
    }
    return false;
}

static int spawn_driving(int model, int colour, int ai)
{
    float x, y, a;
    int from, to, dir;
    if (!spawn_point(&x, &y, &a, &from, &to, &dir))
        return -1;
    int i = car_new(model, colour, x, y, a, ai);
    if (i >= 0) {
        cars[i].from = (uint8_t)from;
        cars[i].to = (uint8_t)to;
        cars[i].dir = (uint8_t)dir;
        cars[i].vx = sinf(a) * 40;
        cars[i].vy = -cosf(a) * 40;
    }
    return i;
}

/* two police cars across the crossing the player is heading for */
static void road_block(void)
{
    float vx = pl.car >= 0 ? cars[pl.car].vx : 0, vy = pl.car >= 0 ? cars[pl.car].vy : 0;
    int best = -1;
    float bs = -1e12f;
    for (int n = 0; n < nnodes; n++) {
        float dx = (float)nodes[n].x - px(), dy = (float)nodes[n].y - py(), d = sqrtf(dx * dx + dy * dy);
        if (d < 200 || d > 380 || on_screen((float)nodes[n].x, (float)nodes[n].y, 40) ||
            car_near((float)nodes[n].x, (float)nodes[n].y, 40))
            continue;
        float s = (dx * vx + dy * vy) / d - d * 0.1f;
        if (s > bs) {
            bs = s;
            best = n;
        }
    }
    if (best < 0)
        return;
    for (int k = 0; k < 2; k++) {
        int i = car_new(CAR_POLICE, 0, (float)nodes[best].x + (k ? 12.0f : -12.0f), (float)nodes[best].y + (k ? 5.0f : -5.0f),
                        PI / 2 + (k ? 0.25f : -0.25f), AI_POLICE);
        if (i >= 0) {
            cars[i].block = BLOCK_WAIT;
            cars[i].chase = 1;
            cars[i].to = (uint8_t)best;
        }
    }
}

static void spawner(void)
{
    int traffic = 0, parked = 0, police = 0, people = 0, blocks = 0, ambulance = 0, tow = 0;
    for (int i = 0; i < CARS; i++) {
        car_t *c = &cars[i];
        if (!c->used)
            continue;
        /* the garage keeps what stands in it */
        if (c->ai == AI_NONE && c->keep != 1)
            c->keep = (G.owns & OWN_GARAGE) && in_yard(c->x, c->y) && c->hp > 0 ? 2 : 0;
        if (c->ai != AI_PLAYER && !c->keep && dist2(c->x, c->y, px(), py()) > 440 * 440) {
            car_remove(i);
            continue;
        }
        if (c->ai == AI_TRAFFIC) traffic++;
        else if (c->ai == AI_POLICE) {
            if (c->chase) police++; else traffic++;
            blocks += c->block == BLOCK_WAIT;
        } else if (c->ai == AI_NONE) parked++;
        if (c->svc) {
            if (c->model == CAR_AMBULANCE) ambulance++; else tow++;
        }
    }
    for (int i = 0; i < PEDS; i++)
        people += peds[i].used;

    if (traffic < 10 && (G.frame & 7) == 0) {
        static const uint8_t mix[] = { CAR_COMPACT, CAR_COMPACT, CAR_SEDAN, CAR_SEDAN, CAR_SEDAN, CAR_VAN,
                                       CAR_VAN, CAR_TAXI, CAR_TAXI, CAR_SPORT, CAR_POLICE, CAR_BUS,
                                       CAR_TRUCK, CAR_BIKE, CAR_BIKE, CAR_SEDAN, CAR_COMPACT };
        int model = mix[rnd_n((int)sizeof mix)];
        /* the last colour is the pink of the racer to be stolen: nobody else has it */
        spawn_driving(model, rnd_n(CAR_COLOURS - 1), model == CAR_POLICE ? AI_POLICE : AI_TRAFFIC);
    }
    int wanted = G.stars > 3 ? 4 : G.stars;
    if (police < wanted && (G.frame % 90) == 0) {
        int i = spawn_driving(CAR_POLICE, 0, AI_POLICE);
        if (i >= 0)
            cars[i].chase = 1;
    }
    if (G.stars >= 3 && !blocks && (G.frame % 450) == 200)
        road_block();
    if (parked < 7 && (G.frame & 3) == 1) {
        int tx = ((int)px() >> 4) + rnd_n(41) - 20, ty = ((int)py() >> 4) + rnd_n(41) - 20;
        static const uint8_t mix[] = { CAR_COMPACT, CAR_SEDAN, CAR_SEDAN, CAR_VAN, CAR_SPORT, CAR_BIKE, CAR_COMPACT };
        int model = mix[rnd_n((int)sizeof mix)];
        if (tile_at(tx, ty) == T_LOT && tile_at(tx, ty + 1) == T_LOT && !in_yard((float)(tx * TILE), (float)(ty * TILE))) {
            float cx = (float)(tx * TILE + 8), cy = (float)(ty * TILE + TILE);
            if (!on_screen(cx, cy, 20) && !car_near(cx, cy, 24))
                car_new(model, rnd_n(CAR_COLOURS - 1), cx, cy, rnd_n(2) ? 0 : PI, AI_NONE);
        } else if (tile_at(tx, ty) == T_BAY && tile_at(tx + 1, ty) == T_BAY) {     /* along Main Street */
            float cx = (float)(tx * TILE + TILE), cy = (float)(ty * TILE + 8);
            if (!on_screen(cx, cy, 20) && !car_near(cx, cy, 30))
                car_new(model, rnd_n(CAR_COLOURS - 1), cx, cy, rnd_n(2) ? PI / 2 : -PI / 2, AI_NONE);
        }
    }
    if (people < 16 && (G.frame & 3) == 2) {
        int tx = ((int)px() >> 4) + rnd_n(31) - 15, ty = ((int)py() >> 4) + rnd_n(35) - 17;
        float cx = (float)(tx * TILE + 8), cy = (float)(ty * TILE + 8);
        if (t_walk(tile_at(tx, ty)) && !on_screen(cx, cy, 12))
            ped_new(cx, cy, rnd_n(96));
    }
    /* somebody lies in the road, something is wrecked: send for help */
    if ((G.frame & 63) == 5 && !ambulance)
        for (int i = 0; i < PEDS; i++)
            if (peds[i].used && (peds[i].state == P_DEAD || (peds[i].state == P_DOWN && peds[i].timer > 120))) {
                int k = spawn_driving(CAR_AMBULANCE, 0, AI_TRAFFIC);
                if (k >= 0) {
                    cars[k].svc = SVC_GO;
                    cars[k].svc_i = (uint8_t)i;
                    cars[k].seek = 1;
                }
                break;
            }
    if ((G.frame & 63) == 37 && !tow)
        for (int i = 0; i < CARS; i++)
            if (cars[i].used && cars[i].hp <= 0 && cars[i].ai == AI_NONE && !cars[i].keep) {
                int k = spawn_driving(CAR_TOW, 0, AI_TRAFFIC);
                if (k >= 0) {
                    cars[k].svc = SVC_GO;
                    cars[k].svc_i = (uint8_t)i;
                    cars[k].seek = 1;
                }
                break;
            }
}

/* ---- jobs ------------------------------------------------------------------ */

static void targets_clear(void)
{
    for (int i = 0; i < TARGETS; i++)
        G.tgt[i].on = false;
}

bool game_target(float *x, float *y)
{
    int best = -1;
    float bd = 1e12f;
    for (int i = 0; i < TARGETS; i++)
        if (G.tgt[i].on && dist2(G.tgt[i].x, G.tgt[i].y, px(), py()) < bd) {
            bd = dist2(G.tgt[i].x, G.tgt[i].y, px(), py());
            best = i;
        }
    if (best < 0)
        return false;
    *x = G.tgt[best].x;
    *y = G.tgt[best].y;
    return true;
}

/* the target the player is at, or -1 */
static int target_hit(void)
{
    for (int i = 0; i < TARGETS; i++)
        if (G.tgt[i].on && dist2(G.tgt[i].x, G.tgt[i].y, px(), py()) < 17 * 17)
            return i;
    return -1;
}

/* the middle of a stretch of road, dmin..dmax away from fx, fy */
static void road_point(float fx_, float fy_, float dmin, float dmax, float *x, float *y)
{
    float bx = (float)nodes[0].x, by = (float)nodes[0].y, berr = 1e12f;
    int isle = world_isle(fx_, fy_);
    if (isle < 0)
        isle = node_isle[world_node_near(fx_, fy_)];
    for (int tries = 0; tries < 120; tries++) {
        int n = rnd_n(nnodes), d = rnd_n(4);
        if (nodes[n].e[d] == NO_NODE || node_isle[n] != isle || node_isle[nodes[n].e[d]] != isle)
            continue;                           /* jobs stay on the island they start on */
        float mx = (float)(nodes[n].x + nodes[nodes[n].e[d]].x) * 0.5f;
        float my = (float)(nodes[n].y + nodes[nodes[n].e[d]].y) * 0.5f;
        if (!t_road(tile_px(mx, my)))
            continue;
        float dd = sqrtf(dist2(mx, my, fx_, fy_));
        float err = dd < dmin ? dmin - dd : dd > dmax ? dd - dmax : 0;
        if (err < berr) {
            berr = err;
            bx = mx;
            by = my;
            if (err == 0)
                break;
        }
    }
    *x = bx;
    *y = by;
}

/* the kerb next to a point on a road: 24 pixels off its middle line */
static void kerb(float x, float y, float *kx, float *ky, bool *north_south)
{
    *north_south = t_road(tile_px(x, y - 28)) && t_road(tile_px(x, y + 28));
    *kx = x + (*north_south ? 24 : 0);
    *ky = y + (*north_south ? 0 : 24);
}

static void job_target(float x, float y, float seconds, const char *text)
{
    targets_clear();
    G.tgt[0].x = x;
    G.tgt[0].y = y;
    G.tgt[0].on = true;
    G.timed = seconds > 0;
    G.time_wins = false;
    G.job_time = seconds;
    snprintf(G.job_text, sizeof G.job_text, "%s", text);
}

static void job_end(bool won, const char *why)
{
    if (G.job == J_NONE)
        return;
    if (G.job_car >= 0) {
        cars[G.job_car].keep = 0;
        cars[G.job_car].tail = 0;
        G.job_car = -1;
    }
    for (int i = 0; i < PEDS; i++)
        if (peds[i].used && peds[i].state == P_WAIT)
            peds[i].state = P_WALK;
    if (won) {
        int bonus = G.reward + G.reward * (G.jobs > 20 ? 20 : G.jobs) / 20;
        char s[48];
        G.jobs++;
        snprintf(s, sizeof s, L("JOB DONE +$%d", "ERLEDIGT +$%d"), bonus);
        message(s, RGB(120, 255, 120), 150);
        snd_sfx(SFX_DONE, 255);
        pay(bonus);
        save();
    } else {
        message(why ? why : G.job == J_STORY ? L("MISSION FAILED", "MISSION GESCHEITERT") : L("JOB FAILED", "GESCHEITERT"),
                RGB(255, 96, 64), 130);
        snd_sfx(SFX_FAIL, 255);
    }
    G.job = J_NONE;
    targets_clear();
    G.timed = G.time_wins = false;
    G.job_text[0] = 0;
    G.phone_wait = 150;
    G.story_wait = 200;
}

/* a car for a job, parked half on the footway near a road some way off */
static int job_car_at_kerb(int model, int colour, float dmin, float dmax)
{
    float rx_, ry_, x, y;
    bool ns;
    road_point(pl.x, pl.y, dmin, dmax, &rx_, &ry_);
    kerb(rx_, ry_, &x, &y, &ns);
    if (ns) x -= 5; else y -= 5;
    int c = car_new(model, colour, x, y, ns ? 0 : PI / 2, AI_NONE);
    if (c < 0) {                                /* no room for it: make room */
        for (int i = 0; i < CARS; i++)
            if (cars[i].used && cars[i].ai != AI_PLAYER && !cars[i].keep && !on_screen(cars[i].x, cars[i].y, 20)) {
                car_remove(i);
                break;
            }
        c = car_new(model, colour, x, y, ns ? 0 : PI / 2, AI_NONE);
    }
    if (c >= 0)
        cars[c].keep = 1;
    return c;
}

static void fare_at_kerb(float x, float y)
{
    float kx, ky;
    bool ns;
    kerb(x, y, &kx, &ky, &ns);
    int p = ped_new(kx, ky, rnd_n(96));
    if (p >= 0)
        peds[p].state = P_WAIT;
}

static void job_begin(int type)
{
    float x, y, d;
    G.job = type;
    G.stage = 0;
    G.job_car = -1;
    G.reward = 0;
    G.spot = 0;
    G.lost = 0;
    snd_sfx(SFX_JOB, 255);
    switch (type) {
    case J_COURIER:
        road_point(pl.x, pl.y, 220, 420, &x, &y);
        d = sqrtf(dist2(x, y, pl.x, pl.y));
        job_target(x, y, d / 40 + 14, L("PICK UP THE PARCEL", "PAKET ABHOLEN"));
        message(L("COURIER JOB", "KURIERFAHRT"), RGB(255, 220, 64), 110);
        break;
    case J_TAXI:
        road_point(pl.x, pl.y, 200, 400, &x, &y);
        d = sqrtf(dist2(x, y, pl.x, pl.y));
        job_target(x, y, d / 40 + 16, L("PICK UP A FARE BY CAR", "FAHRGAST MIT AUTO HOLEN"));
        fare_at_kerb(x, y);
        message(L("TAXI JOB", "TAXIFAHRT"), RGB(255, 220, 64), 110);
        break;
    case J_RACE:
        road_point(pl.x, pl.y, 200, 340, &x, &y);
        d = sqrtf(dist2(x, y, pl.x, pl.y));
        G.left = 6;
        job_target(x, y, d / 45 + 14, L("CHECKPOINT 1 OF 6", "KONTROLLPUNKT 1 VON 6"));
        message(L("CHECKPOINT RUN", "ZEITFAHRT"), RGB(255, 220, 64), 110);
        break;
    case J_MULTI:
        targets_clear();
        for (int i = 0; i < TARGETS; i++) {
            road_point(pl.x, pl.y, 160 + 90.0f * (float)i, 260 + 110.0f * (float)i, &x, &y);
            G.tgt[i].x = x;
            G.tgt[i].y = y;
            G.tgt[i].on = true;
        }
        G.left = TARGETS;
        G.timed = true;
        G.time_wins = false;
        G.job_time = 80;
        snprintf(G.job_text, sizeof G.job_text, L("4 DROPS, ANY ORDER", "4 ABGABEN, EGAL WIE"));
        message(L("ROUND TRIP", "RUNDFAHRT"), RGB(255, 220, 64), 110);
        break;
    case J_HOTCAR:
        G.job_car = job_car_at_kerb(CAR_SPORT, CAR_COLOURS - 1, 240, 460);
        if (G.job_car < 0)
            break;
        job_target(cars[G.job_car].x, cars[G.job_car].y, 0, L("STEAL THE PINK RACER", "ROSA RENNER STEHLEN"));
        message(L("HOT WHEELS", "HEISSE REIFEN"), RGB(255, 220, 64), 110);
        break;
    case J_FRAGILE:
        G.job_car = job_car_at_kerb(CAR_VAN, 3, 200, 400);
        if (G.job_car < 0)
            break;
        job_target(cars[G.job_car].x, cars[G.job_car].y, 0, L("GET THE WHITE VAN", "WEISSEN LIEFERWAGEN HOLEN"));
        message(L("FRAGILE LOAD", "ZERBRECHLICH"), RGB(255, 220, 64), 110);
        break;
    case J_HEAT:
        G.heat = 0;
        offence(230);
        targets_clear();
        G.timed = false;
        snprintf(G.job_text, sizeof G.job_text, L("LOSE THE POLICE", "POLIZEI ABHÄNGEN"));
        message(L("HEAT IS ON", "ES WIRD HEISS"), RGB(255, 220, 64), 110);
        break;
    case J_GETAWAY:
        road_point(pl.x, pl.y, 220, 420, &x, &y);
        d = sqrtf(dist2(x, y, pl.x, pl.y));
        job_target(x, y, d / 40 + 18, L("PICK UP THE CREW BY CAR", "CREW MIT AUTO ABHOLEN"));
        fare_at_kerb(x, y);
        message(L("GETAWAY", "FLUCHTWAGEN"), RGB(255, 220, 64), 110);
        break;
    default: {                                  /* J_TAIL */
        G.job = J_TAIL;
        int c = spawn_driving(CAR_SEDAN, 4, AI_TRAFFIC);
        for (int tries = 0; c < 0 && tries < 40; tries++)
            c = spawn_driving(CAR_SEDAN, 4, AI_TRAFFIC);
        G.job_car = c;
        if (c < 0)
            break;
        cars[c].keep = 1;
        cars[c].tail = 1;
        job_target(cars[c].x, cars[c].y, 0, L("FIND THE BLACK SEDAN", "SCHWARZE LIMOUSINE FINDEN"));
        message(L("SHADOW", "SCHATTEN"), RGB(255, 220, 64), 110);
        break;
    }
    }
    if ((type == J_HOTCAR || type == J_FRAGILE || type >= J_TAIL) && G.job_car < 0) {
        G.job = J_NONE;                         /* the town was too full for it */
        G.phone_wait = 50;
    }
}

void game_debug_job(int type) { job_begin(type); }

#if RT_STORY
#include "story.c"                       /* part of this file: it uses everything above */
#else
bool story_giver(giver_t *g) { (void)g; return false; }
int story_count(void) { return 0; }
void story_brief(const char **name, const char **title, const char *const **lines, int *reward)
{
    (void)name; (void)title; (void)lines; (void)reward;
}
int story_portrait(void) { return -1; }
int story_chapter(int mission) { (void)mission; return 0; }
const char *story_chapter_name(int mission) { (void)mission; return ""; }
const char *story_caption(int pic) { (void)pic; return ""; }
#endif

static void jobs_step(void)
{
    static const uint8_t order[] = { J_COURIER, J_TAXI, J_RACE, J_MULTI, J_HOTCAR, J_FRAGILE, J_HEAT, J_GETAWAY, J_TAIL };
    static const uint8_t open[RANKS] = { 2, 4, 6, 9, 9 };   /* how many of them at each rank */
    if (G.phone_wait > 0)
        G.phone_wait--;
#if RT_STORY
    if (story_step())
        return;
#endif
    if (G.job == J_NONE) {
        if (pl.car < 0 && !G.phone_wait)
            for (int p = 0; p < PHONES; p++)
                if (dist2(pl.x, pl.y, (float)w_phone[p].x, (float)w_phone[p].y) < 11 * 11) {
                    job_begin(order[(p + G.jobs) % open[game_rank()]]);
                    break;
                }
        return;
    }
    if (G.timed) {
        G.job_time -= DT;
        if (G.job_time <= 0) {
            if (G.time_wins) {
                G.reward = 320;
                job_end(true, NULL);
            } else
                job_end(false, L("OUT OF TIME", "ZEIT ABGELAUFEN"));
            return;
        }
    }
    bool driving = pl.car >= 0;
    float speed = pspeed(), x, y, d;
    int hit = target_hit();
    char s[48];
    car_t *jc = G.job_car >= 0 ? &cars[G.job_car] : NULL;

    switch (G.job) {
    case J_COURIER:
        if (hit < 0)
            break;
        if (G.stage == 0) {
            G.stage = 1;
            road_point(px(), py(), 420, 760, &x, &y);
            d = sqrtf(dist2(x, y, px(), py()));
            G.reward = 90 + (int)(d / 5);
            job_target(x, y, d / 48 + 12, L("DELIVER THE PARCEL", "PAKET ABLIEFERN"));
            snd_sfx(SFX_COIN, 255);
        } else
            job_end(true, NULL);
        break;
    case J_TAXI:
    case J_GETAWAY:
        if (hit < 0 || !driving || speed > 30)
            break;
        if (G.stage == 0) {
            G.stage = 1;
            for (int i = 0; i < PEDS; i++)
                if (peds[i].used && peds[i].state == P_WAIT)
                    peds[i].used = 0;
            road_point(px(), py(), 420, 800, &x, &y);
            d = sqrtf(dist2(x, y, px(), py()));
            snd_sfx(SFX_DOOR, 255);
            if (G.job == J_TAXI) {
                G.reward = 110 + (int)(d / 4);
                job_target(x, y, d / 46 + 12, L("TAKE THE FARE THERE", "FAHRGAST HINBRINGEN"));
            } else {                            /* and now everybody is after them */
                G.reward = 380;
                job_target(x, y, 0, L("TO THE HIDEOUT", "ZUM VERSTECK"));
                offence(230);
            }
        } else {
            if (G.job == J_TAXI)
                G.reward += (int)G.job_time * 3;
            else {
                G.heat = 0;
                G.stars = 0;
            }
            job_end(true, NULL);
        }
        break;
    case J_RACE:
        if (hit < 0 || !driving)
            break;
        snd_sfx(SFX_COIN, 255);
        G.reward += 55;
        if (--G.left == 0) {
            G.reward += (int)G.job_time * 4;
            job_end(true, NULL);
            break;
        }
        road_point(px(), py(), 220, 360, &x, &y);
        d = sqrtf(dist2(x, y, px(), py()));
        snprintf(s, sizeof s, L("CHECKPOINT %d OF 6", "KONTROLLPUNKT %d VON 6"), 7 - G.left);
        job_target(x, y, G.job_time + d / 62 + 2, s);
        break;
    case J_MULTI:
        if (hit < 0)
            break;
        G.tgt[hit].on = false;
        snd_sfx(SFX_COIN, 255);
        G.reward += 70;
        if (--G.left == 0) {
            G.reward += (int)G.job_time * 2;
            job_end(true, NULL);
        } else
            snprintf(G.job_text, sizeof G.job_text, L("%d DROP%s TO GO", "NOCH %d ABGABE%s"), G.left,
                     G.left > 1 ? L("S", "N") : "");
        break;
    case J_HOTCAR:
    case J_FRAGILE:
        if (!jc || !jc->used || jc->hp <= 0) {
            job_end(false, L("IT IS A WRECK", "NUR NOCH SCHROTT"));
            break;
        }
        if (G.stage == 0) {
            G.tgt[0].x = jc->x;
            G.tgt[0].y = jc->y;
            if (pl.car != G.job_car)
                break;
            G.stage = 1;
            if (G.job == J_HOTCAR) {
                offence(110);
                int far = dist2(pl.x, pl.y, (float)w_paint[0].x, (float)w_paint[0].y) >
                          dist2(pl.x, pl.y, (float)w_paint[1].x, (float)w_paint[1].y) ? 0 : 1;
                job_target((float)w_paint[far].x, (float)w_paint[far].y, 0, L("TO THE PAINT SHOP", "ZUR LACKIEREREI"));
            } else {
                road_point(px(), py(), 480, 820, &x, &y);
                job_target(x, y, 0, L("DELIVER IT IN ONE PIECE", "HEIL ABLIEFERN"));
            }
            break;
        }
        if (G.job == J_FRAGILE && jc->hp < 45) {
            job_end(false, L("THE LOAD IS SMASHED", "DIE LADUNG IST HIN"));
            break;
        }
        if (pl.car == G.job_car && hit >= 0 && speed < 35) {
            G.reward = G.job == J_HOTCAR ? 160 + jc->hp * 3 : 80 + jc->hp * 4;
            int c = G.job_car;
            pl.car = -1;
            pl.x = cars[c].x;
            pl.y = cars[c].y + 22;
            if (!ped_ok(pl.x, pl.y))
                pl.y = cars[c].y - 22;
            car_remove(c);
            if (G.job == J_HOTCAR) {
                G.heat = 0;
                G.stars = 0;
            }
            job_end(true, NULL);
        }
        break;
    case J_HEAT:
        if (G.stars == 0) {
            G.reward = 320;
            job_end(true, NULL);
        }
        break;
    case J_TAIL:
        if (!jc || !jc->used || jc->hp <= 0 || jc->ai != AI_TRAFFIC) {
            job_end(false, L("THEY ARE GONE", "SIE SIND WEG"));
            break;
        }
        G.tgt[0].x = jc->x;
        G.tgt[0].y = jc->y;
        d = sqrtf(dist2(jc->x, jc->y, px(), py()));
        if (G.stage == 0) {
            if (d < 150) {                      /* found: now stay behind it */
                G.stage = 1;
                G.timed = true;
                G.time_wins = true;
                G.job_time = 35;
                snprintf(G.job_text, sizeof G.job_text, L("FOLLOW, NOT TOO CLOSE", "FOLGEN, NICHT ZU DICHT"));
            }
            break;
        }
        if (d < 46)
            G.spot += 45 * DT;
        else if (G.spot > 0)
            G.spot -= 15 * DT;
        if (G.spot >= 100)
            job_end(false, L("THEY SAW YOU", "SIE HABEN DICH BEMERKT"));
        else if (d > 240) {
            if (++G.lost > 150)
                job_end(false, L("YOU LOST THEM", "ABGEHÄNGT"));
        } else
            G.lost = 0;
        break;
    }
}

/* ---- the player ------------------------------------------------------------ */

static void respawn(spot_t at)
{
    if (pl.car >= 0) {
        cars[pl.car].ai = AI_NONE;
        pl.car = -1;
    }
    pl.x = (float)at.x;
    pl.y = (float)at.y;
    pl.hp = 3;
    pl.inv = 100;
    pl.kx = pl.ky = 0;
    G.heat = 0;
    G.stars = 0;
    G.camfx = pl.x - SCR_W / 2;
    G.camfy = pl.y - SCR_H / 2;
    for (int i = 0; i < CARS; i++)
        if (cars[i].used && cars[i].ai == AI_POLICE)
            cars[i].chase = 0;
}

/* a hailed taxi takes the player to the job's target or the nearest phone */
static void ride(car_t *taxi)
{
    float x, y;
    if (!game_target(&x, &y)) {
        float bd = 1e12f;
        x = pl.x;
        y = pl.y;
        for (int p = 0; p < PHONES; p++) {
            float d = dist2(pl.x, pl.y, (float)w_phone[p].x, (float)w_phone[p].y);
            if (d > 60 * 60 && d < bd) {
                bd = d;
                x = (float)w_phone[p].x;
                y = (float)w_phone[p].y;
            }
        }
    }
    int fare = 15 + (int)(sqrtf(dist2(x, y, pl.x, pl.y)) / 25);
    if (G.cash < fare) {
        char s[48];
        snprintf(s, sizeof s, L("THE FARE IS $%d", "DIE FAHRT KOSTET $%d"), fare);
        note(s);
        return;
    }
    pay(-fare);
    taxi->hail = 0;
    taxi->seek = 0;
    regrid(taxi);
    G.ride_x = x;
    G.ride_y = y;
    G.ev = EV_RIDE;
    G.ev_t = 70;
    snprintf(G.note, sizeof G.note, L("TAXI RIDE -$%d", "TAXIFAHRT -$%d"), fare);
    G.note_t = 120;
    snd_sfx(SFX_DOOR, 255);
}

static void ride_end(void)
{
    /* out at the nearest place to stand */
    int tx = (int)G.ride_x >> 4, ty = (int)G.ride_y >> 4;
    for (int r = 0; r < 8; r++)
        for (int j = -r; j <= r; j++)
            for (int i = -r; i <= r; i++)
                if ((i == -r || i == r || j == -r || j == r) && t_walk(tile_at(tx + i, ty + j))) {
                    pl.x = (float)((tx + i) * TILE + 8);
                    pl.y = (float)((ty + j) * TILE + 8);
                    G.camfx = pl.x - SCR_W / 2;
                    G.camfy = pl.y - SCR_H / 2;
                    if (G.timed && !G.time_wins)
                        G.job_time -= 4;        /* the ride took a moment */
                    return;
                }
}

static void enter_or_leave(void)
{
    if (pl.car >= 0) {
        car_t *c = &cars[pl.car];
        if (c->vx * c->vx + c->vy * c->vy > 75 * 75)
            return;
        float rx = cosf(c->a), ry = sinf(c->a), half = (float)art_car[c->model].h * 0.5f + 7;
        float side[4][2] = { {-12, 0}, {12, 0}, {0, -half}, {0, half} };
        for (int k = 0; k < 4; k++) {
            float x = c->x + rx * side[k][0] + ry * side[k][1], y = c->y + ry * side[k][0] - rx * side[k][1];
            if (ped_ok(x, y)) {
                pl.x = x;
                pl.y = y;
                pl.a = c->a;
                c->ai = AI_NONE;
                pl.car = -1;
                snd_sfx(SFX_DOOR, 200);
                if ((G.owns & OWN_GARAGE) && in_yard(c->x, c->y) && c->keep != 1 && c->hp > 0) {
                    c->keep = 2;
                    note(L("PARKED IN YOUR GARAGE", "IN DEINER GARAGE GEPARKT"));
                    save();
                }
                return;
            }
        }
        return;
    }
    int best = -1;
    float bd = 1e12f;
    for (int i = 0; i < CARS; i++) {
        car_t *c = &cars[i];
        if (!c->used || c->hp <= 0 || c->vx * c->vx + c->vy * c->vy > 60 * 60)
            continue;
        float reach = 11 + (float)art_car[c->model].h * 0.3f, d = dist2(c->x, c->y, pl.x, pl.y);
        if (d < reach * reach && d < bd) {
            bd = d;
            best = i;
        }
    }
    if (best < 0)
        return;
    car_t *c = &cars[best];
    if (c->hail > 0) {                          /* the taxi that was whistled for */
        ride(c);
        return;
    }
    if (c->ai == AI_TRAFFIC || (c->ai == AI_POLICE && c->block != BLOCK_LEFT)) {
        /* somebody is in it: out they go */
        int p = ped_new(pl.x, pl.y, c->model == CAR_POLICE ? LOOK_COP : rnd_n(96));
        if (p >= 0)
            ped_flee(&peds[p], c->x, c->y, 260);
        offence(c->model == CAR_POLICE ? 90 : 30);
        snd_sfx(SFX_YELP, 200);
    }
    c->ai = AI_PLAYER;
    c->chase = c->seek = c->svc = c->block = c->tail = 0;
    c->hail = 0;
    if (c->keep == 2)
        c->keep = 0;
    pl.car = best;
    snd_sfx(SFX_DOOR, 255);
    G.radio = RADIO_AUTO;                       /* whatever this one is tuned to */
    if (game_station() == MUSIC_POLICE)
        note(L("POLICE RADIO", "POLIZEIFUNK"));
    else if (game_station() >= 0)
        note(music_name(game_station()));
}

/* B tapped on foot: the nearest taxi comes over */
static void whistle(void)
{
    int best = -1;
    float bd = 320 * 320;
    snd_sfx(SFX_WHISTLE, 255);
    snd_say(SAY_TAXI);
    for (int i = 0; i < CARS; i++) {
        const car_t *c = &cars[i];
        if (!c->used || c->model != CAR_TAXI || c->ai != AI_TRAFFIC || c->hp <= 0)
            continue;
        float d = dist2(c->x, c->y, pl.x, pl.y);
        if (d < bd) {
            bd = d;
            best = i;
        }
    }
    if (best < 0) {
        note(L("NO TAXI NEAR", "KEIN TAXI IN DER NÄHE"));
        return;
    }
    cars[best].hail = 700;
    cars[best].seek = 1;
    note("TAXI!");
}

/* B tapped in a car: people jump, cars ahead get going */
static void horn(void)
{
    const car_t *c = &cars[pl.car];
    float fx_ = sinf(c->a), fy_ = -cosf(c->a);
    snd_sfx(SFX_HORN, 255);
    for (int i = 0; i < PEDS; i++) {
        ped_t *p = &peds[i];
        if (!p->used || p->state != P_WALK)
            continue;
        float dx = p->x - c->x, dy = p->y - c->y, f = dx * fx_ + dy * fy_, l = -dx * fy_ + dy * fx_;
        if (f > 0 && f < 80 && fabsf(l) < 26) {
            p->state = P_FLEE;
            p->vx = -fy_ * (l > 0 ? 70.0f : -70.0f);
            p->vy = fx_ * (l > 0 ? 70.0f : -70.0f);
            p->timer = 26;
        }
    }
    for (int i = 0; i < CARS; i++) {
        car_t *o = &cars[i];
        if (!o->used || o->ai != AI_TRAFFIC)
            continue;
        float dx = o->x - c->x, dy = o->y - c->y, f = dx * fx_ + dy * fy_, l = -dx * fy_ + dy * fx_;
        if (f > 0 && f < 90 && fabsf(l) < 16)
            o->horned = 90;
    }
}

static void player_step(void)
{
    static int b_held;
    bool tap = false;
    if (HELD(BTN_B))
        b_held++;
    else {
        tap = b_held > 0 && b_held < 10;
        b_held = 0;
    }
    if (pl.inv > 0)
        pl.inv--;
    if (pl.cool > 0)
        pl.cool--;
    if (pl.car >= 0) {
        car_t *c = &cars[pl.car];
        /* A stick that is pressed in tips a little, to any side: while it is
         * pressed its directions do not count, or the car would set off just
         * when the player wants to get out. Pressed means stop. */
        bool press = HELD(BTN_C) != 0;
        bool gas = HELD(BTN_A) || (!press && HELD(BTN_UP)), brake = HELD(BTN_B) || (!press && HELD(BTN_DOWN));
        bool hb = (HELD(BTN_A) && HELD(BTN_B)) || press;
        float steer = press ? 0 : (HELD(BTN_RIGHT) ? 1.0f : 0) - (HELD(BTN_LEFT) ? 1.0f : 0);
        if (tap)
            horn();
        music_station(game_station());
        car_step(c, hb ? 0 : gas ? 1.0f : brake ? -1.0f : 0, steer, hb);
        pl.x = c->x;
        pl.y = c->y;
        float fx_ = sinf(c->a), fy_ = -cosf(c->a);
        float vf = c->vx * fx_ + c->vy * fy_, vl = -c->vx * fy_ + c->vy * fx_;
        int pitch = c->model == CAR_BIKE ? 70 : c->model == CAR_BUS || c->model == CAR_TRUCK ? 26 : 38;
        snd_engine(c->hp > 0 ? pitch + (int)(fabsf(vf) * 0.55f) : 0, c->hp > 0 ? 60 + (gas ? 50 : 0) : 0);
        snd_skid(fabsf(vl) > 40 ? 90 : 0);
        return;
    }
    snd_engine(0, 0);
    snd_skid(0);
    music_station(-1);
    if (tap)
        whistle();
    float dx = (HELD(BTN_RIGHT) ? 1.0f : 0) - (HELD(BTN_LEFT) ? 1.0f : 0);
    float dy = (HELD(BTN_DOWN) ? 1.0f : 0) - (HELD(BTN_UP) ? 1.0f : 0);
    float speed = HELD(BTN_B) ? 72 : 44;
    if (dx != 0 && dy != 0)
        speed *= 0.7071f;
    float vx = dx * speed + pl.kx, vy = dy * speed + pl.ky;
    pl.kx *= 0.88f;
    pl.ky *= 0.88f;
    if (dx != 0 || dy != 0) {
        pl.a = atan2f(dx, -dy);
        pl.anim += HELD(BTN_B) ? 2 : 1;
    }
    float nx = pl.x + vx * DT, ny = pl.y + vy * DT;
    if (ped_ok(nx - 3, pl.y) && ped_ok(nx + 3, pl.y))
        pl.x = nx;
    if (ped_ok(pl.x, ny - 3) && ped_ok(pl.x, ny + 3))
        pl.y = ny;
    /* A: fire if there is something to fire, else get into a car */
    if (pl.ammo > 0) {
        if (HELD(BTN_A) && !pl.cool) {
            shoot(pl.x, pl.y, pl.a, true);
            pl.ammo--;
            pl.cool = 14;
            panic(pl.x, pl.y, 130);
            offence(10);
        }
    } else if (PRESSED(BTN_A))
        enter_or_leave();
}

/* something to buy: stand on its spot with the money */
static bool buy(spot_t at, int price, const char *what)
{
    if (dist2(pl.x, pl.y, (float)at.x, (float)at.y) > 12 * 12 || G.shop_wait)
        return false;
    G.shop_wait = 150;
    char s[48];
    if (G.cash < price) {
        snprintf(s, sizeof s, "%s: $%d", what, price);
        note(s);
        return false;
    }
    pay(-price);
    snd_sfx(SFX_BUY, 255);
    return true;
}

static void world_things(void)
{
    /* tokens */
    for (int i = 0; i < TOKENS; i++)
        if (!(G.tokens & (1u << i)) && dist2(px(), py(), (float)w_token[i].x, (float)w_token[i].y) < 12 * 12) {
            G.tokens |= 1u << i;
            snd_sfx(SFX_COIN, 255);
            int n = 0;
            for (int k = 0; k < TOKENS; k++)
                n += (G.tokens >> k) & 1;
            char s[48];
            if (n == TOKENS)
                snprintf(s, sizeof s, L("ALL CHIPS! +$1050", "ALLE CHIPS! +$1050"));
            else
                snprintf(s, sizeof s, L("CHIP %d OF %d +$50", "CHIP %d VON %d +$50"), n, TOKENS);
            message(s, RGB(255, 220, 64), 120);
            pay(n == TOKENS ? 1050 : 50);
            save();
        }
    /* the paint shops: a new colour, the dents out, and the police forget */
    if (G.paint_wait > 0)
        G.paint_wait--;
    if (pl.car >= 0 && !G.paint_wait && !(G.job == J_HOTCAR && G.stage == 1)) {
        car_t *c = &cars[pl.car];
        for (int k = 0; k < PAINTS; k++) {
            if (dist2(c->x, c->y, (float)w_paint[k].x, (float)w_paint[k].y) > 14 * 14 ||
                c->vx * c->vx + c->vy * c->vy > 40 * 40)
                continue;
            G.paint_wait = 200;
            if (G.stars == 0 && c->hp >= 100 && !(c->dmg[0] | c->dmg[1] | c->dmg[2] | c->dmg[3]))
                break;
            if (G.cash < 50) {
                note(L("A PAINT JOB IS $50", "LACKIEREN KOSTET $50"));
                break;
            }
            bool watched = false;
            for (int i = 0; i < CARS; i++)
                if (cars[i].used && cars[i].chase && dist2(cars[i].x, cars[i].y, c->x, c->y) < 80 * 80)
                    watched = true;
            if (watched) {
                note(L("THEY CAN SEE YOU", "SIE SEHEN DICH"));
                break;
            }
            pay(-50);
            c->hp = 100;
            memset(c->dmg, 0, sizeof c->dmg);
            if (c->model != CAR_TAXI && c->model != CAR_POLICE && c->model != CAR_AMBULANCE && c->model != CAR_TOW)
                c->colour = (uint8_t)((c->colour + 1 + rnd_n(CAR_COLOURS - 2)) % (CAR_COLOURS - 1));
            if (G.job != J_HEAT && G.job != J_GETAWAY) {
                G.heat = 0;
                G.stars = 0;
            }
            message(L("FRESH PAINT -$50", "FRISCH LACKIERT -$50"), RGB(120, 255, 120), 120);
            snd_sfx(SFX_JOB, 255);
            save();
        }
    }
    /* what money buys, on foot */
    if (G.shop_wait > 0)
        G.shop_wait--;
    if (pl.car < 0) {
        if (!(G.owns & OWN_GARAGE) && buy(w_garage, PRICE_GARAGE, L("THE GARAGE", "DIE GARAGE"))) {
            G.owns |= OWN_GARAGE;
            message(L("THE GARAGE IS YOURS", "DIE GARAGE GEHÖRT DIR"), RGB(120, 255, 120), 170);
            note(L("CARS LEFT IN ITS YARD STAY", "AUTOS IM HOF BLEIBEN DORT"));
            save();
        }
        if (!(G.owns & OWN_TOWER) && buy(w_tower, PRICE_TOWER, L("THE TOWER", "DER TOWER"))) {
            G.owns |= OWN_TOWER;
            message(L("RUSHTOWN IS YOURS", "RUSHTOWN GEHÖRT DIR"), RGB(255, 220, 64), 400);
            snd_sfx(SFX_DONE, 255);
            save();
        }
        if (buy(w_ammo, PRICE_AMMO, L("24 ROUNDS", "24 SCHUSS"))) {
            pl.ammo += 24;
            if (pl.ammo > 240)
                pl.ammo = 240;
            note(L("24 ROUNDS -$100", "24 SCHUSS -$100"));
            save();
        }
    }
#if RT_ISLANDS
    /* the ferry: on the end of either pier, on foot or with the car */
    if (G.ferry_wait > 0)
        G.ferry_wait--;
    for (int k = 0; k < 2 && !G.ferry_wait && !G.ev; k++) {
        if (dist2(px(), py(), (float)w_ferry[k].x, (float)w_ferry[k].y) > 15 * 15 || pspeed() > 40)
            continue;
        G.ferry_wait = 150;
        if (!(G.owns & OWN_FERRY))
            note(L("THE FERRY IS OUT OF ORDER", "DIE FÄHRE IST AUSSER BETRIEB"));
        else if (G.cash < PRICE_FERRY)
            note(L("THE FERRY IS $10", "DIE FÄHRE KOSTET $10"));
        else {
            pay(-PRICE_FERRY);
            G.ev = EV_FERRY;
            G.ev_t = 90;
            G.ride_x = (float)w_ferry[1 - k].x;
            G.ride_y = (float)w_ferry[1 - k].y + (k ? -44.0f : 44.0f);  /* ashore, off the pier's end */
            snd_sfx(SFX_SPLASH, 160);
        }
    }
#endif
    /* hearts come back while nobody is after you */
    if (pl.hp < 3 && !G.stars && (G.frame % 900) == 0)
        pl.hp++;
    /* through a red light with the police looking on */
    static int in_node = -1;
    if (pl.car >= 0) {
        const car_t *c = &cars[pl.car];
        int n = world_node_near(c->x, c->y);
        bool in = dist2(c->x, c->y, (float)nodes[n].x, (float)nodes[n].y) < 13 * 13;
        if (in && in_node != n) {
            int dir = fabsf(c->vx) > fabsf(c->vy) ? (c->vx > 0 ? D_E : D_W) : (c->vy > 0 ? D_S : D_N);
            if (game_light(n, dir) == 2 && pspeed() > 50)
                for (int i = 0; i < CARS; i++)
                    if (cars[i].used && cars[i].ai == AI_POLICE && cars[i].block != BLOCK_LEFT &&
                        dist2(cars[i].x, cars[i].y, c->x, c->y) < 170 * 170 &&
                        world_sight(cars[i].x, cars[i].y, c->x, c->y)) {
                        offence(32);
                        note(L("RAN A RED LIGHT", "BEI ROT GEFAHREN"));
                        break;
                    }
        }
        in_node = in ? n : -1;
    } else
        in_node = -1;
    /* the name of the part of town, when it changes */
    const char *d = world_district(px(), py());
    if (d != G.district) {
        G.district = d;
        if (!G.note_t)
            note(d);
    }
}

static void wanted_step(void)
{
    if (G.heat <= 0 || G.ev) {                  /* nobody is after the player, or it is over: quiet */
        snd_siren(false);
        return;
    }
    bool seen = false, close = false;
    for (int i = 0; i < CARS; i++) {
        const car_t *c = &cars[i];
        if (!c->used || c->ai != AI_POLICE)
            continue;
        float d2 = dist2(c->x, c->y, px(), py());
        if (d2 < 190 * 190)
            seen = true;
        if (c->chase && d2 < 26 * 26)
            close = true;
    }
    for (int i = 0; i < PEDS; i++)
        if (peds[i].used && peds[i].state == P_COP && dist2(peds[i].x, peds[i].y, px(), py()) < 150 * 150)
            seen = true;
    snd_siren(seen && G.stars > 0);
    if (close && pl.car >= 0 && !G.say_wait) {  /* from the car behind */
        snd_say(SAY_PULL);
        G.say_wait = 300;
    }
    if (seen)
        G.unseen = 0;
    else if (++G.unseen > UNSEEN)           /* out of sight for a while: they give up bit by bit */
        G.heat -= 9 * DT * (G.stars > 2 ? 1.6f : 1.0f);
    if (G.heat < 30 && !seen)
        G.heat -= 12 * DT;
    int before = G.stars;
    G.stars = G.heat >= 400 ? 4 : G.heat >= 220 ? 3 : G.heat >= 100 ? 2 : G.heat >= 30 ? 1 : 0;
    if (G.heat <= 0) {
        G.heat = 0;
        G.stars = 0;
    }
    if (before && !G.stars) {
        message(L("YOU LOST THEM", "ABGEHÄNGT"), RGB(120, 255, 120), 110);
        snd_siren(false);
    }
    /* boxed in: a police car right next to a player who sits in a standing car */
    static int held;
    if (G.stars && close && pl.car >= 0 && pspeed() < 16 && !G.ev) {
        if (++held > 60) {
            held = 0;
            bust();
        }
    } else
        held = 0;
}

/* ---- the frame ------------------------------------------------------------- */

static void camera(bool snap)
{
    float tx = px() - SCR_W / 2, ty = py() - SCR_H / 2;
    if (pl.car >= 0) {                          /* look where it is going */
        tx += cars[pl.car].vx * 0.32f;
        ty += cars[pl.car].vy * 0.32f;
    }
    if (snap) {
        G.camfx = tx;
        G.camfy = ty;
    } else {
        G.camfx += (tx - G.camfx) * 0.12f;
        G.camfy += (ty - G.camfy) * 0.12f;
    }
    G.camfx = clampf(G.camfx, 0, WORLD_W - SCR_W);
    G.camfy = clampf(G.camfy, 0, WORLD_H - SCR_H);
    G.cam_x = (int)G.camfx;
    G.cam_y = (int)G.camfy;
}

static float cruise(int model)
{
    return model == CAR_BUS ? 46 : model == CAR_TRUCK || model == CAR_TOW ? 52 : model == CAR_BIKE ? 78 : 62;
}

static void world_step(void)
{
    for (int i = 0; i < CARS; i++) {
        car_t *c = &cars[i];
        if (!c->used)
            continue;
        if (c->ai == AI_TRAFFIC) {
            if (c->svc)
                service_step(c);
            else if (c->hail > 0) {             /* whistled for: to the player, and wait there */
                c->tx = pl.x;
                c->ty = pl.y;
                seek_step(c, 90, true);
                if (--c->hail <= 0 || pl.car >= 0 || G.state != ST_PLAY) {
                    c->hail = 0;
                    c->seek = 0;
                    regrid(c);
                }
            } else
                follow_roads(c, cruise(c->model), true);
        } else if (c->ai == AI_POLICE)
            police_step(c);
        else if (c->ai == AI_NONE)
            car_step(c, 0, 0, true);
        if (tile_px(c->x, c->y) == T_WATER) {   /* into the water: gone */
            if (on_screen(c->x, c->y, 20)) {
                burst(FX_SPLASH, c->x, c->y, 16, 60);
                snd_sfx(SFX_SPLASH, 255);
            }
            if (c->ai == AI_PLAYER) {
                pl.car = -1;
                G.ev = EV_WET;
                G.ev_t = 120;
            }
            car_remove(i);
        }
    }
    cars_collide();
    peds_step();
    cars_vs_people();
    shots_step();
    for (int i = 0; i < FXS; i++) {
        fx_t *f = &fxs[i];
        if (!f->used)
            continue;
        f->x += f->vx * DT;
        f->y += f->vy * DT;
        if (--f->life <= 0)
            f->used = 0;
    }
    spawner();
}

static void start_game(void)
{
    memset(cars, 0, sizeof cars);
    memset(peds, 0, sizeof peds);
    memset(fxs, 0, sizeof fxs);
    memset(shots, 0, sizeof shots);
    int ammo = pl.ammo;
    memset(&pl, 0, sizeof pl);
    pl.ammo = ammo;
    pl.car = -1;
    pl.hp = 3;
    pl.x = (float)w_start.x;
    pl.y = (float)w_start.y;
    G.state = ST_PLAY;
    G.job = J_NONE;
    G.job_car = -1;
    G.heat = 0;
    G.stars = 0;
    G.ev = EV_NONE;
    targets_clear();
    G.timed = false;
    G.job_text[0] = 0;
    G.district = NULL;
    G.note_t = 0;
    camera(true);
    /* something to drive right away */
    /* on the footway a few steps down the street, out of the traffic's way */
    float cx = pl.x, cy = pl.y + 30;
    if (tile_px(cx, cy + 10) < T_SW) {
        cx = pl.x + 30;
        cy = pl.y;
    }
    car_new(CAR_COMPACT, 1, cx, cy, tile_px(pl.x, pl.y + 40) < T_SW ? PI / 2 : 0, AI_NONE);
    /* and what was left in the garage */
    for (int k = 0; k < 4; k++)
        if (G.yard[k][0] && (G.owns & OWN_GARAGE)) {
            int tx = w_yard[0] + 1 + 2 * k;
            if (tx > w_yard[2])
                break;
            int c = car_new(G.yard[k][0] - 1, G.yard[k][1], (float)(tx * TILE + 8), (float)(w_yard[1] * TILE + 20), 0, AI_NONE);
            if (c >= 0)
                cars[c].keep = 2;
        }
    world_open_bridge((G.owns & OWN_BRIDGE) != 0);
    message(L("WELCOME TO RUSHTOWN", "WILLKOMMEN IN RUSHTOWN"), RGB(255, 220, 64), 170);
    G.card = -1;
    G.card_seen = 0;
}

/* the saved game of a mode becomes the one being played */
static void load_mode(int mode)
{
    const save_t *s = &G.slot[mode];
    G.mode = mode;
    G.cash = G.earned = G.jobs = G.bumped = G.owns = G.story = G.fails = 0;
    G.lives = LIVES;
    G.tokens = 0;
    pl.ammo = 0;
    memset(G.yard, 0, sizeof G.yard);
    if (s->magic == SAVE_MAGIC) {
        G.cash = s->cash;
        G.earned = s->earned;
        G.tokens = s->tokens;
        G.jobs = s->jobs;
        G.bumped = s->bumped;
        G.owns = s->owns;
        G.story = s->story;
        G.fails = s->fails <= 3 ? s->fails : 0;
        G.lives = s->lives >= 1 && s->lives <= LIVES ? s->lives : LIVES;
        pl.ammo = s->ammo;
        memcpy(G.yard, s->yard, sizeof G.yard);
    }
    if (mode == MODE_FREE || !RT_STORY)         /* nothing is shut in the free game */
        G.owns |= OWN_BRIDGE | OWN_FERRY;
}

void game_init(uint32_t seed)
{
    rs = seed | 1;
    art_init();
    world_init();
    memset(&G, 0, sizeof G);
    memset(&pl, 0, sizeof pl);
    G.sound = true;
    G.radio = RADIO_AUTO;
    for (int m = 0; m < MODES; m++)
        if (!plat_load(&G.slot[m], m))
            G.slot[m].magic = 0;
        else {
            G.sound = G.slot[m].sound != 0;
            rt_lang = G.slot[m].lang ? 1 : 0;
        }
    snd_mute(!G.sound);
    G.menu = RT_STORY ? MODE_STORY : MODE_FREE;
    load_mode(MODE_FREE);                       /* the title shows the whole town */
    start_game();
    G.state = ST_TITLE;
    G.msg_t = 0;
}

static void title_step(void)
{
    music_station(0);
    /* the town goes about its day behind the title */
    float t = (float)G.frame * 0.004f;
    pl.x = (float)w_start.x + sinf(t) * 150;
    pl.y = (float)w_start.y + cosf(t * 0.7f) * 110;
    pl.car = -1;
    world_step();
    G.camfx = clampf(pl.x - SCR_W / 2, 0, WORLD_W - SCR_W);
    G.camfy = clampf(pl.y - SCR_H / 2, 0, WORLD_H - SCR_H);
    G.cam_x = (int)G.camfx;
    G.cam_y = (int)G.camfy;
    if (PRESSED(BTN_LEFT | BTN_RIGHT)) {        /* english or deutsch */
        rt_lang = !rt_lang;
        snd_sfx(SFX_UI, 255);
    }
#if RT_STORY
    if (PRESSED(BTN_UP | BTN_DOWN)) {           /* the story or the free game */
        G.menu = !G.menu;
        snd_sfx(SFX_UI, 255);
    }
#endif
    if (PRESSED(BTN_A | BTN_B | BTN_C)) {
        rs ^= (uint32_t)G.frame * 2654435761u;  /* the wait before the press is the dice */
        if (!rs)
            rs = 1;
        snd_sfx(SFX_JOB, 255);
        load_mode(G.menu);
        music_station(-1);
        start_game();
#if RT_STORY
        story_opening();
#endif
    }
}

/* No life left. The money and the gun are gone; the story goes back to
 * the beginning of its chapter, and there are five lives again. What was
 * bought, what stands in the garage and the rank stay. */
static void over_step(void)
{
    if (!PRESSED(BTN_A | BTN_B))
        return;
    G.cash = 0;
    pl.ammo = 0;
    G.fails = 0;
    G.lives = LIVES;
    if (G.mode == MODE_STORY)
        G.story = story_chapter(G.story);
    save();
    G.menu = G.mode;
    G.state = ST_TITLE;
}

static void pause_step(void)
{
    int rows = 3 + (music_stations() ? 1 : 0), quit = rows - 1;
    if (PRESSED(BTN_UP)) { G.menu = (G.menu + rows - 1) % rows; snd_sfx(SFX_UI, 255); }
    if (PRESSED(BTN_DOWN)) { G.menu = (G.menu + 1) % rows; snd_sfx(SFX_UI, 255); }
    if (PRESSED(BTN_B | BTN_C)) {
        G.state = ST_PLAY;
        return;
    }
    if (!PRESSED(BTN_A))
        return;
    if (G.menu == 0)
        G.state = ST_PLAY;
    else if (G.menu == 1) {
        G.sound = !G.sound;
        snd_mute(!G.sound);
        snd_sfx(SFX_UI, 255);
        save();
    } else if (G.menu != quit) {                /* the radio: the car's own, each station, off */
        G.radio = G.radio == RADIO_OFF ? RADIO_AUTO : G.radio == RADIO_AUTO ? 0
                : G.radio + 1 >= music_stations() ? RADIO_OFF : G.radio + 1;
        music_station(game_station());
        snd_sfx(SFX_UI, 255);
    } else {
        save();
        music_station(-1);
        G.menu = G.mode;
        G.state = ST_TITLE;
        snd_engine(0, 0);
        snd_skid(0);
        snd_siren(false);
    }
}

void game_tick(uint32_t buttons)
{
    static int c_held;
    btn_prev = btn;
    btn = buttons;
    G.frame++;
    if (G.msg_t > 0) G.msg_t--;
    if (G.note_t > 0) G.note_t--;
    if (G.say_wait > 0) G.say_wait--;


    if (G.state == ST_TITLE) {
        title_step();
        return;
    }
    if (G.state == ST_PAUSE) {
        pause_step();
        return;
    }
#if RT_STORY
    if (G.state == ST_TALK) {
        talk_step();
        return;
    }
#endif
    if (G.state == ST_OVER) {
        over_step();
        return;
    }
    /* CENTER: a tap gets in and out of cars, holding it opens the menu */
    if (HELD(BTN_C)) {
        if (++c_held == 28) {
            G.state = ST_PAUSE;
            G.menu = 0;
            snd_engine(0, 0);
            snd_skid(0);
            snd_siren(false);
            snd_sfx(SFX_UI, 255);
            return;
        }
    } else {
        /* not twice within half a second: a switch that bounces would get
         * the player out of the car and straight back in */
        static int door_wait;
        if (door_wait > 0)
            door_wait--;
        if (c_held > 0 && c_held < 28 && !G.ev && !door_wait) {
            enter_or_leave();
            door_wait = 25;
        }
        c_held = 0;
    }
    if (G.ev) {                                 /* knocked out, caught, in the water, in a taxi */
        snd_engine(0, 0);
        snd_skid(0);
        snd_siren(false);
        music_station(-1);
        if (--G.ev_t <= 0) {
            if (G.ev == EV_RIDE)
                ride_end();
            else if (G.ev == EV_FERRY) {        /* across, car and all */
                if (pl.car >= 0) {
                    car_t *c = &cars[pl.car];
                    c->x = G.ride_x;
                    c->y = G.ride_y;
                    c->vx = c->vy = 0;
                    c->a = G.ride_y > 1600 ? PI : 0;
                }
                pl.x = G.ride_x;
                pl.y = G.ride_y;
                G.ferry_wait = 250;
                G.camfx = pl.x - SCR_W / 2;
                G.camfy = pl.y - SCR_H / 2;
            } else {
                bool busted = G.ev == EV_BUSTED, story = G.job == J_STORY;
                /* Knocked out or drowned costs a tenth of the money, up to
                 * $300, and a life. An arrest costs no life: a quarter as
                 * bail, up to $1000, the gun and the car. */
                int fine = G.cash / (busted ? 4 : 10), most = busted ? 1000 : 300;
                if (fine > most)
                    fine = most;
                pay(-fine);
                if (busted) {
                    pl.ammo = 0;
                    if (pl.car >= 0) {          /* impounded */
                        int c = pl.car;
                        pl.car = -1;
                        car_remove(c);
                    }
                } else
                    G.lives--;
                respawn(busted ? w_police : w_hospital);
                job_end(false, NULL);
                char t[48];
                if (busted) {
                    /* The job is gone. The story cannot lose a mission
                     * without losing its thread, so there the pay shrinks
                     * by a quarter with every arrest. */
                    if (story && G.fails < 3)
                        G.fails++;
                    snprintf(t, sizeof t, L("BAIL $%d. THE CAR STAYS.", "KAUTION $%d. WAGEN WEG."), fine);
                    note(t);
                } else if (G.lives > 0) {
                    snprintf(t, sizeof t, G.lives == 1 ? L("LAST LIFE", "LETZTES LEBEN") : L("%d LIVES LEFT", "NOCH %d LEBEN"), G.lives);
                    note(t);
                } else {                        /* the end: see over_step() */
                    G.state = ST_OVER;
                    snd_sfx(SFX_FAIL, 255);
                }
                G.note_t = 250;
                save();
            }
            G.ev = EV_NONE;
        }
        world_step();
        camera(G.ev == EV_NONE);
        return;
    }
    player_step();
    world_step();
    wanted_step();
    jobs_step();
    world_things();
    camera(false);
}

void game_draw(void)
{
    draw_all();
}
