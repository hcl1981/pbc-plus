/* art.c -- every picture of the game, made at start: ground tiles are
 * painted by small routines, cars and people come from the character
 * drawings below. Nothing is loaded from anywhere. */
#include <string.h>
#include <math.h>
#include "rt.h"

px_t art_tile[T_COUNT][TILE * TILE];
px_t art_water[4][TILE * TILE];

/* ---- ground ---------------------------------------------------------------- */

static uint32_t hash2(int x, int y, int s)
{
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + (uint32_t)s * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static uint32_t adj(uint32_t rgb, int d)
{
    int r = (int)((rgb >> 16) & 255) + d, g = (int)((rgb >> 8) & 255) + d, b = (int)(rgb & 255) + d;
    if (r < 0) r = 0;
    if (r > 255) r = 255;
    if (g < 0) g = 0;
    if (g > 255) g = 255;
    if (b < 0) b = 0;
    if (b > 255) b = 255;
    return (uint32_t)(r << 16 | g << 8 | b);
}

static px_t *cur;
static void put(int x, int y, uint32_t rgb)
{
    if ((unsigned)x < TILE && (unsigned)y < TILE)
        cur[y * TILE + x] = HEX(rgb);
}
static void box(int x, int y, int w, int h, uint32_t rgb)
{
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            put(x + i, y + j, rgb);
}
/* the whole tile in `rgb`, each pixel up to `amount` lighter or darker */
static void grain(uint32_t rgb, int amount, int seed)
{
    for (int y = 0; y < TILE; y++)
        for (int x = 0; x < TILE; x++)
            put(x, y, adj(rgb, (int)(hash2(x, y, seed) % (uint32_t)(2 * amount + 1)) - amount));
}

#define ASPHALT 0x3f4248
#define MARK    0xc8c2a0
#define WALK    0x9c9b94
#define GRASS   0x4c8a3c
#define SAND    0xd8c38a
#define WATER   0x2c6cae

static void sidewalk(int mask)
{
    grain(WALK, 3, 7);
    for (int i = 0; i < TILE; i++) {            /* joints of the slabs */
        put(i, 7, 0x8a8982);
        put(i, 15, 0x8a8982);
        put(7, i, 0x8a8982);
        put(15, i, 0x8a8982);
    }
    for (int i = 0; i < TILE; i++) {            /* kerb towards the road */
        if (mask & 1) { put(i, 0, 0x55554f); put(i, 1, 0xbdbcb4); }
        if (mask & 4) { put(i, 15, 0x55554f); put(i, 14, 0xbdbcb4); }
        if (mask & 8) { put(0, i, 0x55554f); put(1, i, 0xbdbcb4); }
        if (mask & 2) { put(15, i, 0x55554f); put(14, i, 0xbdbcb4); }
    }
}

static void tiles(void)
{
    for (int f = 0; f < 4; f++) {               /* water: glints that wander */
        cur = art_water[f];
        grain(WATER, 3, 11);
        for (int y = 0; y < TILE; y++)
            for (int x = 0; x < TILE; x++) {
                int w = (x + ((y * 5 + f * 4) & 15)) & 15;
                if ((y & 3) == 1 && w < 5)
                    put(x, y, w == 2 ? 0x7fb4e4 : 0x4f8fcc);
            }
    }
    memcpy(art_tile[T_WATER], art_water[0], sizeof art_water[0]);

    cur = art_tile[T_SAND];
    grain(SAND, 6, 2);
    cur = art_tile[T_GRASS];
    grain(GRASS, 7, 3);
    cur = art_tile[T_FLOWER];
    grain(GRASS, 7, 3);
    static const uint8_t fl[][2] = { {3, 4}, {11, 2}, {7, 9}, {13, 12}, {2, 13} };
    for (int i = 0; i < 5; i++)
        put(fl[i][0], fl[i][1], i & 1 ? 0xf4e04a : 0xf0f0f0);
    cur = art_tile[T_PAVE];
    grain(0xaaa59c, 3, 4);
    for (int i = 0; i < TILE; i++)
        for (int j = 0; j < TILE; j++)
            if ((i & 7) == 7 || (j & 7) == 7)
                put(i, j, 0x99948b);
    cur = art_tile[T_PATH];
    grain(0xb99f6c, 6, 5);

    cur = art_tile[T_LOT];
    grain(0x4b4e55, 3, 6);
    box(0, 0, 1, TILE, 0xd0d0c8);

    cur = art_tile[T_ROAD];
    grain(ASPHALT, 3, 1);
    cur = art_tile[T_RV_L];
    grain(ASPHALT, 3, 1);
    box(15, 2, 1, 6, MARK);
    cur = art_tile[T_RV_R];
    grain(ASPHALT, 3, 1);
    box(0, 2, 1, 6, MARK);
    cur = art_tile[T_RH_T];
    grain(ASPHALT, 3, 1);
    box(2, 15, 6, 1, MARK);
    cur = art_tile[T_RH_B];
    grain(ASPHALT, 3, 1);
    box(2, 0, 6, 1, MARK);
    cur = art_tile[T_ZV];                       /* zebra across a road that runs north-south */
    grain(ASPHALT, 3, 1);
    for (int x = 1; x < TILE; x += 4)
        box(x, 3, 2, 10, 0xd6d6ce);
    cur = art_tile[T_ZH];
    grain(ASPHALT, 3, 1);
    for (int y = 1; y < TILE; y += 4)
        box(3, y, 10, 2, 0xd6d6ce);

    cur = art_tile[T_BV_L];                     /* bridge deck */
    grain(0x55585e, 3, 1);
    box(15, 2, 1, 6, MARK);
    for (int y = 0; y < TILE; y += 8)
        box(0, y, TILE, 1, 0x484b51);
    cur = art_tile[T_BV_R];
    grain(0x55585e, 3, 1);
    box(0, 2, 1, 6, MARK);
    for (int y = 0; y < TILE; y += 8)
        box(0, y, TILE, 1, 0x484b51);
    for (int side = 0; side < 2; side++) {      /* its footways, railing outside */
        cur = art_tile[side ? T_BS_R : T_BS_L];
        grain(0x8f7350, 5, 9);
        for (int y = 3; y < TILE; y += 4)
            box(0, y, TILE, 1, 0x775e40);
        box(side ? 13 : 0, 0, 3, TILE, 0x3a3d42);
        box(side ? 14 : 1, 0, 1, TILE, 0xc9ccd2);
    }

    cur = art_tile[T_TREE];
    grain(GRASS, 7, 3);
    box(6, 6, 4, 4, 0x5b4024);
    cur = art_tile[T_PALM];
    grain(SAND, 6, 2);
    box(7, 7, 3, 3, 0x6b4a28);
    cur = art_tile[T_BUILD];
    grain(0x2a2c30, 2, 8);
    cur = art_tile[T_FOUNT];
    grain(0xaaa59c, 3, 4);
    for (int y = 0; y < TILE; y++)
        for (int x = 0; x < TILE; x++) {
            int d = (2 * x - 15) * (2 * x - 15) + (2 * y - 15) * (2 * y - 15);
            if (d < 15 * 15)
                put(x, y, d < 5 * 5 ? 0xd8eefc : d < 11 * 11 ? 0x4f9bdc : 0x70706a);
        }
    cur = art_tile[T_PAD];                      /* the paint shop's bay */
    grain(0x4b4e55, 3, 6);
    for (int y = 0; y < TILE; y++)
        for (int x = 0; x < TILE; x++)
            if (x < 2 || y < 2 || x > 13 || y > 13)
                put(x, y, ((x + y) >> 2) & 1 ? 0xe8c020 : 0x202020);

    for (int half = 0; half < 2; half++) {      /* the long bridge: its lanes, its footways */
        cur = art_tile[half ? T_BH_B : T_BH_T];
        grain(0x55585e, 3, 1);
        box(2, half ? 0 : 15, 6, 1, MARK);
        for (int x = 0; x < TILE; x += 8)
            box(x, 0, 1, TILE, 0x484b51);
        cur = art_tile[half ? T_BF_B : T_BF_T];
        grain(0x8f7350, 5, 9);
        for (int x = 3; x < TILE; x += 4)
            box(x, 0, 1, TILE, 0x775e40);
        box(0, half ? 13 : 0, TILE, 3, 0x3a3d42);
        box(0, half ? 14 : 1, TILE, 1, 0xc9ccd2);
    }
    cur = art_tile[T_BARRIER];                  /* shut: red and white */
    grain(0x55585e, 3, 1);
    for (int y = 0; y < TILE; y++)
        for (int x = 4; x < 12; x++)
            put(x, y, ((y + 2) >> 2) & 1 ? 0xd83030 : 0xf0f0f0);

    cur = art_tile[T_BAY];                      /* Main Street's parking lane */
    grain(0x474a50, 3, 6);
    box(0, 0, 1, 5, 0xc8c8c0);
    box(0, 11, 1, 5, 0xc8c8c0);

    for (int m = 0; m < 16; m++) {
        cur = art_tile[T_SW + m];
        sidewalk(m);
    }
}

/* ---- cars ------------------------------------------------------------------
 * Drawn nose up. o outline, B body, b its shade, L its highlight, W glass,
 * w glass with a glint, H head light, T tail light, K tyre, S and s roof
 * sign or the two police lights, X stripe. */
static const char *const SEDAN[] = {
    "...oooooo...",
    "..oHBBBBHo..",
    ".oBBBBBBBBo.",
    ".oBLLLLLLBo.",
    "KoBLLLLLLBoK",
    "KoBBBBBBBBoK",
    "KoBwWWWWWBoK",
    ".obWWWWWWbo.",
    ".obWbbbbWbo.",
    ".oBbBBBBbBo.",
    ".oBbBLLBbBo.",
    ".oBbBLLBbBo.",
    ".oBbBBBBbBo.",
    ".oBbBBBBbBo.",
    ".obWbbbbWbo.",
    ".obWWWWWWbo.",
    "KoBWWWWWWBoK",
    "KoBBBBBBBBoK",
    "KoBbbbbbbBoK",
    ".oBBBBBBBBo.",
    "..oTBBBBTo..",
    "...oooooo...", 0
};
static const char *const COMPACT[] = {
    "...oooooo...",
    "..oHBBBBHo..",
    ".oBLLLLLLBo.",
    "KoBLLLLLLBoK",
    "KoBBBBBBBBoK",
    ".obwWWWWWbo.",
    ".obWWWWWWbo.",
    ".oBbBBBBbBo.",
    ".oBbBLLBbBo.",
    ".oBbBLLBbBo.",
    ".oBbBBBBbBo.",
    ".obWWWWWWbo.",
    "KoBWWWWWWBoK",
    "KoBBBBBBBBoK",
    ".oBbbbbbbBo.",
    ".oBBBBBBBBo.",
    "..oTBBBBTo..",
    "...oooooo...", 0
};
static const char *const SPORT[] = {
    "....oooo....",
    "..ooBXXBoo..",
    ".oHBBXXBBHo.",
    ".oBLLXXLLBo.",
    "KoBLLXXLLBoK",
    "KoBBBXXBBBoK",
    ".oBBBXXBBBo.",
    ".obwWWWWWbo.",
    ".obWWWWWWbo.",
    ".obWbXXbWbo.",
    ".oBbBXXBbBo.",
    ".oBbBXXBbBo.",
    ".obWbXXbWbo.",
    ".obWWWWWWbo.",
    ".oBBBXXBBBo.",
    "KoBBBXXBBBoK",
    "KoBBBXXBBBoK",
    "KoBbbXXbbBoK",
    ".oBBBXXBBBo.",
    ".oTTBXXBTTo.",
    "..oooooooo..", 0
};
static const char *const VAN[] = {
    "..oooooooo..",
    ".oHBBBBBBHo.",
    ".oBBBBBBBBo.",
    "KoBwWWWWWBoK",
    "KoBWWWWWWBoK",
    ".oBbbbbbbBo.",
    ".oBLLLLLLBo.",
    ".oBLLLLLLBo.",
    ".oBLLLLLLBo.",
    ".oBLbbbbLBo.",
    ".oBLLLLLLBo.",
    ".oBLLLLLLBo.",
    ".oBLLLLLLBo.",
    ".oBLbbbbLBo.",
    ".oBLLLLLLBo.",
    ".oBLLLLLLBo.",
    ".oBLLLLLLBo.",
    ".oBLLLLLLBo.",
    "KoBLLLLLLBoK",
    "KoBLLLLLLBoK",
    "KoBbbbbbbBoK",
    ".oBBBBBBBBo.",
    ".oTBBBBBBTo.",
    "..oooooooo..", 0
};

static const char *const BIKE[] = {
    "..KK..",
    "..KK..",
    "..HH..",
    ".oBBo.",
    "boBBob",
    ".bXXb.",
    ".bXXb.",
    ".bbbb.",
    ".obbo.",
    ".oBBo.",
    "..BB..",
    "..TT..",
    "..KK..",
    "..KK..", 0
};

static uint8_t car_pix[CAR_MODELS][CAR_W_MAX * CAR_H_MAX];
car_model_t art_car[CAR_MODELS];

static int role(char c)
{
    static const char roles[] = ".oBbLWwHTKSsX";
    const char *p = strchr(roles, c);
    return p ? (int)(p - roles) : 0;
}

static void car(int model, const char *const *rows, float accel, float vmax, float grip, float turn, float mass)
{
    int h = 0, w = (int)strlen(rows[0]);
    for (; rows[h]; h++)
        for (int x = 0; x < w; x++)
            car_pix[model][h * w + x] = (uint8_t)role(rows[h][x]);
    art_car[model] = (car_model_t){ (uint8_t)w, (uint8_t)h, car_pix[model], accel, vmax, grip, turn, mass };
}

/* The long ones are painted, not drawn: a cab with a windscreen and, behind
 * it, a bus's roof with windows along the sides, a lorry's box, or a tow
 * truck's bed with its arm. Colour numbers as role() gives them. */
enum { LONG_BUS, LONG_TRUCK, LONG_TOW };
static void long_one(int model, int w, int h, int kind, float accel, float vmax, float grip, float turn, float mass)
{
    uint8_t *p = car_pix[model];
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int c = 2;
            bool edge = x == 1 || x == w - 2 || y == 0 || y == h - 1;
            if (x == 0 || x == w - 1)
                c = (y >= 3 && y <= 6) || (y >= h - 9 && y <= h - 5) ? 9 : 0;
            else if ((x == 1 || x == w - 2) && (y == 0 || y == h - 1))
                c = 0;
            else if (edge)
                c = 1;
            else if (y == 1 && (x == 2 || x == w - 3))
                c = 7;
            else if (y == h - 2 && (x == 2 || x == w - 3))
                c = 8;
            else if ((y == 3 || y == 4) && x >= 3 && x <= w - 4)
                c = y == 3 && x == 3 ? 6 : 5;
            else if (y >= 6 && y <= h - 3) {
                if (kind == LONG_BUS) {
                    if (x == 2 || x == w - 3)
                        c = y % 4 ? 5 : 2;
                    else
                        c = (y == 12 || y == 13 || y == h - 12 || y == h - 11) && x >= 5 && x <= w - 6 ? 3 : 4;
                } else if (y == 8)
                    c = 1;                      /* the gap behind the cab */
                else if (y > 8 && kind == LONG_TRUCK)
                    c = (x == 2 || x == w - 3 || y == 9 || y == h - 3) ? 3 : y % 5 == 0 ? 3 : 4;
                else if (y > 8)                 /* the bed, the arm down its middle, a lamp on the cab */
                    c = x == w / 2 || x == w / 2 - 1 ? (y < 12 ? 10 : 1) : 9;
            }
            p[y * w + x] = (uint8_t)c;
        }
    art_car[model] = (car_model_t){ (uint8_t)w, (uint8_t)h, p, accel, vmax, grip, turn, mass };
}

static const uint32_t body[CAR_COLOURS] = {
    0xc23a32, 0x2f6fbe, 0x3c9a4e, 0xd0d4d8, 0x30343a,
    0xd88a2a, 0x7a4bb0, 0x2fa3a0, 0x9a6a3c, 0xe05a9a,
};

void art_car_palette(px_t *pal, int model, int colour, int frame)
{
    uint32_t b = body[colour % CAR_COLOURS], x = 0xf0f0f0, s1 = 0xf0f0f0, s2 = 0xf0f0f0;
    if (model == CAR_AMBULANCE) {               /* white, a red cross, red lights when it is called */
        b = 0xeef0f2;
        x = 0xd02828;
        s1 = frame & 8 ? 0xff3030 : 0x702020;
        s2 = frame & 8 ? 0x702020 : 0xff3030;
    } else if (model == CAR_TOW) {
        b = 0xd98a20;
        s1 = s2 = frame & 8 ? 0xffc020 : 0x705010;
    } else if (model == CAR_BIKE)
        x = 0xf0f0f0;                           /* the helmet */
    else if (model == CAR_TAXI) {
        b = 0xe8b820;
        s1 = s2 = 0x202020;
    } else if (model == CAR_POLICE) {
        b = 0xe4e6ea;
        x = 0x2850b8;
        s1 = frame & 8 ? 0xff3030 : 0x601818;
        s2 = frame & 8 ? 0x182060 : 0x3060ff;
    } else if (colour % CAR_COLOURS == 3)
        x = 0xc23a32;
    pal[0] = 0;
    pal[1] = HEX(adj(b, -255) | 0x101014);          /* o */
    pal[2] = HEX(b);
    pal[3] = HEX(adj(b, -38));
    pal[4] = HEX(adj(b, 30));
    pal[5] = HEX(0x26323e);                         /* W */
    pal[6] = HEX(0x6a8aa8);
    pal[7] = HEX(0xfff2b0);
    pal[8] = HEX(0xd82020);
    pal[9] = HEX(0x101012);
    pal[10] = HEX(s1);
    pal[11] = HEX(s2);
    pal[12] = HEX(x);
}

/* ---- people ---------------------------------------------------------------- */
static const char *const PED[3][PED_H] = {
    { "........", "...ss...", ".cChhCc.", ".CChhCC.", ".sCCCCs.", "........", "........", "........" },
    { "..p.....", "...ss...", ".cChhCc.", "sCChhCC.", "..CCCCs.", ".....p..", "........", "........" },
    { ".....p..", "...ss...", ".cChhCc.", ".CChhCCs", ".sCCCC..", "..p.....", "........", "........" },
};
uint8_t art_ped[3][PED_W * PED_H];

void art_ped_palette(px_t *pal, int look)
{
    static const uint32_t shirt[PED_LOOKS] = {
        0x3a6fd0, 0xd04a4a, 0x4aa85a, 0xe0e0e0, 0x8a4ab0, 0xe09030, 0x30a0a8, 0x606870,
    };
    static const uint32_t hair[4] = { 0x2a2018, 0x6a4a20, 0xd8b050, 0x909090 };
    static const uint32_t skin[3] = { 0xf0c8a0, 0xc89468, 0x8a5c3a };
    uint32_t sh = 0xffd820, ha = 0x2a2018, sk = 0xf0c8a0;
    if (look == 254) {                              /* a police officer */
        sh = 0x283c90;
        ha = 0x182050;
    } else if (look != 255) {
        sh = shirt[look % PED_LOOKS];
        ha = hair[(look / 8) & 3];
        sk = skin[(look / 3) % 3];
    }
    pal[0] = 0;
    pal[1] = HEX(ha);
    pal[2] = HEX(sk);
    pal[3] = HEX(sh);
    pal[4] = HEX(adj(sh, -45));
    pal[5] = HEX(0x202024);
}

/* ---- tree tops ------------------------------------------------------------- */
uint8_t art_canopy[2][24 * 24];
px_t art_canopy_pal[2][8];

static void canopies(void)
{
    for (int y = 0; y < 24; y++)
        for (int x = 0; x < 24; x++) {
            float dx = (float)x - 11.5f, dy = (float)y - 11.5f;
            float r = sqrtf(dx * dx + dy * dy), a = atan2f(dy, dx);
            /* a round tree: a lumpy disc, lit from the upper left */
            float edge = 9.6f + 1.4f * sinf(a * 5.0f) + (float)(hash2(x, y, 21) % 3) * 0.4f;
            int c = 0;
            if (r < edge) {
                float lit = (-dx - dy) / 14.0f + (float)(hash2(x / 2, y / 2, 22) % 5) * 0.12f;
                c = r > edge - 1.2f ? 1 : lit > 0.55f ? 4 : lit > 0.1f ? 3 : 2;
            }
            art_canopy[0][y * 24 + x] = (uint8_t)c;
            /* a palm: seven fronds */
            float f = cosf(a * 7.0f + 0.4f);
            c = 0;
            if (r < 2.5f)
                c = 1;
            else if (r < 11.5f && f > 0.25f + r * 0.035f)
                c = f > 0.8f ? 4 : (-dx - dy) > 0 ? 3 : 2;
            art_canopy[1][y * 24 + x] = (uint8_t)c;
        }
    static const uint32_t tree[5] = { 0, 0x1f4a22, 0x2d6b2c, 0x3c8a36, 0x58a848 };
    static const uint32_t palm[5] = { 0, 0x3a2c14, 0x2f7a34, 0x44983c, 0x74bc4c };
    for (int i = 0; i < 5; i++) {
        art_canopy_pal[0][i] = HEX(tree[i]);
        art_canopy_pal[1][i] = HEX(palm[i]);
    }
}

void art_init(void)
{
    tiles();
    /*                          accel  vmax  grip  turn  mass */
    car(CAR_COMPACT,   COMPACT, 150, 150,  9.0f, 3.4f, 1.0f);
    car(CAR_SEDAN,     SEDAN,   160, 175,  8.0f, 3.0f, 1.2f);
    car(CAR_SPORT,     SPORT,   230, 235,  7.0f, 3.2f, 1.1f);
    car(CAR_VAN,       VAN,     110, 135,  9.0f, 2.5f, 1.8f);
    car(CAR_TAXI,      SEDAN,   160, 175,  8.0f, 3.0f, 1.2f);
    car(CAR_POLICE,    SEDAN,   200, 205,  8.5f, 3.2f, 1.3f);
    car(CAR_BIKE,      BIKE,    260, 225, 10.0f, 4.2f, 0.4f);
    car(CAR_AMBULANCE, VAN,     130, 160,  9.0f, 2.6f, 1.8f);
    long_one(CAR_BUS,   14, 34, LONG_BUS,    70, 105, 9.0f, 1.7f, 4.0f);
    long_one(CAR_TRUCK, 14, 30, LONG_TRUCK,  80, 115, 9.0f, 1.9f, 3.5f);
    long_one(CAR_TOW,   14, 26, LONG_TOW,   100, 125, 9.0f, 2.2f, 2.5f);
    /* the ambulance: lights over the cab, a cross on the roof */
    memcpy(car_pix[CAR_AMBULANCE] + 5 * 12, "\0\1\2\12\12\13\13\12\12\2\1\0", 12);
    for (int y = 9; y <= 15; y++)
        for (int x = 3; x <= 8; x++)
            if (x == 5 || x == 6 || y == 12 || y == 11 || y == 13)
                if ((x >= 5 && x <= 6) || (y >= 11 && y <= 13))
                    car_pix[CAR_AMBULANCE][y * 12 + x] = 12;
    /* the sign on the taxi's roof, the light bar and stripe of the police */
    memcpy(car_pix[CAR_TAXI] + 10 * 12, "\0\1\2\3\2\12\12\2\3\2\1\0", 12);
    memcpy(car_pix[CAR_POLICE] + 9 * 12, "\0\1\14\14\14\14\14\14\14\14\1\0", 12);
    memcpy(car_pix[CAR_POLICE] + 10 * 12, "\0\1\2\3\12\12\13\13\3\2\1\0", 12);
    memcpy(car_pix[CAR_POLICE] + 11 * 12, "\0\1\2\3\12\12\13\13\3\2\1\0", 12);
    memcpy(car_pix[CAR_POLICE] + 13 * 12, "\0\1\14\14\14\14\14\14\14\14\1\0", 12);
    for (int f = 0; f < 3; f++)
        for (int y = 0; y < PED_H; y++)
            for (int x = 0; x < PED_W; x++) {
                static const char roles[] = ".hsCcp";
                const char *p = strchr(roles, PED[f][y][x]);
                art_ped[f][y * PED_W + x] = (uint8_t)(p ? p - roles : 0);
            }
    canopies();
}
