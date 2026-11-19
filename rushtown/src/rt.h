/* rt.h -- Rushtown: what all parts of the game share.
 *
 * The game itself (world, art, game, hud, snd, gfx) knows nothing about the
 * hardware: it draws into a 240 x 280 frame of panel words and is fed the
 * buttons 50 times a second. src/pbc/ runs it on the PicoBoy Color Plus,
 * tools/host.c on a PC for tests and pictures.
 */
#ifndef RT_H
#define RT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* What can be left out again, each with one switch (CMake: -DRT_STORY=0 ...):
 *   RT_ISLANDS  Iron Isle and Palm Key, the long bridge and the ferry
 *   RT_STORY    the story missions (src/story.c)
 *   RT_MUSIC    the car radio (src/music.c)
 *   RT_SPEECH   spoken shouts (src/speech_data.c)
 *   RT_ART      the story's pictures: portraits, scenes, the title (src/pics.bin) */
#ifndef RT_ISLANDS
#define RT_ISLANDS 1
#endif
#ifndef RT_STORY
#define RT_STORY 1
#endif
#ifndef RT_MUSIC
#define RT_MUSIC 1
#endif
#ifndef RT_SPEECH
#define RT_SPEECH 1
#endif
#ifndef RT_ART
#define RT_ART 1
#endif

#define RT_VERSION "0.1"                /* shown on the title; count it up with every release */

#define SCR_W   240
#define SCR_H   280
#define TILE    16
#if RT_ISLANDS
#define MAP_W   160
#define MAP_H   150
#define ISLES   3
#else
#define MAP_W   96
#define MAP_H   96
#define ISLES   1
#endif
#define WORLD_W (MAP_W * TILE)
#define WORLD_H (MAP_H * TILE)

#define TICK_HZ 50
#define DT      (1.0f / TICK_HZ)

/* ---- colours ---------------------------------------------------------------
 * The panel wants blue in the upper five bits (MADCTL has BGR set), so no
 * raw 565 constants anywhere: RGB() is the one place that knows. */
typedef uint16_t px_t;
#define RGB(r, g, b) ((px_t)((((unsigned)(b) & 0xf8u) << 8) | \
                             (((unsigned)(g) & 0xfcu) << 3) | \
                             (((unsigned)(r) & 0xf8u) >> 3)))
#define HEX(v) RGB(((v) >> 16) & 0xff, ((v) >> 8) & 0xff, (v) & 0xff)

/* ---- language --------------------------------------------------------------
 * English and German, chosen on the title. Every text the player reads is
 * written as L("english", "deutsch") where it is used. The town's own names
 * (districts, signs, ranks, radio stations) stay as they are. */
extern int rt_lang;                     /* 0 english, 1 deutsch */
#define L(en, de) (rt_lang ? (de) : (en))

/* ---- buttons --------------------------------------------------------------- */
enum {
    BTN_UP = 1, BTN_DOWN = 2, BTN_LEFT = 4, BTN_RIGHT = 8,
    BTN_A = 16, BTN_B = 32, BTN_C = 64,
};

/* ---- gfx.c ----------------------------------------------------------------- */
extern px_t *g_fb;                      /* the frame being drawn, SCR_W x SCR_H */

void g_clear(px_t c);
void g_rect(int x, int y, int w, int h, px_t c);
void g_dark(int x, int y, int w, int h);            /* halves the brightness */
void g_blend(int x, int y, int w, int h, px_t c);   /* 50 % of c */
void g_frame(int x, int y, int w, int h, px_t c);
void g_pix(int x, int y, px_t c);
void g_line(int x0, int y0, int x1, int y1, px_t c);
void g_disc(int cx, int cy, int r, px_t c);
void g_ring(int cx, int cy, int r, px_t c);
void g_tile(const px_t *t, int x, int y);           /* 16 x 16, opaque */
/* 8 bit picture with a palette, colour 0 is transparent */
void g_sprite(const uint8_t *pix, int w, int h, const px_t *pal, int x, int y);
/* the same turned by `a` (radians, 0 = up, clockwise) around its middle,
 * which lands at cx, cy; shadow: darken instead of painting */
void g_sprite_rot(const uint8_t *pix, int w, int h, const px_t *pal,
                  float cx, float cy, float a, bool shadow);
/* a car: the same, with its ends and sides dented by `level` 0..3 each
 * (front, rear, left, right) */
void g_car(const uint8_t *pix, int w, int h, const px_t *pal,
           float cx, float cy, float a, bool shadow, const uint8_t *level);
/* one of the story's pictures, its upper left corner at x, y; false if the
 * game was built without them. The order is that of the sheet (tools/art.py). */
enum {
    PIC_ROSA, PIC_DAVE, PIC_LOLA, PIC_GRAY,
    PIC_TITLE, PIC_ARRIVAL, PIC_TRATTORIA, PIC_TOWER,
    PIC_HARBOUR, PIC_FERRY, PIC_BAR, PIC_BRIDGE,
    PIC_WORKS, PIC_GETAWAY, PIC_BUSTED, PIC_KO, PICS
};
bool g_picture(int id, int x, int y);
int  g_text(int x, int y, const char *s, px_t c, int scale);    /* returns the width */
int  g_text_w(const char *s, int scale);
void g_text_c(int cx, int y, const char *s, px_t c, int scale); /* centred, with a shadow */
void g_text_s(int x, int y, const char *s, px_t c, int scale);  /* with a shadow */
void g_text_o(int x, int y, const char *s, px_t c, int scale);  /* black all around: reads on anything */

/* ---- art.c ----------------------------------------------------------------- */
enum {
    T_WATER, T_SAND, T_GRASS, T_FLOWER, T_PAVE, T_PATH, T_LOT,
    T_ROAD, T_RV_L, T_RV_R, T_RH_T, T_RH_B, T_ZV, T_ZH,
    T_BV_L, T_BV_R, T_BS_L, T_BS_R,
    T_TREE, T_PALM, T_BUILD, T_FOUNT, T_PAD, T_BAY,
    T_BH_T, T_BH_B, T_BF_T, T_BF_B, T_BARRIER,  /* the long bridge: lanes, footways, shut */
    T_SW = 32,                          /* + mask of roads next to it: N1 E2 S4 W8 */
    T_COUNT = 48
};
extern px_t art_tile[T_COUNT][TILE * TILE];
extern px_t art_water[4][TILE * TILE];

enum { CAR_COMPACT, CAR_SEDAN, CAR_SPORT, CAR_VAN, CAR_TAXI, CAR_POLICE,
       CAR_BUS, CAR_TRUCK, CAR_BIKE, CAR_AMBULANCE, CAR_TOW, CAR_MODELS };
#define CAR_W_MAX 14
#define CAR_H_MAX 34
typedef struct {
    uint8_t w, h;
    const uint8_t *pix;
    float accel, vmax, grip, turn, mass;
} car_model_t;
extern car_model_t art_car[CAR_MODELS];
#define CAR_COLOURS 10
void art_car_palette(px_t *pal, int model, int colour, int frame);

#define PED_W 8
#define PED_H 8
extern uint8_t art_ped[3][PED_W * PED_H];
#define PED_LOOKS 8
void art_ped_palette(px_t *pal, int look);          /* look 255: the player */

extern uint8_t art_canopy[2][24 * 24];
extern px_t art_canopy_pal[2][8];
void art_init(void);

/* ---- world.c --------------------------------------------------------------- */
extern uint8_t map[MAP_H][MAP_W];

typedef struct { uint8_t x, y, w, h, style, colour, sign; } building_t;
enum { B_OFFICE, B_GLASS, B_HOUSE, B_HALL, B_BOX, B_SHOP, B_SHIP };
enum { SIGN_NONE, SIGN_H, SIGN_POLICE, SIGN_PAINT, SIGN_GARAGE, SIGN_AMMO, SIGN_TOWER, SIGN_WORKS, SIGN_BAR };
#define BUILDINGS_MAX 640
extern building_t buildings[BUILDINGS_MAX];
extern int nbuildings;

/* crossings of the road grid; e[d] is the node in direction d or NO_NODE */
enum { D_N, D_E, D_S, D_W };
#define NO_NODE 255
typedef struct { int16_t x, y; uint8_t e[4]; } node_t;
#define NODES_MAX 160
extern node_t nodes[NODES_MAX];
extern int nnodes;

typedef struct { int16_t x, y; } spot_t;
#if RT_ISLANDS
#define PHONES 12
#define TOKENS 32
#else
#define PHONES 8
#define TOKENS 24
#endif
#define PAINTS 2
extern spot_t w_phone[PHONES], w_token[TOKENS], w_paint[PAINTS];
extern spot_t w_hospital, w_police, w_start;
/* things to buy: where to stand; the garage's yard in tiles (x0, y0, x1, y1) */
extern spot_t w_garage, w_ammo, w_tower;
/* places of the story; the two piers of the ferry; the main island's end of the bridge */
extern spot_t w_square, w_harbour, w_works, w_bar, w_ferry[2], w_bridge;
extern uint8_t node_isle[];             /* which island a crossing is on */
int  world_isle(float x, float y);      /* which island a place is on, -1: none */
void world_open_bridge(bool open);
extern uint8_t w_yard[4];

void world_init(void);
static inline int tile_at(int tx, int ty)
{
    return (unsigned)tx < MAP_W && (unsigned)ty < MAP_H ? map[ty][tx] : T_WATER;
}
static inline int tile_px(float x, float y)
{
    return tile_at((int)x >> 4, (int)y >> 4);
}
static inline bool t_road(int t)   { return (t >= T_ROAD && t <= T_BV_R) || t == T_BH_T || t == T_BH_B; }
static inline bool t_solid(int t)  { return t == T_BUILD || t == T_TREE || t == T_PALM || t == T_FOUNT || t == T_BARRIER; }
static inline bool t_rough(int t)  { return t == T_SAND || t == T_GRASS || t == T_FLOWER; }
/* where people stroll */
static inline bool t_walk(int t)
{
    return t >= T_SW || t == T_PAVE || t == T_PATH || t == T_ZV || t == T_ZH ||
           t == T_BS_L || t == T_BS_R || t == T_BF_T || t == T_BF_B;
}
const char *world_district(float x, float y);
int world_node_near(float x, float y);
bool world_sight(float x0, float y0, float x1, float y1);  /* nothing solid between */

/* ---- snd.c ----------------------------------------------------------------- */
#define SND_RATE 22050
enum {
    SFX_STEP, SFX_DOOR, SFX_CRASH, SFX_BUMP, SFX_COIN, SFX_JOB, SFX_DONE, SFX_FAIL,
    SFX_BUSTED, SFX_SPLASH, SFX_HORN, SFX_PHONE, SFX_STAR, SFX_UI, SFX_YELP,
    SFX_SHOT, SFX_WHISTLE, SFX_BUY, SFX_RANK,
};
void snd_sfx(int id, int vol);          /* vol 0..255 */
void snd_engine(int hz, int vol);       /* every tick; vol 0 = off */
void snd_skid(int vol);
void snd_siren(bool on);
bool snd_siren_on(void);
void snd_mute(bool m);
void snd_render(uint8_t *out, int n);   /* unsigned 8 bit, rest level 128 */

/* music.c: the car radio. Station -1 is off, MUSIC_POLICE the police band. */
#define MUSIC_POLICE 99
int  music_stations(void);
const char *music_name(int n);
void music_station(int n);
int  music_sample(void);

/* speech_data.c: a few spoken shouts, played by snd_say() */
enum { SAY_STOP, SAY_PULL, SAY_ARREST, SAY_HEY, SAY_WATCH, SAY_TAXI, SAY_COUNT };
typedef struct { const uint8_t *pcm; uint32_t len; } speech_t;
extern const speech_t speech[2][SAY_COUNT];    /* by language */
void snd_say(int id);

/* ---- game.c ---------------------------------------------------------------- */
typedef struct {
    uint32_t magic;
    int32_t  cash, earned;
    uint32_t tokens;
    uint16_t jobs, bumped, ammo;
    uint8_t  sound, owns;               /* owns: OWN_* */
    uint8_t  story, lang;               /* the next story mission; rt_lang */
    uint8_t  yard[4][2];                /* cars in the garage: model + 1, colour */
    uint8_t  fails;                     /* arrests during the story's current mission, up to 3 */
    uint8_t  lives;                     /* 1 .. LIVES */
} save_t;
#define LIVES 5                         /* for each chapter of the story, each rank of the free game */
#define SAVE_MAGIC 0x52544f33u          /* "RTO3" */
enum { OWN_GARAGE = 1, OWN_TOWER = 2, OWN_BRIDGE = 4, OWN_FERRY = 8 };
enum { MODE_STORY, MODE_FREE, MODES };  /* each has its own saved game */
#define PRICE_FERRY  10
#define PRICE_GARAGE 1500
#define PRICE_TOWER  15000
#define PRICE_AMMO   100

void game_init(uint32_t seed);
void game_tick(uint32_t buttons);       /* one 1/50 s */
void game_draw(void);
/* the platform keeps these */
bool plat_load(save_t *s, int slot);
bool plat_save(const save_t *s, int slot);

uint32_t rnd(void);
static inline int rnd_n(int n) { return (int)(rnd() % (uint32_t)n); }

#endif
