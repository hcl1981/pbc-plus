/* draw.c -- one frame: ground, shadows, people and cars, roofs, tree tops,
 * smoke, and on top of it all the display of money, stars and the job. */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "game.h"

#define YELLOW RGB(255, 220, 64)
#define WHITE  RGB(255, 255, 255)

static uint32_t hash(int x, int y)
{
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static px_t shade(uint32_t rgb, int d)
{
    int r = (int)((rgb >> 16) & 255) + d, g = (int)((rgb >> 8) & 255) + d, b = (int)(rgb & 255) + d;
    r = r < 0 ? 0 : r > 255 ? 255 : r;
    g = g < 0 ? 0 : g > 255 ? 255 : g;
    b = b < 0 ? 0 : b > 255 ? 255 : b;
    return RGB(r, g, b);
}

/* ---- the ground ------------------------------------------------------------ */

static void ground(void)
{
    int tx0 = G.cam_x >> 4, ty0 = G.cam_y >> 4, ox = -(G.cam_x & 15), oy = -(G.cam_y & 15);
    int wave = G.frame / 14;
    for (int j = 0; j <= SCR_H / TILE + 1; j++)
        for (int i = 0; i <= SCR_W / TILE; i++) {
            int tx = tx0 + i, ty = ty0 + j, t = tile_at(tx, ty);
            if (t == T_BUILD)                   /* a roof goes over it anyway */
                continue;
            const px_t *src = t == T_WATER ? art_water[(tx * 3 + ty * 5 + wave) & 3] : art_tile[t];
            g_tile(src, ox + i * TILE, oy + j * TILE);
        }
}

static void marker(float wx, float wy, px_t col)
{
    int x = (int)wx - G.cam_x, y = (int)wy - G.cam_y, r = 9 + ((G.frame >> 2) & 3);
    if (x < -20 || y < -20 || x > SCR_W + 20 || y > SCR_H + 20)
        return;
    g_ring(x, y, r, col);
    g_ring(x, y, r - 1, col);
    g_ring(x, y, 4, WHITE);
}

static void ground_things(void)
{
    for (int p = 0; p < PHONES; p++) {          /* a phone box that rings when there is work */
        int x = w_phone[p].x - G.cam_x, y = w_phone[p].y - G.cam_y;
        if (x < -16 || y < -16 || x > SCR_W + 16 || y > SCR_H + 16)
            continue;
        g_rect(x - 4, y - 4, 9, 9, RGB(20, 30, 60));
        g_rect(x - 3, y - 3, 7, 7, RGB(60, 110, 220));
        g_rect(x - 1, y - 2, 3, 5, RGB(210, 225, 250));
        if (G.job == J_NONE && G.state == ST_PLAY) {
            marker((float)w_phone[p].x, (float)w_phone[p].y, YELLOW);
            if ((G.frame >> 3) & 1)
                g_text_c(x + 1, y - 20, "!", YELLOW, 1);
        }
    }
    for (int i = 0; i < TOKENS; i++) {          /* a chip, turning */
        if (G.tokens & (1u << i))
            continue;
        int x = w_token[i].x - G.cam_x, y = w_token[i].y - G.cam_y;
        if (x < -8 || y < -8 || x > SCR_W + 8 || y > SCR_H + 8)
            continue;
        int w = (int)(fabsf(cosf((float)G.frame * 0.09f)) * 4.0f);
        g_rect(x - w - 1, y - 4, 2 * w + 3, 9, RGB(90, 60, 0));
        g_rect(x - w, y - 3, 2 * w + 1, 7, YELLOW);
        if (w > 1)
            g_rect(x - 1, y - 2, 2, 5, RGB(255, 250, 200));
    }
    for (int i = 0; i < TARGETS; i++)
        if (G.tgt[i].on)
            marker(G.tgt[i].x, G.tgt[i].y, G.job == J_HOTCAR && G.stage == 0 ? RGB(255, 120, 200) : RGB(90, 255, 120));
#if RT_ISLANDS
    for (int k = 0; k < 2; k++)                 /* the ferry's two piers */
        if (G.state == ST_PLAY)
            marker((float)w_ferry[k].x, (float)w_ferry[k].y, (G.owns & OWN_FERRY) ? RGB(90, 220, 255) : RGB(110, 110, 120));
#endif
    /* what is for sale: a dollar sign to stand on */
    const spot_t shop[3] = { w_garage, w_tower, w_ammo };
    for (int k = 0; k < 3; k++) {
        if ((k == 0 && (G.owns & OWN_GARAGE)) || (k == 1 && (G.owns & OWN_TOWER)) || G.state != ST_PLAY)
            continue;
        int x = shop[k].x - G.cam_x, y = shop[k].y - G.cam_y;
        if (x < -16 || y < -16 || x > SCR_W + 16 || y > SCR_H + 16)
            continue;
        g_disc(x, y, 7, RGB(20, 60, 30));
        g_disc(x, y, 6, RGB(60, 190, 90));
        g_text(x - 3, y - 4, "$", WHITE, 1);
    }
}

/* a lamp at each way into a crossing that has lights */
static void lights(void)
{
    static const int8_t at[4][2] = { {19, 21}, {-21, 19}, {-19, -21}, {21, -19} };  /* for traffic going N, E, S, W */
    static const px_t lamp[3] = { RGB(60, 230, 90), RGB(250, 210, 50), RGB(240, 50, 50) };
    for (int n = 0; n < nnodes; n++) {
        int x = nodes[n].x - G.cam_x, y = nodes[n].y - G.cam_y;
        if (x < -30 || y < -30 || x > SCR_W + 30 || y > SCR_H + 30 || !game_lit(n))
            continue;
        for (int d = 0; d < 4; d++) {
            if (nodes[n].e[(d + 2) & 3] == NO_NODE)
                continue;                       /* nobody comes from there */
            int lx = x + at[d][0], ly = y + at[d][1];
            g_rect(lx - 3, ly - 3, 6, 6, RGB(16, 16, 20));
            g_rect(lx - 2, ly - 2, 4, 4, lamp[game_light(n, d)]);
        }
    }
}

/* ---- buildings ------------------------------------------------------------- */

static const uint32_t ROOF_OFFICE[8] = { 0x7c8088, 0x8a8478, 0x6c7684, 0x9a9690, 0x70787a, 0x86807c, 0x5f6670, 0x94908a };
static const uint32_t ROOF_HOUSE[8]  = { 0xa8483c, 0x8a5a3a, 0x9c4440, 0x6a7a8c, 0xb06a40, 0x70543c, 0x4c7a5c, 0x944c5c };
static const uint32_t ROOF_BOX[8]    = { 0xc0392b, 0x2e6fb0, 0x2f8a4a, 0xd98a20, 0x7a4ab0, 0xb8b8b0, 0x1f8a8a, 0xc8a020 };

static bool building_rect(const building_t *b, int *x, int *y, int *w, int *h)
{
    *x = b->x * TILE - G.cam_x;
    *y = b->y * TILE - G.cam_y;
    *w = b->w * TILE;
    *h = b->h * TILE;
    return *x < SCR_W + 8 && *y < SCR_H + 8 && *x + *w > -8 && *y + *h > -8;
}

static void building_shadows(void)
{
    for (int i = 0; i < nbuildings; i++) {
        int x, y, w, h;
        const building_t *b = &buildings[i];
        if (!building_rect(b, &x, &y, &w, &h))
            continue;
        int s = b->style == B_BOX || b->style == B_HOUSE ? 3 : 6;
        g_dark(x + w, y + s, s, h);
        g_dark(x + s, y + h, w - s, s);
    }
}

static void roofs(void)
{
    for (int i = 0; i < nbuildings; i++) {
        int x, y, w, h;
        const building_t *b = &buildings[i];
        if (!building_rect(b, &x, &y, &w, &h))
            continue;
        uint32_t base = b->style == B_HOUSE ? ROOF_HOUSE[b->colour & 7]
                      : b->style == B_BOX ? ROOF_BOX[b->colour & 7]
                      : b->style == B_GLASS ? 0x4a7a9c
                      : b->style == B_SHOP ? (b->sign == SIGN_AMMO ? 0x6a7a4a : 0xb85aa0)
                      : b->style == B_SHIP ? (b->colour ? 0x2a4a7a : 0x8a2a2a)
                      : b->style == B_HALL ? 0x8c9298 : ROOF_OFFICE[b->colour & 7];
        g_rect(x, y, w, h, shade(base, 0));
        if (b->style == B_SHIP) {               /* a hull with a pointed bow, a deck, a bridge aft */
            for (int k = 0; k < h / 2; k++) {
                int cut = (h / 2 - k) * 2;
                g_rect(x + w - cut, y + k, cut, 1, HEX(0x2c6cae));
                g_rect(x + w - cut, y + h - 1 - k, cut, 1, HEX(0x2c6cae));
            }
            g_rect(x + 3, y + 3, w - h - 4, h - 6, RGB(200, 196, 184));
            g_rect(x + 5, y + 5, 14, h - 10, WHITE);
            g_rect(x + 7, y + 7, 10, h - 14, RGB(60, 80, 100));
            for (int k = 26; k < w - h - 6; k += 12)
                g_rect(x + k, y + 6, 8, h - 12, ROOF_BOX[(k / 12 + b->colour) & 7] >> 0 ? HEX(ROOF_BOX[(k / 12 + b->colour) & 7]) : WHITE);
            continue;
        }
        switch (b->style) {
        case B_HOUSE:                           /* a ridge, one side in the light */
            if (w >= h) {
                g_rect(x, y, w, h / 2, shade(base, 22));
                g_rect(x, y + h / 2 - 1, w, 2, shade(base, -40));
            } else {
                g_rect(x, y, w / 2, h, shade(base, 22));
                g_rect(x + w / 2 - 1, y, 2, h, shade(base, -40));
            }
            for (int k = 4; k < (w >= h ? w : h) - 2; k += 4) {
                if (w >= h)
                    g_rect(x + k, y + 1, 1, h - 2, shade(base, -14));
                else
                    g_rect(x + 1, y + k, w - 2, 1, shade(base, -14));
            }
            g_rect(x + 5 + (int)(hash(b->x, b->y) % 9), y + 5, 4, 4, RGB(70, 60, 56));
            break;
        case B_GLASS:
            for (int k = 8; k < w; k += 8)
                g_rect(x + k, y + 3, 1, h - 6, shade(base, 40));
            for (int k = 8; k < h; k += 8)
                g_rect(x + 3, y + k, w - 6, 1, shade(base, 40));
            g_rect(x + 3, y + 3, w - 6, 2, shade(base, 60));
            break;
        case B_HALL:
            for (int k = 3; k < w; k += 5)
                g_rect(x + k, y + 2, 1, h - 4, shade(base, -22));
            g_rect(x + 2, y + h / 2, w - 4, 2, shade(base, 26));
            break;
        case B_BOX:
            for (int k = 2; k < w - 1; k += 3)
                g_rect(x + k, y + 2, 1, h - 4, shade(base, -30));
            break;
        case B_SHOP:
            for (int k = 0; k < w; k += 8)
                g_rect(x + k, y + h - 6, 4, 6, RGB(250, 240, 230));
            break;
        default: {                              /* a wall around the edge, boxes of the air conditioning */
            g_rect(x + 3, y + 3, w - 6, h - 6, shade(base, -16));
            uint32_t hsh = hash(b->x, b->y);
            for (int k = 0; k < 3; k++, hsh >>= 7) {
                int ax = x + 6 + (int)(hsh % (uint32_t)(w > 22 ? w - 22 : 1));
                int ay = y + 6 + (int)((hsh >> 3) % (uint32_t)(h > 20 ? h - 20 : 1));
                if (w < 30 || h < 30)
                    break;
                g_rect(ax, ay, 9, 7, RGB(60, 64, 70));
                g_rect(ax + 1, ay + 1, 7, 5, RGB(170, 176, 184));
                g_rect(ax + 3, ay + 2, 3, 3, RGB(90, 96, 104));
            }
            break;
        }
        }
        /* light from the upper left */
        g_rect(x, y, w, 1, shade(base, 50));
        g_rect(x, y, 1, h, shade(base, 50));
        g_rect(x, y + h - 1, w, 1, shade(base, -60));
        g_rect(x + w - 1, y, 1, h, shade(base, -60));
        int cx = x + w / 2, cy = y + h / 2;
        if (b->sign == SIGN_H) {
            g_rect(cx - 12, cy - 12, 24, 24, WHITE);
            g_rect(cx - 3, cy - 9, 6, 18, RGB(220, 40, 40));
            g_rect(cx - 9, cy - 3, 18, 6, RGB(220, 40, 40));
        } else if (b->sign == SIGN_POLICE) {
            g_rect(cx - 26, cy - 7, 52, 14, RGB(30, 50, 140));
            g_text(cx - 24, cy - 4, "POLICE", WHITE, 1);
        } else if (b->sign == SIGN_PAINT) {
            g_rect(cx - 22, cy - 7, 44, 14, RGB(30, 30, 36));
            g_text(cx - 20, cy - 4, "PAINT", YELLOW, 1);
        } else if (b->sign == SIGN_GARAGE) {
            g_rect(cx - 26, cy - 7, 52, 14, RGB(30, 30, 36));
            g_text(cx - 24, cy - 4, "GARAGE", (G.owns & OWN_GARAGE) ? RGB(120, 255, 120) : WHITE, 1);
        } else if (b->sign == SIGN_AMMO) {
            g_rect(cx - 18, cy - 7, 36, 14, RGB(30, 30, 36));
            g_text(cx - 16, cy - 4, "GUNS", RGB(255, 140, 60), 1);
        } else if (b->sign == SIGN_WORKS) {
            g_rect(cx - 22, cy - 7, 44, 14, RGB(30, 30, 36));
            g_text(cx - 20, cy - 4, "WORKS", RGB(200, 200, 210), 1);
        } else if (b->sign == SIGN_BAR) {
            g_rect(cx - 30, cy - 7, 60, 14, RGB(40, 20, 50));
            g_text(cx - 28, cy - 4, "LOLA'S", RGB(255, 150, 220), 1);
        } else if (b->sign == SIGN_TOWER) {
            g_rect(cx - 22, cy - 7, 44, 14, RGB(20, 30, 50));
            g_text(cx - 20, cy - 4, "TOWER", (G.owns & OWN_TOWER) ? YELLOW : WHITE, 1);
        }
    }
}

static void trees(bool shadow)
{
    int tx0 = (G.cam_x >> 4) - 1, ty0 = (G.cam_y >> 4) - 1;
    for (int j = 0; j <= SCR_H / TILE + 3; j++)
        for (int i = 0; i <= SCR_W / TILE + 2; i++) {
            int tx = tx0 + i, ty = ty0 + j, t = tile_at(tx, ty);
            if (t != T_TREE && t != T_PALM)
                continue;
            uint32_t h = hash(tx, ty);
            float x = (float)(tx * TILE + 8 - G.cam_x + (int)(h % 5) - 2);
            float y = (float)(ty * TILE + 8 - G.cam_y + (int)((h >> 4) % 5) - 2);
            int k = t == T_PALM;
            if (shadow)
                g_sprite_rot(art_canopy[k], 24, 24, art_canopy_pal[k], x + 4, y + 5, 0, true);
            else
                g_sprite(art_canopy[k], 24, 24, art_canopy_pal[k], (int)x - 12, (int)y - 12);
        }
}

/* ---- who moves ------------------------------------------------------------- */

static void person(float wx, float wy, float a, int look, int anim, bool down, bool dead)
{
    px_t pal[8];
    float x = wx - (float)G.cam_x, y = wy - (float)G.cam_y;
    if (x < -10 || y < -10 || x > SCR_W + 10 || y > SCR_H + 10)
        return;
    art_ped_palette(pal, look);
    if (dead)
        g_disc((int)x, (int)y, 4, RGB(120, 20, 24));
    int f = down ? 0 : (anim >> 3) % 4;
    f = f == 1 ? 1 : f == 3 ? 2 : 0;
    g_sprite_rot(art_ped[f], PED_W, PED_H, pal, x + 1, y + 2, a, true);
    g_sprite_rot(art_ped[f], PED_W, PED_H, pal, x, y, a, false);
}

static void movers(void)
{
    giver_t gv;
    if (G.state != ST_TITLE && G.job == J_NONE && story_giver(&gv)) {   /* who has work, waiting */
        int x = (int)gv.x - G.cam_x, y = (int)gv.y - G.cam_y;
        if (x > -30 && y > -30 && x < SCR_W + 30 && y < SCR_H + 30) {
            marker(gv.x, gv.y, RGB(90, 220, 255));
            person(gv.x, gv.y, PI, gv.look, 0, false, false);
            if ((G.frame >> 3) & 1)
                g_text_c(x + 1, y - 22, "?", RGB(90, 220, 255), 1);
        }
    }
    for (int i = 0; i < PEDS; i++)
        if (peds[i].used)
            person(peds[i].x, peds[i].y, peds[i].a, peds[i].look, peds[i].anim,
                   peds[i].state == P_DOWN || peds[i].state == P_DEAD, peds[i].state == P_DEAD);
    if (G.state != ST_TITLE && pl.car < 0 && !(pl.inv & 4) && G.ev != EV_WET && G.ev != EV_RIDE)
        person(pl.x, pl.y, pl.a, LOOK_PLAYER, pl.anim, G.ev == EV_KO, false);
    for (int i = 0; i < CARS; i++) {
        const car_t *c = &cars[i];
        if (!c->used)
            continue;
        float x = c->x - (float)G.cam_x, y = c->y - (float)G.cam_y;
        if (x < -20 || y < -20 || x > SCR_W + 20 || y > SCR_H + 20)
            continue;
        px_t pal[16];
        const car_model_t *m = &art_car[c->model];
        art_car_palette(pal, c->model, c->colour, c->chase || c->svc ? G.frame : 0);
        if (c->hp <= 0)                         /* burnt out */
            for (int k = 2; k <= 4; k++)
                pal[k] = RGB(52 + k * 6, 50 + k * 6, 48 + k * 6);
        uint8_t lv[4] = { DMG_LEVEL(c->dmg[0]), DMG_LEVEL(c->dmg[1]), DMG_LEVEL(c->dmg[2]), DMG_LEVEL(c->dmg[3]) };
        const uint8_t *dents = lv[0] | lv[1] | lv[2] | lv[3] ? lv : NULL;
        g_car(m->pix, m->w, m->h, pal, x + 2, y + 3, c->a, true, dents);
        g_car(m->pix, m->w, m->h, pal, x, y, c->a, false, dents);
    }
}

static void effects(void)
{
    for (int i = 0; i < SHOTS; i++)
        if (shots[i].used) {
            int x = (int)shots[i].x - G.cam_x, y = (int)shots[i].y - G.cam_y;
            g_line(x, y, x - (int)(shots[i].vx * 0.016f), y - (int)(shots[i].vy * 0.016f), RGB(255, 240, 160));
        }
    for (int i = 0; i < FXS; i++) {
        const fx_t *f = &fxs[i];
        if (!f->used)
            continue;
        int x = (int)f->x - G.cam_x, y = (int)f->y - G.cam_y;
        switch (f->kind) {
        case FX_SMOKE: {
            int r = 1 + (34 - f->life) / 9;
            if (r < 1) r = 1;
            g_blend(x - r, y - r, 2 * r + 1, 2 * r + 1, RGB(200, 200, 200));
            break;
        }
        case FX_SPARK:
            g_rect(x, y, 2, 2, f->life > 12 ? WHITE : YELLOW);
            break;
        case FX_DUST:
            g_blend(x - 2, y - 2, 4, 4, RGB(200, 180, 130));
            break;
        default:
            g_rect(x, y, 2, 2, f->life > 14 ? WHITE : RGB(140, 190, 240));
            break;
        }
    }
}

/* ---- the display on top ---------------------------------------------------- */

static void star(int x, int y, px_t c)
{
    static const uint8_t rows[9] = { 0x10, 0x10, 0x38, 0xfe, 0x7c, 0x38, 0x7c, 0x6c, 0x44 };
    for (int j = 0; j < 9; j++)
        for (int i = 0; i < 8; i++)
            if (rows[j] & (0x80 >> i))
                g_pix(x + i, y + j, c);
}

static void heart(int x, int y, px_t c)
{
    static const uint8_t rows[7] = { 0x6c, 0xfe, 0xfe, 0x7c, 0x38, 0x10, 0x00 };
    for (int j = 0; j < 7; j++)
        for (int i = 0; i < 8; i++)
            if (rows[j] & (0x80 >> i))
                g_pix(x + i, y + j, c);
}

/* an arrow at the edge of the screen towards something that is not on it */
static void pointer(float wx, float wy, px_t col)
{
    float dx = wx - (float)(G.cam_x + SCR_W / 2), dy = wy - (float)(G.cam_y + SCR_H / 2);
    if (fabsf(dx) < SCR_W / 2 - 8 && fabsf(dy) < SCR_H / 2 - 20)
        return;
    float a = atan2f(dy, dx), s = sinf(a), c = cosf(a);
    float k = fabsf(c) * (SCR_H / 2 - 30) > fabsf(s) * (SCR_W / 2 - 14)
            ? (SCR_W / 2 - 14) / fabsf(c) : (SCR_H / 2 - 30) / fabsf(s);
    int x = SCR_W / 2 + (int)(c * k), y = SCR_H / 2 + (int)(s * k);
    /* an arrow with a shaft, so that its point is not in doubt: f runs
     * back from the point, l across; black around it */
    int mx = x - (int)(c * 9), my = y - (int)(s * 9);   /* its middle: only that much is looked at */
    for (int j = my - 11; j <= my + 11; j++)
        for (int i = mx - 11; i <= mx + 11; i++) {
            float di = (float)(i - x), dj = (float)(j - y);
            float f = -(di * c + dj * s), l = fabsf(-di * s + dj * c);
            bool in = (f >= 0 && f <= 9 && l <= f * 0.8f) || (f > 9 && f <= 17 && l <= 2.2f);
            bool rim = (f >= -1.5f && f <= 10.5f && l <= (f + 1.5f) * 0.8f + 1.0f) ||
                       (f > 9 && f <= 18.5f && l <= 3.7f);
            if (in)
                g_pix(i, j, col);
            else if (rim)
                g_pix(i, j, RGB(0, 0, 0));
        }
}

static void hud(void)
{
    char s[40];
    g_dark(0, 0, SCR_W, 15);
    snprintf(s, sizeof s, "$%d", G.cash);
    g_text_s(4, 3, s, YELLOW, 1);
    for (int i = 0; i < 4; i++) {
        bool on = i < G.stars;
        if (on && G.stars && (G.frame & 16) && G.unseen > UNSEEN)
            on = false;                         /* they have lost sight: the stars blink */
        star(SCR_W - 12 - i * 11, 3, on ? YELLOW : RGB(70, 70, 80));
    }
    {                                           /* the lives: a little figure and how many */
        px_t pal[8];
        art_ped_palette(pal, LOOK_PLAYER);
        g_sprite(art_ped[0], PED_W, PED_H, pal, 104, 5);
        snprintf(s, sizeof s, "x%d", G.lives);
        g_text_s(114, 3, s, G.lives > 1 ? WHITE : RGB(255, 96, 64), 1);
    }
    if (pl.car >= 0) {
        int hp = cars[pl.car].hp;
        g_dark(4, SCR_H - 13, 62, 9);
        g_rect(5, SCR_H - 12, 60 * hp / 100, 7, hp > 50 ? RGB(90, 220, 110) : hp > 25 ? YELLOW : RGB(240, 70, 60));
        g_frame(4, SCR_H - 13, 62, 9, RGB(20, 20, 24));
    } else
        for (int i = 0; i < 3; i++)
            heart(5 + i * 10, SCR_H - 12, i < pl.hp ? RGB(240, 60, 70) : RGB(60, 60, 70));

    if (G.job_text[0]) {
        g_dark(0, 15, SCR_W, 13);
        g_text_s(4, 18, G.job_text, WHITE, 1);
        if (G.timed) {
            int t = (int)G.job_time;
            snprintf(s, sizeof s, "%d:%02d", t / 60, t % 60);
            g_text_s(SCR_W - 4 - g_text_w(s, 1), 18, s, t < 10 && (G.frame & 16) ? RGB(255, 80, 60) : YELLOW, 1);
        }
    }
    float tx, ty;
    giver_t gv;
    if (game_target(&tx, &ty))
        pointer(tx, ty, RGB(90, 255, 120));
    else if (G.job == J_NONE && story_giver(&gv))
        pointer(gv.x, gv.y, RGB(90, 220, 255));
    else if (G.job == J_NONE) {                 /* the nearest phone */
        int best = 0;
        float bd = 1e12f;
        for (int p = 0; p < PHONES; p++) {
            float dx = (float)w_phone[p].x - pl.x, dy = (float)w_phone[p].y - pl.y;
            if (dx * dx + dy * dy < bd) {
                bd = dx * dx + dy * dy;
                best = p;
            }
        }
        pointer((float)w_phone[best].x, (float)w_phone[best].y, YELLOW);
    }
    if (pl.ammo > 0) {
        snprintf(s, sizeof s, "%d", pl.ammo);
        int w = g_text_w(s, 1);
        g_dark(SCR_W - w - 18, SCR_H - 14, w + 16, 11);
        g_rect(SCR_W - w - 15, SCR_H - 11, 7, 3, RGB(200, 200, 210));
        g_rect(SCR_W - w - 15, SCR_H - 8, 3, 3, RGB(200, 200, 210));
        g_text_s(SCR_W - w - 4, SCR_H - 12, s, WHITE, 1);
    }
    if ((G.job == J_TAIL && G.stage == 1) || (G.job == J_STORY && G.spot > 0)) {    /* how much they have noticed */
        g_dark(SCR_W / 2 - 42, 31, 84, 9);
        g_rect(SCR_W / 2 - 40, 33, (int)(G.spot * 0.8f), 5, G.spot > 60 ? RGB(240, 70, 60) : YELLOW);
        g_frame(SCR_W / 2 - 41, 32, 82, 7, RGB(20, 20, 24));
    }

    /* caught or knocked out: the word, and after a moment the picture of where one wakes up */
    if (G.ev == EV_BUSTED) {
        if (G.ev_t < 80)
            g_picture(PIC_BUSTED, 0, 0);
        g_text_c(SCR_W / 2, G.ev_t < 80 ? 24 : 112, L("BUSTED", "GEFASST"), RGB(90, 140, 255), 3);
    } else if (G.ev == EV_KO) {
        if (G.ev_t < 80)
            g_picture(PIC_KO, 0, 0);
        g_text_c(SCR_W / 2, G.ev_t < 80 ? 24 : 112, L("KNOCKED OUT", "K.O."), RGB(255, 80, 60), 2);
    }
    else if (G.ev == EV_WET)
        g_text_c(SCR_W / 2, 112, L("SPLASH!", "PLATSCH!"), RGB(120, 190, 255), 3);
    else if (G.ev == EV_RIDE || G.ev == EV_FERRY) {
        g_dark(0, 15, SCR_W, SCR_H - 15);
        g_dark(0, 15, SCR_W, SCR_H - 15);
        g_text_c(SCR_W / 2, 120, G.ev == EV_RIDE ? "TAXI" : L("FERRY", "FÄHRE"), YELLOW, 3);
    }
    else if (G.msg_t > 0)
        g_text_c(SCR_W / 2, 96, G.msg, G.msg_col, g_text_w(G.msg, 2) > SCR_W - 12 ? 1 : 2);
    if (G.note_t > 0 && G.ev != EV_BUSTED && G.ev != EV_KO)
        g_text_c(SCR_W / 2, SCR_H - 30, G.note, WHITE, 1);
}

/* ---- the map in the menu --------------------------------------------------- */

static px_t map_colour(int t)
{
    if (t == T_WATER) return RGB(40, 90, 160);
    if (t == T_SAND) return RGB(214, 196, 140);
    if (t == T_GRASS || t == T_FLOWER || t == T_TREE) return RGB(70, 130, 60);
    if (t == T_BUILD) return RGB(120, 110, 104);
    if (t_road(t) || t == T_BAY) return RGB(48, 50, 56);
    if (t == T_BARRIER) return RGB(220, 60, 60);
    if (t == T_LOT || t == T_PAD) return RGB(84, 88, 96);
    return RGB(160, 158, 150);
}

/* the whole world fits the screen at 2 pixels a tile, or at 1 with the islands */
#define MAP_PX (MAP_W > 96 ? 1 : 2)
#define MAP_X0 ((SCR_W - MAP_W * MAP_PX) / 2)
#define MAP_Y0 (MAP_W > 96 ? 30 : 22)

static void dot(float wx, float wy, px_t c, bool big)
{
    int x = MAP_X0 + (int)wx * MAP_PX / TILE, y = MAP_Y0 + (int)wy * MAP_PX / TILE, r = big ? 2 : 1;
    g_rect(x - r - 1, y - r - 1, 2 * r + 3, 2 * r + 3, RGB(0, 0, 0));
    g_rect(x - r, y - r, 2 * r + 1, 2 * r + 1, c);
}

static void pause_menu(void)
{
    char s[40];
    g_dark(0, 0, SCR_W, SCR_H);
    g_dark(0, 0, SCR_W, SCR_H);
    g_frame(MAP_X0 - 2, MAP_Y0 - 2, MAP_W * MAP_PX + 4, MAP_H * MAP_PX + 4, WHITE);
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++)
            g_rect(MAP_X0 + x * MAP_PX, MAP_Y0 + y * MAP_PX, MAP_PX, MAP_PX, map_colour(map[y][x]));
    for (int p = 0; p < PHONES; p++)
        dot((float)w_phone[p].x, (float)w_phone[p].y, RGB(60, 120, 255), false);
    for (int p = 0; p < PAINTS; p++)
        dot((float)w_paint[p].x, (float)w_paint[p].y, RGB(255, 120, 200), false);
    dot((float)w_hospital.x, (float)w_hospital.y, RGB(240, 60, 60), false);
    dot((float)w_garage.x, (float)w_garage.y, (G.owns & OWN_GARAGE) ? RGB(120, 255, 120) : WHITE, false);
    dot((float)w_ammo.x, (float)w_ammo.y, RGB(255, 140, 60), false);
    dot((float)w_tower.x, (float)w_tower.y, YELLOW, false);
    for (int i = 0; i < TARGETS; i++)
        if (G.tgt[i].on && (G.frame & 16))
            dot(G.tgt[i].x, G.tgt[i].y, RGB(90, 255, 120), true);
    if (G.frame & 8)
        dot(pl.car >= 0 ? cars[pl.car].x : pl.x, pl.car >= 0 ? cars[pl.car].y : pl.y, YELLOW, true);

    int chips = 0;
    for (int k = 0; k < TOKENS; k++)
        chips += (G.tokens >> k) & 1;
    if (G.mode == MODE_STORY && story_count())
        snprintf(s, sizeof s, "%s  MISSION %d/%d", rank_name[game_rank()],
                 G.story < story_count() ? G.story + 1 : story_count(), story_count());
    else
        snprintf(s, sizeof s, "%s  JOBS %d  CHIPS %d", rank_name[game_rank()], G.jobs, chips);
    g_text_c(SCR_W / 2, MAP_Y0 + MAP_H * MAP_PX + 8, s, RGB(200, 200, 210), 1);
    giver_t gv;
    if (G.job == J_NONE && story_giver(&gv) && (G.frame & 16))
        dot(gv.x, gv.y, RGB(90, 220, 255), true);
    int rows = 3 + (music_stations() ? 1 : 0);
    for (int i = 0; i < rows; i++) {
        if (i == 0)
            snprintf(s, sizeof s, L("RESUME", "WEITER"));
        else if (i == 1)
            snprintf(s, sizeof s, L("SOUND: %s", "TON: %s"), G.sound ? L("ON", "AN") : L("OFF", "AUS"));
        else if (i == rows - 1)
            snprintf(s, sizeof s, L("QUIT TO TITLE", "ZUM TITELBILD"));
        else
            snprintf(s, sizeof s, "RADIO: %s%s", G.radio == RADIO_AUTO ? "" : "> ",
                     G.radio == RADIO_OFF ? L("OFF", "AUS") : game_station() == MUSIC_POLICE ? L("POLICE", "POLIZEI")
                     : game_station() < 0 ? L("NONE HERE", "HIER KEINS") : music_name(game_station()));
        int y = 234 + i * 11;
        g_text_c(SCR_W / 2, y, s, i == G.menu ? YELLOW : RGB(150, 150, 160), 1);
        if (i == G.menu)
            g_text_s(SCR_W / 2 - g_text_w(s, 1) / 2 - 12, y, ">", YELLOW, 1);
    }
}

static void title(void)
{
    g_dark(0, 40, SCR_W, 62);                   /* behind it the town goes about its day */
    g_text_o(SCR_W - 4 - g_text_w(RT_VERSION, 1), 4, RT_VERSION, WHITE, 1);
    g_text_c(SCR_W / 2 + 2, 52, "RUSHTOWN", RGB(200, 60, 40), 3);
    g_text_c(SCR_W / 2, 50, "RUSHTOWN", YELLOW, 3);
    g_text_c(SCR_W / 2, 84, L("A CITY, A CAR, NO RULES", "STADT, AUTO, KEINE REGELN"), WHITE, 1);

    /* the two games, each with what is saved of it */
    for (int m = RT_STORY ? 0 : 1; m < MODES; m++) {
        int y = 122 + (m - (RT_STORY ? 0 : 1)) * 40;
        bool sel = m == G.menu;
        const save_t *sv = &G.slot[m];
        char s[40];
        if (sel) {                              /* a frame around the one that is chosen */
            g_frame(11, y - 6, SCR_W - 22, 38, RGB(0, 0, 0));
            g_frame(12, y - 5, SCR_W - 24, 36, YELLOW);
            g_frame(13, y - 4, SCR_W - 26, 34, YELLOW);
            g_frame(14, y - 3, SCR_W - 28, 32, RGB(0, 0, 0));
        }
        g_text_o(22, y, m == MODE_STORY ? "STORY" : L("FREE ROAM", "FREIES SPIEL"), sel ? YELLOW : WHITE, 2);
        if (sv->magic != SAVE_MAGIC)
            snprintf(s, sizeof s, m == MODE_STORY ? L("new: 20 missions", "neu: 20 Missionen") : L("new: everything is open", "neu: alles ist offen"));
        else if (m == MODE_STORY)
            snprintf(s, sizeof s, L("mission %d of %d  $%d", "Mission %d von %d  $%d"), sv->story + 1 > story_count() ? story_count() : sv->story + 1,
                     story_count(), (int)sv->cash);
        else
            snprintf(s, sizeof s, "$%d  %d jobs", (int)sv->cash, sv->jobs);
        g_text_o(22, y + 20, s, WHITE, 1);
    }
    const char *t = (G.frame >> 4) & 1 ? L("A OR B: PLAY", "A ODER B: SPIELEN") : "";
    g_text_o(SCR_W / 2 - g_text_w(t, 1) / 2, 210, t, WHITE, 1);
    t = L("< ENGLISH >", "< DEUTSCH >");
    g_text_o(SCR_W / 2 - g_text_w(t, 1) / 2, 226, t, WHITE, 1);
    t = L("IN GAME, HOLD CENTER: MAP", "IM SPIEL MITTE HALTEN: KARTE");
    g_text_o(SCR_W / 2 - g_text_w(t, 1) / 2, 265, t, WHITE, 1);
}

/* no life left */
static void game_over(void)
{
    g_picture(PIC_KO, 0, 0);
    g_dark(0, 0, SCR_W, SCR_H);
    g_text_c(SCR_W / 2, 60, "GAME OVER", RGB(255, 80, 60), 3);
    g_text_c(SCR_W / 2, 120, L("NO LIFE LEFT.", "KEIN LEBEN MEHR."), WHITE, 1);
    g_text_c(SCR_W / 2, 138, L("MONEY AND GUN ARE GONE.", "GELD UND WAFFE SIND WEG."), WHITE, 1);
    if (G.mode == MODE_STORY && story_count()) {
        char s[48];
        g_text_c(SCR_W / 2, 164, L("THE STORY GOES BACK TO", "DIE STORY GEHT ZURÜCK ZU"), WHITE, 1);
        snprintf(s, sizeof s, "%s", story_chapter_name(G.story));
        g_text_c(SCR_W / 2, 178, s, RGB(90, 220, 255), 1);
    }
    g_text_c(SCR_W / 2, 210, L("WHAT YOU OWN STAYS YOURS.", "WAS DIR GEHÖRT, BLEIBT."), RGB(170, 170, 180), 1);
    if ((G.frame >> 4) & 1)
        g_text_c(SCR_W / 2, 240, L("A OR B", "A ODER B"), YELLOW, 1);
}

#if RT_STORY
/* before a mission: at the story's turns a picture, then the briefing */
static void talk(void)
{
    const char *name, *title;
    const char *const *lines;
    int reward;
    char s[40];
    if (G.card >= 0 && g_picture(G.card, 0, 0)) {
        const char *cap = story_caption(G.card);
        g_dark(0, SCR_H - 34, SCR_W, 34);
        g_dark(0, SCR_H - 34, SCR_W, 34);
        g_text_c(SCR_W / 2, SCR_H - 28, cap, WHITE, 1);
        if ((G.frame >> 4) & 1)
            g_text_c(SCR_W / 2, SCR_H - 14, "A", YELLOW, 1);
        return;
    }
    story_brief(&name, &title, &lines, &reward);
    g_dark(0, 0, SCR_W, SCR_H);
    g_rect(8, 30, SCR_W - 16, 222, RGB(18, 20, 30));
    g_frame(8, 30, SCR_W - 16, 222, RGB(90, 220, 255));
    int tx = 16;
    if (g_picture(story_portrait(), 16, 38)) {  /* who is talking, beside the name */
        g_frame(15, 37, 74, 92, RGB(90, 220, 255));
        tx = 98;
    }
    /* the name and the title, each broken at a blank if it is too long for its place */
    for (int part = 0; part < 2; part++) {
        const char *t = part ? title : name;
        int y = part ? 78 : 44, room = (SCR_W - 12 - tx) / 8, len = (int)strlen(t);
        px_t col = part ? YELLOW : RGB(90, 220, 255);
        if (g_text_w(t, 1) / 8 <= room)
            g_text_s(tx, y, t, col, 1);
        else {
            int cut = room < len ? room : len;
            while (cut > 0 && t[cut] != ' ')
                cut--;
            snprintf(s, sizeof s, "%.*s", cut, t);
            g_text_s(tx, y, s, col, 1);
            g_text_s(tx, y + 12, t + cut + 1, col, 1);
        }
    }
    for (int i = 0; i < 4; i++)
        if (lines[i])
            g_text(16, 140 + i * 14, lines[i], WHITE, 1);
    if (G.fails)                                /* arrested on this one before: it pays less now */
        snprintf(s, sizeof s, L("PAYS ONLY $%d NOW", "BRINGT NUR NOCH $%d"), reward);
    else
        snprintf(s, sizeof s, L("PAYS $%d", "BRINGT $%d"), reward);
    g_text_s(16, 202, s, G.fails ? RGB(255, 96, 64) : RGB(120, 255, 120), 1);
    g_text_c(SCR_W / 2, 222, L("A: I'M IN", "A: BIN DABEI"), WHITE, 1);
    g_text_c(SCR_W / 2, 236, L("B: NOT NOW", "B: SPÄTER"), RGB(170, 170, 180), 1);
}
#endif

int ground_only;                        /* tools/host.c: the town without the display on top */

void draw_all(void)
{
    ground();
    ground_things();
    lights();
    building_shadows();
    trees(true);
    movers();
    roofs();
    trees(false);
    effects();
    if (ground_only)
        return;
    if (G.state == ST_TITLE)
        title();
    else if (G.state == ST_OVER)
        game_over();
    else {
        hud();
        if (G.state == ST_PAUSE)
            pause_menu();
#if RT_STORY
        if (G.state == ST_TALK)
            talk();
#endif
    }
}
