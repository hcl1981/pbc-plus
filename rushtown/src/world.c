/* world.c -- the town. It is built at start from a fixed seed, so it is the
 * same every time and players get to know it.
 *
 * The main island: a ring road, a grid of two-lane roads with some
 * stretches left out (they are pedestrian zones), a river with four
 * bridges, and between the roads blocks filled after the district they are
 * in. With RT_ISLANDS two more, built by the same code in smaller: Iron
 * Isle in the east, at the end of a long bridge, and Palm Key in the south,
 * where the ferry goes. Everything below works in an island's own tiles;
 * M(), set() and the *_out() helpers put it where the island lies.
 */
#include <string.h>
#include <stdlib.h>
#include "rt.h"

uint8_t map[MAP_H][MAP_W];
building_t buildings[BUILDINGS_MAX];
int nbuildings;
node_t nodes[NODES_MAX];
uint8_t node_isle[NODES_MAX];
int nnodes;
spot_t w_phone[PHONES], w_token[TOKENS], w_paint[PAINTS];
spot_t w_hospital, w_police, w_start;
spot_t w_garage, w_ammo, w_tower;
spot_t w_square, w_harbour, w_works, w_bar, w_ferry[2], w_bridge;
uint8_t w_yard[4];

#define LINES 12
enum { MAIN, IRON, PALM };
typedef struct {
    int ox, oy, w, h;                   /* where it lies and how big it is, in tiles */
    int nvx, nhy, vx[LINES], hy[LINES], base;   /* its road lines, its first node */
} isle_t;
static isle_t isles[ISLES];
static int kind;                        /* the island being built */
static int OX, OY, IW, IH, base;
#define M(x, y) map[(y) + OY][(x) + OX]

static int nvx, nhy, vx[LINES], hy[LINES];
static bool seg_v[LINES][LINES];        /* on line i from row j to j + 1 */
static bool seg_h[LINES][LINES];        /* on row j from line i to i + 1 */
static int river_j, river_y;            /* the block row it runs through, its first tile row */
static int wide_j;                      /* the row that is Main Street: a parking lane each side */
static int sq_i, sq_j;                  /* the square: this block and the one east of it */
static int hb_j;                        /* the harbour: the easternmost block of this row */
static int main_river_y, main_wide_j;
#define RIVER_H 3
#define CITY0 8                         /* the first road line; the last is 10 from the far edge */

static uint32_t ws;
static uint32_t wr(void)
{
    ws ^= ws << 13;
    ws ^= ws >> 17;
    ws ^= ws << 5;
    return ws;
}
static int wn(int n) { return (int)(wr() % (uint32_t)n); }
static int wrange(int a, int b) { return a + wn(b - a + 1); }

static void set(int x, int y, int t)
{
    if ((unsigned)x < (unsigned)IW && (unsigned)y < (unsigned)IH)
        M(x, y) = (uint8_t)t;
}
static spot_t spot_out(int tx, int ty)  /* the middle of an island's tile, in the world's pixels */
{
    return (spot_t){ (int16_t)((tx + OX) * TILE + 8), (int16_t)((ty + OY) * TILE + 8) };
}
static void fill(int x0, int y0, int x1, int y1, int t)     /* inclusive */
{
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            set(x, y, t);
}

/* first and last tile row of the road on row j */
static int top(int j) { return hy[j] - (j == wide_j ? 1 : 0); }
static int bot(int j) { return hy[j] + 1 + (j == wide_j ? 1 : 0); }

static bool land(int x, int y)
{
    const int a = 3, bx = IW - 4, by = IH - 4, r = kind == MAIN ? 9 : 7;
    if (x < a || x > bx || y < a || y > by)
        return false;
    int dx = x < a + r ? a + r - x : x > bx - r ? x - (bx - r) : 0;
    int dy = y < a + r ? a + r - y : y > by - r ? y - (by - r) : 0;
    return dx * dx + dy * dy <= r * r;
}

static int lines(int *p, int last)
{
    int n = 0, x = CITY0;
    p[n++] = x;
    for (;;) {
        int step = wrange(8, 16);
        if (x + step > last - 8)
            break;
        x += step;
        p[n++] = x;
    }
    if (last - x > 16)
        p[n++] = x + (last - x) / 2;
    p[n++] = last;
    return n;
}

/* ---- the road graph ---------------------------------------------------------- */

static int degree(int i, int j)
{
    return (j > 0 && seg_v[i][j - 1]) + (j < nhy - 1 && seg_v[i][j]) +
           (i > 0 && seg_h[i - 1][j]) + (i < nvx - 1 && seg_h[i][j]);
}

static bool connected(void)
{
    static uint8_t seen[LINES * LINES], stack[LINES * LINES];
    int sp = 0, count = 0;
    memset(seen, 0, sizeof seen);
    stack[sp++] = 0;
    seen[0] = 1;
    while (sp) {
        int n = stack[--sp], i = n % nvx, j = n / nvx;
        count++;
        int nb[4] = { -1, -1, -1, -1 };
        if (j > 0 && seg_v[i][j - 1]) nb[0] = n - nvx;
        if (j < nhy - 1 && seg_v[i][j]) nb[1] = n + nvx;
        if (i > 0 && seg_h[i - 1][j]) nb[2] = n - 1;
        if (i < nvx - 1 && seg_h[i][j]) nb[3] = n + 1;
        for (int k = 0; k < 4; k++)
            if (nb[k] >= 0 && !seen[nb[k]]) {
                seen[nb[k]] = 1;
                stack[sp++] = (uint8_t)nb[k];
            }
    }
    return count == nvx * nhy;
}

static void graph(void)
{
    for (int i = 0; i < nvx; i++)
        for (int j = 0; j < nhy; j++) {
            seg_v[i][j] = j < nhy - 1;
            seg_h[i][j] = i < nvx - 1;
        }
    /* the river: through the block row nearest the middle that is high enough */
    river_j = -1;
    for (int j = 1; j < nhy - 2 && kind == MAIN; j++) {
        int inner = hy[j + 1] - hy[j] - 4;
        if (inner >= 8 && (river_j < 0 || abs(hy[j] + hy[j + 1] - 94) < abs(hy[river_j] + hy[river_j + 1] - 94)))
            river_j = j;
    }
    if (river_j >= 0) {
        int y0 = hy[river_j] + 3, inner = hy[river_j + 1] - hy[river_j] - 4;
        river_y = y0 + (inner - RIVER_H) / 2;
        for (int i = 0; i < nvx; i++)
            if (i != 0 && i != nvx - 1 && i != nvx / 3 && i != 2 * nvx / 3)
                seg_v[i][river_j] = false;
    }
    /* Main Street: a row in the northern half, away from the river */
    wide_j = nhy / 3;
    if (wide_j < 1)
        wide_j = 1;
    while (wide_j == river_j || wide_j == river_j + 1)
        wide_j--;
    if (wide_j < 1 || kind != MAIN)
        wide_j = -1;
    /* the harbour: the tallest row south of the river; the square: the
     * tallest north of it, two blocks in the middle with no road between */
    hb_j = sq_j = -1;
    for (int j = 0; j < nhy - 1 && kind == MAIN; j++) {
        if (j == river_j)
            continue;
        int *best = river_j >= 0 && j > river_j ? &hb_j : &sq_j;
        if (*best < 0 || hy[j + 1] - hy[j] > hy[*best + 1] - hy[*best])
            *best = j;
    }
    sq_i = nvx / 2 - 1;
    if (sq_j >= 0) {
        seg_v[sq_i + 1][sq_j] = false;
        if (degree(sq_i + 1, sq_j) < 2 || degree(sq_i + 1, sq_j + 1) < 2)
            seg_v[sq_i + 1][sq_j] = true;
    }
    /* leave some stretches out, as long as every crossing keeps two roads
     * and the town stays in one piece */
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i < nvx; i++)
            for (int j = 0; j < nhy; j++) {
                bool *s = pass ? &seg_h[i][j] : &seg_v[i][j];
                bool ring = pass ? (j == 0 || j == nhy - 1) : (i == 0 || i == nvx - 1);
                if (!*s || ring || (!pass && j == river_j) || wn(100) >= 17)
                    continue;
                *s = false;
                int i2 = pass ? i + 1 : i, j2 = pass ? j : j + 1;
                if (degree(i, j) < 2 || degree(i2, j2) < 2 || !connected())
                    *s = true;
            }

    if (kind == MAIN) {                         /* the districts and the bridge go by these */
        main_river_y = river_j >= 0 ? river_y : IH / 2;
        main_wide_j = wide_j;
    }
    for (int j = 0; j < nhy; j++)
        for (int i = 0; i < nvx; i++) {
            int k = base + j * nvx + i;
            node_t *n = &nodes[k];
            node_isle[k] = (uint8_t)kind;
            n->x = (int16_t)((vx[i] + 1 + OX) * TILE);
            n->y = (int16_t)((hy[j] + 1 + OY) * TILE);
            n->e[D_N] = j > 0 && seg_v[i][j - 1] ? (uint8_t)(k - nvx) : NO_NODE;
            n->e[D_S] = j < nhy - 1 && seg_v[i][j] ? (uint8_t)(k + nvx) : NO_NODE;
            n->e[D_W] = i > 0 && seg_h[i - 1][j] ? (uint8_t)(k - 1) : NO_NODE;
            n->e[D_E] = i < nvx - 1 && seg_h[i][j] ? (uint8_t)(k + 1) : NO_NODE;
        }
    nnodes = base + nvx * nhy;
}

static void roads(void)
{
    /* the stretches; a missing one is paved, with a planter now and then */
    for (int j = 0; j < nhy; j++)
        for (int i = 0; i < nvx - 1; i++) {
            int x0 = vx[i] + 2, x1 = vx[i + 1] - 1, y = hy[j], ya = top(j), yb = bot(j);
            if (seg_h[i][j]) {
                fill(x0, ya - 1, x1, ya - 1, T_SW);
                fill(x0, yb + 1, x1, yb + 1, T_SW);
                fill(x0, ya, x1, yb, T_BAY);    /* what is left of it on Main Street */
                fill(x0, y, x1, y, T_RH_T);
                fill(x0, y + 1, x1, y + 1, T_RH_B);
                fill(x0, ya, x0, yb, T_ZH);
                fill(x1, ya, x1, yb, T_ZH);
            } else {
                fill(x0, ya - 1, x1, yb + 1, T_PAVE);
                for (int x = x0 + 2; x < x1 - 1; x += 3)
                    set(x, y + (x & 1), T_TREE);
            }
        }
    for (int i = 0; i < nvx; i++)
        for (int j = 0; j < nhy - 1; j++) {
            int y0 = bot(j) + 1, y1 = top(j + 1) - 1, x = vx[i];
            if (seg_v[i][j]) {
                fill(x - 1, y0, x - 1, y1, T_SW);
                fill(x + 2, y0, x + 2, y1, T_SW);
                fill(x, y0, x, y1, T_RV_L);
                fill(x + 1, y0, x + 1, y1, T_RV_R);
                fill(x, y0, x + 1, y0, T_ZV);
                fill(x, y1, x + 1, y1, T_ZV);
            } else {
                fill(x - 1, y0, x + 2, y1, T_PAVE);
                for (int y = y0 + 2; y < y1 - 1; y += 3)
                    set(x + (y & 1), y, T_TREE);
            }
        }
    /* the crossings: road in the middle, footway at the four corners and
     * on every side that has no road */
    for (int j = 0; j < nhy; j++)
        for (int i = 0; i < nvx; i++) {
            int ya = top(j), yb = bot(j);
            const node_t *n = &nodes[base + j * nvx + i];
            set(vx[i] - 1, ya - 1, T_SW);
            set(vx[i] + 2, ya - 1, T_SW);
            set(vx[i] - 1, yb + 1, T_SW);
            set(vx[i] + 2, yb + 1, T_SW);
            fill(vx[i], ya, vx[i] + 1, yb, T_ROAD);
            if (n->e[D_N] == NO_NODE) fill(vx[i], ya - 1, vx[i] + 1, ya - 1, T_SW);
            if (n->e[D_S] == NO_NODE) fill(vx[i], yb + 1, vx[i] + 1, yb + 1, T_SW);
            if (n->e[D_W] == NO_NODE) fill(vx[i] - 1, ya, vx[i] - 1, yb, T_SW);
            if (n->e[D_E] == NO_NODE) fill(vx[i] + 2, ya, vx[i] + 2, yb, T_SW);
        }
}

static void river(void)
{
    if (river_j < 0)
        return;
    for (int y = river_y; y < river_y + RIVER_H; y++)
        for (int x = 0; x < IW; x++) {
            int t = M(x, y);
            if (t == T_RV_L)
                t = T_BV_L;
            else if (t == T_RV_R)
                t = T_BV_R;
            else if (t >= T_SW && x + 1 < IW && t_road(M(x + 1, y)))
                t = T_BS_L;
            else if (t >= T_SW && x > 0 && (M(x - 1, y) == T_BV_R || M(x - 1, y) == T_BV_L))
                t = T_BS_R;
            else
                t = T_WATER;
            M(x, y) = (uint8_t)t;
        }
}

/* ---- blocks ------------------------------------------------------------------ */

static void building(int x0, int y0, int x1, int y1, int style, int colour, int sign)
{
    if (nbuildings >= BUILDINGS_MAX || x1 < x0 || y1 < y0)
        return;
    fill(x0, y0, x1, y1, T_BUILD);
    buildings[nbuildings++] = (building_t){ (uint8_t)(x0 + OX), (uint8_t)(y0 + OY), (uint8_t)(x1 - x0 + 1),
                                            (uint8_t)(y1 - y0 + 1), (uint8_t)style, (uint8_t)colour, (uint8_t)sign };
}

/* offices: cut the block up until the pieces are building sized */
static void offices(int x0, int y0, int x1, int y1, int depth)
{
    int w = x1 - x0 + 1, h = y1 - y0 + 1;
    if (w < 2 || h < 2) {
        fill(x0, y0, x1, y1, T_PAVE);
        return;
    }
    if ((w <= 5 && h <= 5) || depth > 3 || (w <= 6 && h <= 6 && wn(3) == 0)) {
        building(x0, y0, x1, y1, wn(3) ? B_OFFICE : B_GLASS, wn(8), SIGN_NONE);
        return;
    }
    bool alley = wn(3) != 0;
    if (w >= h) {
        int cut = x0 + wrange(2, w - 3);
        offices(x0, y0, cut - 1, y1, depth + 1);
        if (alley)
            fill(cut, y0, cut, y1, T_PAVE);
        offices(cut + (alley ? 1 : 0), y0, x1, y1, depth + 1);
    } else {
        int cut = y0 + wrange(2, h - 3);
        offices(x0, y0, x1, cut - 1, depth + 1);
        if (alley)
            fill(x0, cut, x1, cut, T_PAVE);
        offices(x0, cut + (alley ? 1 : 0), x1, y1, depth + 1);
    }
}

static void houses(int x0, int y0, int x1, int y1)
{
    fill(x0, y0, x1, y1, T_GRASS);
    for (int y = y0; y + 2 <= y1; y += 4)
        for (int x = x0; x + 2 <= x1;) {
            int w = wn(3) == 0 && x + 3 <= x1 ? 4 : 3;
            if (wn(7))
                building(x, y, x + w - 1, y + 2, B_HOUSE, wn(8), SIGN_NONE);
            x += w + 1;
        }
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            if (M(x, y) == T_GRASS) {
                int r = wn(100);
                if (r < 7)
                    M(x, y) = T_TREE;
                else if (r < 16)
                    M(x, y) = T_FLOWER;
            }
}

static void park(int x0, int y0, int x1, int y1)
{
    int w = x1 - x0 + 1, h = y1 - y0 + 1, cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
    fill(x0, y0, x1, y1, T_GRASS);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            int r = wn(100);
            if (r < 15)
                M(x, y) = T_TREE;
            else if (r < 28)
                M(x, y) = T_FLOWER;
        }
    if (w >= 6 && h >= 6) {                     /* a pond off the middle */
        int px = cx + (wn(2) ? 2 : -2), py = cy + (wn(2) ? 2 : -2);
        for (int y = py - 1; y <= py + 1; y++)
            for (int x = px - 1; x <= px + 1; x++)
                if (x > x0 && x < x1 && y > y0 && y < y1 && (x != px - 1 || y != py - 1))
                    M(x, y) = T_WATER;
    }
    fill(x0, cy, x1, cy, T_PATH);
    fill(cx, y0, cx, y1, T_PATH);
}

static void parking(int x0, int y0, int x1, int y1)
{
    fill(x0, y0, x1, y1, T_LOT);
}

static void plaza(int x0, int y0, int x1, int y1)
{
    fill(x0, y0, x1, y1, T_PAVE);
    set((x0 + x1) / 2, (y0 + y1) / 2, T_FOUNT);
    set(x0, y0, T_TREE);
    set(x1, y0, T_TREE);
    set(x0, y1, T_TREE);
    set(x1, y1, T_TREE);
}

static void halls(int x0, int y0, int x1, int y1)
{
    int w = x1 - x0 + 1, h = y1 - y0 + 1;
    fill(x0, y0, x1, y1, T_LOT);
    if (w >= h) {
        int cut = x0 + w / 2;
        building(x0, y0, cut - 2, y1 - 2, B_HALL, wn(8), SIGN_NONE);
        building(cut + 1, y0 + 2, x1, y1, B_HALL, wn(8), SIGN_NONE);
    } else {
        int cut = y0 + h / 2;
        building(x0, y0, x1 - 2, cut - 2, B_HALL, wn(8), SIGN_NONE);
        building(x0 + 2, cut + 1, x1, y1, B_HALL, wn(8), SIGN_NONE);
    }
}

static void containers(int x0, int y0, int x1, int y1)
{
    fill(x0, y0, x1, y1, T_PAVE);
    for (int y = y0; y <= y1; y += 2)
        for (int x = x0; x + 1 <= x1; x += 3)
            if (wn(4))
                building(x, y, x + 1, y, B_BOX, wn(8), SIGN_NONE);
}

/* a building with something in front: hospital, police, paint shop */
static spot_t special(int x0, int y0, int x1, int y1, int sign)
{
    int mid = (y0 + y1) / 2;
    bool yard = sign == SIGN_PAINT || sign == SIGN_GARAGE || sign == SIGN_WORKS;
    fill(x0, y0, x1, y1, yard ? T_LOT : T_PAVE);
    building(x0, y0, x1, mid - 1, sign == SIGN_PAINT || sign == SIGN_AMMO || sign == SIGN_BAR ? B_SHOP : sign == SIGN_TOWER ? B_GLASS
             : sign == SIGN_GARAGE || sign == SIGN_WORKS ? B_HALL : B_OFFICE,
             sign == SIGN_H ? 3 : sign == SIGN_POLICE ? 1 : 5, sign);
    if (sign == SIGN_GARAGE) {
        w_yard[0] = (uint8_t)(x0 + OX); w_yard[1] = (uint8_t)(mid + OY); w_yard[2] = (uint8_t)(x1 + OX); w_yard[3] = (uint8_t)(y1 + OY);
    }
    int px = (x0 + x1) / 2, py = mid + 1 <= y1 ? mid + 1 : y1;
    if (sign == SIGN_PAINT)
        set(px, py, T_PAD);
    return spot_out(px, py);
}

/* Rushtown Square: paved, a ring of trees, a fountain in each half */
static void square(int x0, int y0, int x1, int y1)
{
    fill(x0, y0, x1, y1, T_PAVE);
    for (int x = x0; x <= x1; x += 2) {
        set(x, y0, T_TREE);
        set(x, y1, T_TREE);
    }
    for (int y = y0 + 2; y < y1; y += 2) {
        set(x0, y, T_TREE);
        set(x1, y, T_TREE);
    }
    int cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
    set(cx, cy, T_FOUNT);
    if (x1 - x0 >= 6 && y1 - y0 >= 6) {
        fill(cx - 2, cy - 2, cx + 2, cy + 2, T_FLOWER);
        fill(cx - 1, cy - 1, cx + 1, cy + 1, T_PAVE);
        set(cx, cy, T_FOUNT);
    }
    /* ways in through the trees */
    set(cx, y0, T_PAVE);
    set(cx, y1, T_PAVE);
    set(x0, cy, T_PAVE);
    set(x1, cy, T_PAVE);
    w_square = spot_out(cx, y1);
}

/* The harbour: a basin with a quay on three sides and the sea on the
 * fourth, where the ring road crosses on a bridge. Ships at a pier. */
static void harbour(int x0, int y0, int x1, int y1)
{
    int ring = vx[nvx - 1], ym = (y0 + y1) / 2;
    fill(x0, y0, x1, y1, T_PAVE);
    w_harbour = spot_out(x0, ym);
    if (x1 - x0 < 3 || y1 - y0 < 3)
        return;
    for (int y = y0 + 1; y <= y1 - 1; y++) {
        for (int x = x0 + 1; x < IW; x++)
            set(x, y, T_WATER);
        set(ring - 1, y, T_BS_L);
        set(ring, y, T_BV_L);
        set(ring + 1, y, T_BV_R);
        set(ring + 2, y, T_BS_R);
    }
    if (y1 - y0 >= 6) {                         /* a pier down the middle, a ship on each side */
        fill(x0 + 1, ym, x1 - 1, ym, T_PATH);
        building(x0 + 2, ym - 2, x1 - 1 < x0 + 6 ? x1 - 1 : x0 + 6, ym - 1, B_SHIP, 0, SIGN_NONE);
        building(x0 + 2, ym + 1, x1 - 1 < x0 + 5 ? x1 - 1 : x0 + 5, ym + 2 <= y1 - 1 ? ym + 2 : ym + 1, B_SHIP, 1, SIGN_NONE);
    } else
        building(x0 + 2, y0 + 1, x1 - 1 < x0 + 5 ? x1 - 1 : x0 + 5, y0 + 1, B_SHIP, 0, SIGN_NONE);
}

enum { DOWNTOWN, HILLS, EAST, PARKSIDE, MIDTOWN, DOCKS };
static int district_of(int tx, int ty)
{
    int third = tx < 34 ? 0 : tx < 62 ? 1 : 2;
    bool north = ty < main_river_y + 1;
    return north ? (third == 0 ? HILLS : third == 1 ? DOWNTOWN : EAST)
                 : (third == 0 ? PARKSIDE : third == 1 ? MIDTOWN : DOCKS);
}

int world_isle(float x, float y)
{
    int tx = (int)x >> 4, ty = (int)y >> 4;
    for (int k = 0; k < ISLES; k++)
        if (tx >= isles[k].ox && tx < isles[k].ox + isles[k].w && ty >= isles[k].oy && ty < isles[k].oy + isles[k].h)
            return k;
    return -1;
}

const char *world_district(float x, float y)
{
    static const char *const names[] = { "DOWNTOWN", "SUNSET HILLS", "EASTSIDE", "PARKSIDE", "MIDTOWN", "THE DOCKS" };
    int tx = (int)x >> 4, ty = (int)y >> 4, k = world_isle(x, y), t = tile_at(tx, ty);
    if (t == T_BH_T || t == T_BH_B || t == T_BF_T || t == T_BF_B)
        return "THE LONG BRIDGE";
    if (k < 0)
        return "OPEN WATER";
    if (k == IRON)
        return "IRON ISLE";
    if (k == PALM)
        return "PALM KEY";
    if (tx < CITY0 - 1 || ty < CITY0 - 1 || tx > isles[0].w - 8 || ty > isles[0].h - 8)
        return "THE BEACH";
    return names[district_of(tx, ty)];
}

static void blocks(void)
{
    bool hospital = false, police = false, paint0 = false, paint1 = false;
    bool garage = false, ammo = false, tower = false;
    for (int j = 0; j < nhy - 1; j++)
        for (int i = 0; i < nvx - 1; i++) {
            int x0 = vx[i] + 3, x1 = vx[i + 1] - 2, y0 = bot(j) + 2, y1 = top(j + 1) - 2;
            if (j == river_j) {                 /* the banks */
                for (int y = y0; y <= y1; y++)
                    for (int x = x0; x <= x1; x++)
                        if (y < river_y || y >= river_y + RIVER_H)
                            M(x, y) = (uint8_t)(wn(100) < 14 ? T_TREE : wn(100) < 20 ? T_FLOWER : T_GRASS);
                if (river_y - 1 >= y0)
                    fill(x0, river_y - 1, x1, river_y - 1, T_PATH);
                if (river_y + RIVER_H <= y1)
                    fill(x0, river_y + RIVER_H, x1, river_y + RIVER_H, T_PATH);
                continue;
            }
            if (j == sq_j && (i == sq_i || i == sq_i + 1)) {
                if (!seg_v[sq_i + 1][sq_j]) {   /* both halves and the strip between them as one */
                    if (i == sq_i)
                        square(x0, y0, vx[i + 2] - 2, y1);
                } else if (i == sq_i)
                    square(x0, y0, x1, y1);
                else
                    offices(x0, y0, x1, y1, 0);
                continue;
            }
            if (j == hb_j && i == nvx - 2) {
                harbour(x0, y0, x1, y1);
                continue;
            }
            int d = district_of((x0 + x1) / 2, (y0 + y1) / 2), r = wn(100);
            int w = x1 - x0 + 1, h = y1 - y0 + 1;
            bool roomy = w >= 5 && h >= 5;
            if (kind == IRON) {                 /* works, sheds, stacks of boxes */
                if (!hospital && roomy) {
                    w_works = special(x0, y0, x1, y1, SIGN_WORKS);
                    hospital = true;
                } else if (r < 40) halls(x0, y0, x1, y1);
                else if (r < 68) containers(x0, y0, x1, y1);
                else if (r < 84) parking(x0, y0, x1, y1);
                else offices(x0, y0, x1, y1, 0);
                continue;
            }
            if (kind == PALM) {                 /* houses, gardens, a bar */
                if (!hospital && roomy) {
                    w_bar = special(x0, y0, x1, y1, SIGN_BAR);
                    hospital = true;
                } else if (r < 58) houses(x0, y0, x1, y1);
                else if (r < 84) park(x0, y0, x1, y1);
                else plaza(x0, y0, x1, y1);
                continue;
            }
            if (d == DOWNTOWN && !hospital && roomy) {
                w_hospital = special(x0, y0, x1, y1, SIGN_H);
                hospital = true;
            } else if (d == EAST && !police && roomy) {
                w_police = special(x0, y0, x1, y1, SIGN_POLICE);
                police = true;
            } else if (d == HILLS && !paint0 && roomy && j >= 1) {
                w_paint[0] = special(x0, y0, x1, y1, SIGN_PAINT);
                paint0 = true;
            } else if (d == DOCKS && !paint1 && roomy && i <= nvx - 3) {
                w_paint[1] = special(x0, y0, x1, y1, SIGN_PAINT);
                paint1 = true;
            } else if (d == DOWNTOWN && !tower && roomy) {
                w_tower = special(x0, y0, x1, y1, SIGN_TOWER);
                tower = true;
            } else if (d == PARKSIDE && !garage && roomy) {
                w_garage = special(x0, y0, x1, y1, SIGN_GARAGE);
                garage = true;
            } else if (d == MIDTOWN && !ammo && roomy) {
                w_ammo = special(x0, y0, x1, y1, SIGN_AMMO);
                ammo = true;
            } else if (d == DOWNTOWN || d == MIDTOWN) {
                if (r < 66) offices(x0, y0, x1, y1, 0);
                else if (r < 80) plaza(x0, y0, x1, y1);
                else if (r < 92) parking(x0, y0, x1, y1);
                else park(x0, y0, x1, y1);
            } else if (d == HILLS) {
                if (r < 74) houses(x0, y0, x1, y1);
                else if (r < 90) park(x0, y0, x1, y1);
                else parking(x0, y0, x1, y1);
            } else if (d == EAST) {
                if (r < 45) offices(x0, y0, x1, y1, 0);
                else if (r < 75) houses(x0, y0, x1, y1);
                else if (r < 88) parking(x0, y0, x1, y1);
                else plaza(x0, y0, x1, y1);
            } else if (d == PARKSIDE) {
                if (r < 50) park(x0, y0, x1, y1);
                else houses(x0, y0, x1, y1);
            } else {
                if (r < 50) halls(x0, y0, x1, y1);
                else if (r < 78) containers(x0, y0, x1, y1);
                else parking(x0, y0, x1, y1);
            }
        }
}

/* ---- the rest ---------------------------------------------------------------- */

static void palms(void)
{
    for (int y = 0; y < IH; y++)
        for (int x = 0; x < IW; x++)
            if (M(x, y) == T_SAND && wn(100) < (kind == PALM ? 9 : 4) &&
                land(x - 1, y) && land(x + 1, y) && land(x, y - 1) && land(x, y + 1))
                M(x, y) = T_PALM;
}

/* kerbs: which sides of a footway tile have road; over the whole world, last */
static void kerbs(void)
{
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++)
            if (map[y][x] >= T_SW) {
                int m = 0;
                if (t_road(tile_at(x, y - 1)) || tile_at(x, y - 1) == T_BAY) m |= 1;
                if (t_road(tile_at(x + 1, y)) || tile_at(x + 1, y) == T_BAY) m |= 2;
                if (t_road(tile_at(x, y + 1)) || tile_at(x, y + 1) == T_BAY) m |= 4;
                if (t_road(tile_at(x - 1, y)) || tile_at(x - 1, y) == T_BAY) m |= 8;
                map[y][x] = (uint8_t)(T_SW + m);
            }
}

static spot_t corner(int i, int j, int dx, int dy)   /* the footway corner of a crossing */
{
    return spot_out(vx[i] + (dx < 0 ? -1 : 2), dy < 0 ? top(j) - 1 : bot(j) + 1);
}

/* phones at crossings' corners, chips tucked away off the roads */
static void spots(int phone0, int phones, int token0, int tokens)
{
    static const uint8_t at[8][2] = { {1, 1}, {4, 1}, {6, 2}, {2, 3}, {5, 4}, {1, 5}, {3, 6}, {6, 6} };
    for (int p = 0; p < phones; p++) {
        int i = at[p][0] * (nvx - 1) / 7, j = at[p][1] * (nhy - 1) / 7;
        if (kind != MAIN) {
            i = p ? nvx - 2 : 1;
            j = p ? nhy - 2 : 1;
        }
        w_phone[phone0 + p] = corner(i, j, p & 1 ? 1 : -1, p & 2 ? 1 : -1);
    }
    if (kind == MAIN)
        w_start = corner(nvx / 2, nhy / 3, 1, 1);
    int n = 0;
    for (int tries = 0; n < tokens && tries < 20000; tries++) {
        int x = wrange(4, IW - 5), y = wrange(4, IH - 5), t = M(x, y);
        if (t != T_GRASS && t != T_FLOWER && t != T_SAND && t != T_PAVE && t != T_PATH)
            continue;
        spot_t s = spot_out(x, y);
        bool far = true;
        for (int k = 0; k < n; k++)
            if (abs(w_token[token0 + k].x - s.x) + abs(w_token[token0 + k].y - s.y) < (kind == MAIN ? 14 : 10) * TILE)
                far = false;
        if (far)
            w_token[token0 + n++] = s;
    }
    for (; n < tokens; n++)
        w_token[token0 + n] = w_start;
}

/* one island: its place, then everything on it */
static void island(int k, int ox, int oy, int w, int h, int phone0, int phones, int token0, int tokens)
{
    isle_t *I = &isles[k];
    kind = k;
    IW = w;
    IH = h;
    base = nnodes;
    nvx = lines(vx, w - 10);
    nhy = lines(hy, h - 10);
#if RT_ISLANDS
    /* Iron Isle lies so that one of its rows meets the row of the main
     * island the bridge leaves from; Palm Key so that the piers face */
    if (k == IRON)
        oy = isles[MAIN].hy[1 == main_wide_j ? 2 : 1] - hy[1];
    if (k == PALM)
        ox = isles[MAIN].vx[isles[MAIN].nvx / 3] - vx[nvx / 2];
    if (ox < 0) ox = 0;
    if (oy < 0) oy = 0;
#endif
    OX = ox;
    OY = oy;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            if (land(x, y))
                M(x, y) = T_SAND;
    graph();
    roads();
    blocks();
    river();
    palms();
    spots(phone0, phones, token0, tokens);
    I->ox = ox; I->oy = oy; I->w = w; I->h = h;
    I->nvx = nvx; I->nhy = nhy; I->base = base;
    memcpy(I->vx, vx, sizeof vx);
    memcpy(I->hy, hy, sizeof hy);

}

#if RT_ISLANDS
static int bridge_a, bridge_b, bridge_x0, bridge_x1, bridge_y;

/* the long bridge is there from the start, but shut until the story opens it */
void world_open_bridge(bool open)
{
    nodes[bridge_a].e[D_E] = open ? (uint8_t)bridge_b : NO_NODE;
    nodes[bridge_b].e[D_W] = open ? (uint8_t)bridge_a : NO_NODE;
    for (int side = 0; side < 2; side++) {
        int x = side ? bridge_x1 - 3 : bridge_x0 + 3;
        map[bridge_y - 1][x] = (uint8_t)(open ? T_BF_T : T_BARRIER);
        map[bridge_y][x] = (uint8_t)(open ? T_BH_T : T_BARRIER);
        map[bridge_y + 1][x] = (uint8_t)(open ? T_BH_B : T_BARRIER);
        map[bridge_y + 2][x] = (uint8_t)(open ? T_BF_B : T_BARRIER);
    }
}

static void links(void)
{
    const isle_t *m = &isles[MAIN], *ir = &isles[IRON], *pk = &isles[PALM];
    /* the bridge: from the east end of a row of the main island to the west end of Iron Isle's */
    int jb = 1 == main_wide_j ? 2 : 1;
    bridge_y = m->hy[jb];
    bridge_x0 = m->vx[m->nvx - 1] + 2;
    bridge_x1 = ir->ox + ir->vx[0] - 1;
    bridge_a = m->base + jb * m->nvx + m->nvx - 1;
    bridge_b = ir->base + 1 * ir->nvx;
    for (int x = bridge_x0; x <= bridge_x1; x++) {
        bool end = x == bridge_x0 || x == bridge_x1;
        map[bridge_y][x] = (uint8_t)(end ? T_ZH : T_BH_T);
        map[bridge_y + 1][x] = (uint8_t)(end ? T_ZH : T_BH_B);
        if (!end) {
            map[bridge_y - 1][x] = T_BF_T;
            map[bridge_y + 2][x] = T_BF_B;
        }
    }
    w_bridge = (spot_t){ (int16_t)((bridge_x0 + 1) * TILE), (int16_t)(bridge_y * TILE + TILE) };
    world_open_bridge(false);

    /* the ferry: a pier south from the main island's ring, one north from Palm Key's */
    int xm = m->vx[m->nvx / 3], ym = m->hy[m->nhy - 1] + 2;
    for (int y = ym; y < m->h + 1; y++)
        for (int x = xm; x <= xm + 1; x++)
            map[y][x] = (uint8_t)(map[y][x] == T_WATER ? T_PATH : T_PAVE);
    w_ferry[0] = (spot_t){ (int16_t)((xm + 1) * TILE), (int16_t)((m->h - 1) * TILE + 8) };
    int xp = pk->ox + pk->vx[pk->nvx / 2], yp = pk->oy + pk->hy[0] - 2;
    for (int y = pk->oy - 1; y <= yp; y++)
        for (int x = xp; x <= xp + 1; x++)
            map[y][x] = (uint8_t)(map[y][x] == T_WATER ? T_PATH : T_PAVE);
    w_ferry[1] = (spot_t){ (int16_t)((xp + 1) * TILE), (int16_t)(pk->oy * TILE + 8) };
    /* the boat, moored beside the main island's pier */
    OX = OY = 0;
    IW = MAP_W;
    IH = MAP_H;
    building(xm + 3, m->h - 2, xm + 8, m->h - 1, B_SHIP, 1, SIGN_NONE);
}
#else
void world_open_bridge(bool open) { (void)open; }
#endif

void world_init(void)
{
    ws = 0x52757368u;                           /* the town's number */
    nbuildings = 0;
    nnodes = 0;
    memset(map, T_WATER, sizeof map);
    island(MAIN, 0, 0, 96, 96, 0, 8, 0, 24);
#if RT_ISLANDS
    island(IRON, 110, 0, 48, 56, 8, 2, 24, 4);
    island(PALM, 0, 104, 64, 44, 10, 2, 28, 4);
    links();
#endif
    kerbs();
}

int world_node_near(float x, float y)
{
    int best = 0;
    float bd = 1e12f;
    for (int n = 0; n < nnodes; n++) {
        float dx = (float)nodes[n].x - x, dy = (float)nodes[n].y - y, d = dx * dx + dy * dy;
        if (d < bd) {
            bd = d;
            best = n;
        }
    }
    return best;
}

bool world_sight(float x0, float y0, float x1, float y1)
{
    float dx = x1 - x0, dy = y1 - y0;
    float len = dx > 0 ? dx : -dx, ly = dy > 0 ? dy : -dy;
    if (ly > len)
        len = ly;
    int steps = (int)(len / 8.0f) + 1;
    for (int i = 1; i < steps; i++) {
        float t = (float)i / (float)steps;
        if (t_solid(tile_px(x0 + dx * t, y0 + dy * t)))
            return false;
    }
    return true;
}
