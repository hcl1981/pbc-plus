/* game.h -- what game.c (rules) and draw.c (pictures) share. */
#ifndef GAME_H
#define GAME_H

#include "rt.h"

#define PI 3.14159265f

enum { AI_NONE, AI_PLAYER, AI_TRAFFIC, AI_POLICE };
/* Where a car has been hit. A third of DMG_MAX is a dent, two thirds a
 * crumpled end with broken lights, all of it a wreck of that side. */
enum { DMG_FRONT, DMG_REAR, DMG_LEFT, DMG_RIGHT };
#define DMG_MAX 90
#define DMG_LEVEL(d) ((d) / 30)

enum { SVC_NONE, SVC_GO, SVC_WORK };    /* ambulance and tow truck: called, at work */
enum { BLOCK_NONE, BLOCK_WAIT, BLOCK_LEFT };    /* police: a road block; its officer has got out */
typedef struct {
    uint8_t used, model, colour, ai;
    float x, y, a, vx, vy;
    int16_t hp;                         /* 100 .. 0: wrecked */
    uint8_t dmg[4];                     /* dents at DMG_FRONT .. DMG_RIGHT, 0 .. DMG_MAX */
    uint8_t from, to, dir;              /* on the road graph */
    uint8_t keep;                       /* 1: a job's car, 2: in the garage; not taken away */
    uint8_t chase;                      /* police: after the player */
    uint8_t direct;                     /* steering straight at tx, ty */
    uint8_t seek;                       /* heading for tx, ty instead of wandering */
    uint8_t svc, svc_i, block, tail;
    float tx, ty;
    int16_t blocked, stuck, reverse, horned, svc_t, hail;
} car_t;
#define CARS 26
extern car_t cars[CARS];

enum { P_WALK, P_FLEE, P_DOWN, P_WAIT, P_COP, P_DEAD };
typedef struct {
    uint8_t used, state, look, dir;
    float x, y, a, vx, vy;
    int16_t timer, turn, cool;
    uint8_t anim;
} ped_t;
#define PEDS 28
#define LOOK_PLAYER 255
#define LOOK_COP    254
extern ped_t peds[PEDS];

enum { FX_SMOKE, FX_SPARK, FX_DUST, FX_SPLASH };
typedef struct { uint8_t used, kind; float x, y, vx, vy; int16_t life; } fx_t;
#define FXS 48
extern fx_t fxs[FXS];

typedef struct { uint8_t used, mine; float x, y, vx, vy; int16_t life; } shot_t;
#define SHOTS 12
extern shot_t shots[SHOTS];

typedef struct {
    float x, y, a, kx, ky;              /* kx, ky: being knocked away */
    int car;                            /* -1: on foot */
    int hp, inv, anim, ammo, cool;
} player_t;
extern player_t pl;

enum { ST_TITLE, ST_PLAY, ST_PAUSE, ST_TALK, ST_OVER };
enum { EV_NONE, EV_KO, EV_BUSTED, EV_WET, EV_RIDE, EV_FERRY };
enum { J_NONE, J_COURIER, J_TAXI, J_RACE, J_MULTI, J_HOTCAR, J_FRAGILE, J_HEAT, J_GETAWAY, J_TAIL, J_STORY, J_TYPES };
#define RADIO_AUTO -2
#define RADIO_OFF  -1
int game_station(void);                 /* what plays in the player's car now, -1 nothing */
#define UNSEEN 400                      /* ticks out of the police's sight before the stars fade */
#define TARGETS 4
#define RANKS 5
extern const char *const rank_name[RANKS];

typedef struct {
    int state, frame;
    int cam_x, cam_y;
    float camfx, camfy;
    int cash, earned, jobs, bumped, owns;
    uint32_t tokens;
    bool sound;
    float heat;
    int stars, unseen;
    int ev, ev_t;
    float ride_x, ride_y;
    /* the job */
    int job, stage, job_car, left;      /* left: checkpoints to go */
    struct { float x, y; bool on; } tgt[TARGETS];
    float job_time;
    bool timed, time_wins;              /* time_wins: holding out until the clock is at 0 */
    int reward;
    float spot;                         /* tailing: how much they have noticed, 0 .. 100 */
    int lost;
    char job_text[48];
    int phone_wait, shop_wait, paint_wait;
    /* messages */
    char msg[48];
    int msg_t;
    px_t msg_col;
    char note[64];                      /* the small line at the bottom */
    int note_t;
    const char *district;
    int menu;
    uint8_t yard[4][2];                 /* the garage's cars as saved */
    int mode;                           /* MODE_STORY or MODE_FREE */
    int radio;                          /* RADIO_AUTO: what the car is tuned to; a station; RADIO_OFF */
    int lives;                          /* knocked out or drowned costs one; none left is the end */
    int say_wait;
    int ferry_wait;
    /* the story (story.c) */
    int story, step, sub, talk, story_wait;
    int fails;                          /* arrests during the current mission: each takes a quarter off its pay */
    int card, card_seen;                /* a picture before the briefing: PIC_*, -1 none; for which mission it was shown */
    float hold;
    save_t slot[MODES];                 /* what is saved, for the title */
} game_t;
extern game_t G;

int  game_rank(void);
int  game_light(int node, int dir);     /* 0 green, 1 yellow, 2 red, for traffic going `dir` */
bool game_lit(int node);                /* does this crossing have lights */
bool game_target(float *x, float *y);   /* the nearest of the job's targets */
void game_debug_job(int type);          /* tools/host.c: start this job now */

/* story.c, for draw.c */
typedef struct { const char *name; float x, y; int look; } giver_t;
bool story_giver(giver_t *g);            /* who has the next mission, if anybody */
int  story_count(void);
void story_brief(const char **name, const char **title, const char *const **lines, int *reward);
int  story_chapter(int mission);        /* the first mission of the chapter a mission is in */
const char *story_chapter_name(int mission);    /* who gives that chapter's missions */
int  story_portrait(void);              /* PIC_* of who is talking */
const char *story_caption(int pic);     /* the line under a scene */

void draw_all(void);
extern int ground_only;

#endif
