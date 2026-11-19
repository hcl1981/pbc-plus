/* host.c -- Rushtown on a PC, without a window: for tests and pictures.
 *
 *   host bigmap town.ppm            the whole town in one picture
 *   host [steps ...]                play: +A -A press and release a button
 *                                   (U D L R A B C), w50 wait 50 ticks,
 *                                   sNAME.ppm save the frame, p print the state;
 *                                   for tests: jN stand on phone N, g be at the
 *                                   job's target, c sit in a car, x count the cars
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../src/game.h"

static px_t frame[SCR_W * SCR_H];

#if RT_STORY
void story_debug_solve(void);
#else
static void story_debug_solve(void) {}
#endif

bool plat_load(save_t *s, int slot) { (void)s; (void)slot; return false; }
bool plat_save(const save_t *s, int slot) { (void)s; (void)slot; return true; }

static void rgb(px_t p, unsigned char *o)
{
    o[0] = (unsigned char)((p & 31) << 3);
    o[1] = (unsigned char)(((p >> 5) & 63) << 2);
    o[2] = (unsigned char)(((p >> 11) & 31) << 3);
}

static void shot(const char *name)
{
    FILE *f = fopen(name, "wb");
    if (!f) { perror(name); exit(1); }
    fprintf(f, "P6 %d %d 255\n", SCR_W, SCR_H);
    for (int i = 0; i < SCR_W * SCR_H; i++) {
        unsigned char o[3];
        rgb(frame[i], o);
        fwrite(o, 1, 3, f);
    }
    fclose(f);
}

static void bigmap(const char *name)
{
    static unsigned char big[WORLD_H][WORLD_W][3];
    G.state = ST_PLAY;
    pl.car = -1;
    pl.x = pl.y = -1000;
    for (int cy = 0; cy < WORLD_H; cy += SCR_H)
        for (int cx = 0; cx < WORLD_W; cx += SCR_W) {
            G.cam_x = cx;
            G.cam_y = cy;
            memset(cars, 0, sizeof cars);
            memset(peds, 0, sizeof peds);
            ground_only = 1;
            draw_all();
            for (int y = 0; y < SCR_H && cy + y < WORLD_H; y++)
                for (int x = 0; x < SCR_W && cx + x < WORLD_W; x++)
                    rgb(frame[y * SCR_W + x], big[cy + y][cx + x]);
        }
    FILE *f = fopen(name, "wb");
    fprintf(f, "P6 %d %d 255\n", WORLD_W, WORLD_H);
    fwrite(big, 1, sizeof big, f);
    fclose(f);
}

int main(int argc, char **argv)
{
    uint32_t buttons = 0;
    static uint8_t audio[SND_RATE / TICK_HZ];
    g_fb = frame;
    game_init(12345);
    if (argc >= 3 && !strcmp(argv[1], "bigmap")) {
        bigmap(argv[2]);
        return 0;
    }
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        static const char names[] = "UDLRABC";
        if (a[0] == '+' || a[0] == '-') {
            const char *p = strchr(names, a[1]);
            if (!p) { fprintf(stderr, "button %s?\n", a); return 1; }
            if (a[0] == '+') buttons |= 1u << (p - names); else buttons &= ~(1u << (p - names));
        } else if (a[0] == 'w') {
            for (int n = atoi(a + 1); n > 0; n--) {
                game_tick(buttons);
                snd_render(audio, sizeof audio);
            }
        } else if (a[0] == 's') {
            game_draw();
            shot(a + 1);
        } else if (a[0] == 'g') {               /* to the job's target, standing */
            float tx, ty;
            if (game_target(&tx, &ty)) {
                if (pl.car >= 0) {
                    cars[pl.car].x = tx; cars[pl.car].y = ty;
                    cars[pl.car].vx = cars[pl.car].vy = 0;
                }
                pl.x = tx; pl.y = ty;
            }
        } else if (a[0] == 'c') {               /* into some car, right here */
            for (int k = 0; k < CARS && pl.car < 0; k++)
                if (cars[k].used && cars[k].ai != AI_POLICE && k != G.job_car) {
                    cars[k].ai = AI_PLAYER;
                    cars[k].x = pl.x; cars[k].y = pl.y;
                    cars[k].vx = cars[k].vy = 0;
                    pl.car = k;
                }
        } else if (a[0] == 'd') {               /* dFRLR: dent every car, each digit 0..3 */
            for (int k = 0; k < CARS; k++)
                for (int z = 0; z < 4 && a[1 + z]; z++)
                    cars[k].dmg[z] = (uint8_t)((a[1 + z] - '0') * 30);
        } else if (a[0] == 'S') {               /* the story is at mission N (from 0) */
            G.story = atoi(a + 1);
            if (G.story >= 10) G.owns |= OWN_FERRY;
            if (G.story >= 14) G.owns |= OWN_BRIDGE;
            world_open_bridge((G.owns & OWN_BRIDGE) != 0);
        } else if (a[0] == 'Q') {               /* play the next story mission through, by magic */
            int was = G.story, n = 0;
            for (; n < 9000 && G.story == was; n++) {
                giver_t gv;
                uint32_t b = 0;
                if (G.state == ST_TALK)
                    b = n & 1 ? BTN_A : 0;
                else if (G.job == J_STORY)
                    story_debug_solve();
                else if (G.job == J_NONE && story_giver(&gv)) {
                    if (pl.car >= 0) { cars[pl.car].ai = AI_NONE; pl.car = -1; }
                    pl.x = gv.x; pl.y = gv.y;
                }
                game_tick(b);
            }
            printf("mission %2d: %s after %d ticks, cash %d, owns %d, msg '%s'\n", was + 1,
                   G.story == was ? "NOT DONE" : "done", n, G.cash, G.owns, G.msg_t ? G.msg : "");
        } else if (a[0] == 'H') {               /* one heart left, and a car comes */
            pl.hp = 0; G.ev = EV_KO; G.ev_t = 130;
        } else if (a[0] == 'U') {               /* wait until caught, knocked out or the like */
            for (int n = 0; n < 9000 && !G.ev; n++)
                game_tick(buttons);
        } else if (a[0] == 'V') {               /* wait until the job or mission is over */
            for (int n = 0; n < 9000 && G.job != J_NONE; n++)
                game_tick(buttons);
        } else if (a[0] == 'G') {               /* stand where the story's next giver is */
            giver_t gv;
            if (story_giver(&gv)) { pl.x = gv.x; pl.y = gv.y; pl.car = -1; }
        } else if (a[0] == 'K') {               /* the mission's car is a wreck */
            if (G.job_car >= 0) cars[G.job_car].hp = 0;
        } else if (a[0] == 'M') {               /* MODE: play the free game (1) or the story (0) */
            G.menu = atoi(a + 1);
        } else if (a[0] == 'D') {               /* deutsch */
            rt_lang = 1;
        } else if (a[0] == 'J') {               /* start job type N */
            game_debug_job(atoi(a + 1));
        } else if (a[0] == '$') {               /* money, earned */
            G.cash += atoi(a + 1); G.earned += atoi(a + 1);
        } else if (a[0] == 'm') {               /* 24 rounds */
            pl.ammo += 24;
        } else if (a[0] == 'T') {               /* stand at X,Y (pixels) */
            sscanf(a + 1, "%f,%f", &pl.x, &pl.y);
        } else if (a[0] == 'W') {               /* where things are */
            printf("ferry %d,%d ", w_ferry[0].x, w_ferry[0].y);
            printf("garage %d,%d ammo %d,%d tower %d,%d hospital %d,%d police %d,%d paint %d,%d %d,%d nodes %d buildings %d\n",
                   w_garage.x, w_garage.y, w_ammo.x, w_ammo.y, w_tower.x, w_tower.y, w_hospital.x, w_hospital.y,
                   w_police.x, w_police.y, w_paint[0].x, w_paint[0].y, w_paint[1].x, w_paint[1].y, nnodes, nbuildings);
        } else if (a[0] == 'j') {               /* to phone N */
            int n = atoi(a + 1);
            pl.x = w_phone[n].x; pl.y = w_phone[n].y;
        } else if (a[0] == 'x') {               /* the cars */
            int blocked = 0, wrecked = 0, moving = 0, n = 0;
            for (int k = 0; k < CARS; k++) {
                const car_t *c = &cars[k];
                if (!c->used) continue;
                n++;
                float v = c->vx * c->vx + c->vy * c->vy;
                if (c->x != c->x || c->y != c->y) { printf("NaN in car %d\n", k); return 2; }
                blocked += c->blocked > 100; wrecked += c->hp <= 0; moving += v > 30 * 30;
                if (a[1] == 'x')
                    printf("  car %2d ai %d model %d at %4.0f,%4.0f v %3.0f hp %3d blocked %d to %d dir %d\n",
                           k, c->ai, c->model, c->x, c->y, sqrtf(v), c->hp, c->blocked, c->to, c->dir);
            }
            printf("frame %d: %d cars, %d moving, %d blocked, %d wrecked\n", G.frame, n, moving, blocked, wrecked);
        } else if (a[0] == 'p') {
            int nc = 0, np = 0;
            for (int k = 0; k < CARS; k++) nc += cars[k].used;
            for (int k = 0; k < PEDS; k++) np += peds[k].used;
            printf("frame %d state %d pos %.0f,%.0f car %d hp %d cash %d jobs %d stars %d heat %.0f job %d/%d cars %d peds %d ammo %d owns %d rank %d story %d step %d siren %d radio %d lives %d msg '%s' text '%s' note '%s'\n",
                   G.frame, G.state, pl.x, pl.y, pl.car, pl.hp, G.cash, G.jobs, G.stars, G.heat, G.job, G.stage, nc, np, pl.ammo, G.owns, game_rank(), G.story, G.step, snd_siren_on(), game_station(), G.lives,
                   G.msg_t ? G.msg : "", G.job_text, G.note_t ? G.note : "");
        }
    }
    return 0;
}
