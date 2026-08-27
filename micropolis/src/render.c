#include "render.h"
#include "config.h"
#include "engine.h"
#include "tiles.h"
#include "ui.h"
#include "font_data.h"
#include "save.h"
#include "title_image.h"
#include "sprites.h"
#include <string.h>

#define COL_STATUS_BG 0x18E3   // dark slate
#define COL_HUD_BG    0x10A2
#define COL_TEXT      0xEF7D   // off-white
#define COL_CURSOR    0xFFE0   // yellow
#define COL_SEL_RING  0x07FF   // cyan (toolbar selection)
#define COL_R         0x5DCB   // demand bar: residential (green)
#define COL_C         0x35BF   // commercial (blue)
#define COL_I         0xEDA6   // industrial (amber)
#define COL_HUD_FRAME 0xAD55   // light grey — RCI bar outlines

static inline void px(uint16_t *fb, int x, int y, uint16_t c) {
    if ((unsigned)x < DISPLAY_W && (unsigned)y < DISPLAY_H)
        fb[y * DISPLAY_W + x] = c;
}

static void fill_rect(uint16_t *fb, int x, int y, int w, int h, uint16_t c) {
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i)
            px(fb, x + i, y + j, c);
}

static void rect_outline(uint16_t *fb, int x, int y, int w, int h, uint16_t c) {
    for (int i = 0; i < w; ++i) { px(fb, x + i, y, c); px(fb, x + i, y + h - 1, c); }
    for (int j = 0; j < h; ++j) { px(fb, x, y + j, c); px(fb, x + w - 1, y + j, c); }
}

// ---- anti-aliased monospace font (uppercase + digits) ---------------
static inline void blend_px(uint16_t *fb, int x, int y, uint16_t color, uint8_t cov) {
    if ((unsigned)x >= DISPLAY_W || (unsigned)y >= DISPLAY_H || cov == 0) return;
    uint16_t p = fb[y * DISPLAY_W + x];
    int pr = (p >> 11) & 0x1F, pg = (p >> 5) & 0x3F, pb = p & 0x1F;
    int cr = (color >> 11) & 0x1F, cg = (color >> 5) & 0x3F, cb = color & 0x1F;
    pr = (cr * cov + pr * (255 - cov)) / 255;
    pg = (cg * cov + pg * (255 - cov)) / 255;
    pb = (cb * cov + pb * (255 - cov)) / 255;
    fb[y * DISPLAY_W + x] = (uint16_t)((pr << 11) | (pg << 5) | pb);
}

static void draw_glyph(uint16_t *fb, int x, int y, char ch, uint16_t c) {
    if (ch >= 'a' && ch <= 'z') ch -= 32;                 // uppercase only
    unsigned u = (unsigned char)ch;
    if (u < FONT_FIRST || u > FONT_LAST) return;
    const uint8_t *g = kFont[u - FONT_FIRST];
    for (int row = 0; row < FONT_H; ++row)
        for (int col = 0; col < FONT_W; ++col)
            blend_px(fb, x + col, y + row, c, g[row * FONT_W + col]);
}

// Left-aligned text; returns the x just past the last glyph.
static int draw_text(uint16_t *fb, int x, int y, const char *t, uint16_t c) {
    for (; *t; ++t) { draw_glyph(fb, x, y, *t, c); x += FONT_W; }
    return x;
}

// Right-aligned unsigned number; returns the left edge x reached.
static int draw_num(uint16_t *fb, int x_right, int y, long v, uint16_t c) {
    char buf[16]; int n = 0;
    if (v < 0) v = 0;
    do { buf[n++] = (char)('0' + (v % 10)); v /= 10; } while (v && n < 15);
    int x = x_right - n * FONT_W;
    for (int i = 0; i < n; ++i) draw_glyph(fb, x + i * FONT_W, y, buf[n - 1 - i], c);
    return x;
}

// Blit a w x h RGB565 image (clipped to the screen).
static void blit(uint16_t *fb, int dx, int dy, const uint16_t *src, int w, int h) {
    for (int y = 0; y < h; ++y) {
        int yy = dy + y;
        if ((unsigned)yy >= DISPLAY_H) break;
        for (int x = 0; x < w; ++x) {
            int xx = dx + x;
            if ((unsigned)xx >= DISPLAY_W) continue;
            fb[yy * DISPLAY_W + xx] = src[y * w + x];
        }
    }
}

// Transparent blit: pixels equal to `key` are skipped. Clips to the screen.
static void blit_key(uint16_t *fb, int dx, int dy, const uint16_t *src, int w, int h, uint16_t key) {
    for (int y = 0; y < h; ++y) {
        int yy = dy + y;
        if ((unsigned)yy >= DISPLAY_H) continue;
        for (int x = 0; x < w; ++x) {
            int xx = dx + x;
            if ((unsigned)xx >= DISPLAY_W) continue;
            uint16_t c = src[y * w + x];
            if (c != key) fb[yy * DISPLAY_W + xx] = c;
        }
    }
}

// ---- map ------------------------------------------------------------
#define FIRE_LO 56
#define FIRE_HI 63
#define TILE_LIGHTNING 827
#define COL_MM_WATER 0x3319        // mini-map: water (blue)
#define COL_MM_LAND  0x9389        // mini-map: land (earth)
#define COL_MM_TREE  0x2C66        // mini-map: woods/forest (green)
static unsigned g_anim = 0;          // advances once per frame; drives fire flicker + bolt blink

static void draw_map(uint16_t *fb) {
    int blink = (g_anim >> 4) & 1;                       // lightning bolt on/off phase
    for (int ty = 0; ty < VIEW_ROWS; ++ty) {
        for (int tx = 0; tx < VIEW_COLS; ++tx) {
            int wx = UI.scroll_x + tx, wy = UI.scroll_y + ty;
            int id = engine_tile(wx, wy);
            if (id >= FIRE_LO && id <= FIRE_HI)           // flicker fire through its 8 frames
                id = FIRE_LO + ((g_anim / 3 + tx * 5 + ty * 3) & 7);
            if (blink && engine_zone_powerless(wx, wy))   // blink "no power" bolt over the zone
                id = TILE_LIGHTNING;
            const uint16_t *src = tile_get(id);
            int px0 = tx * TILE_PX, py0 = ty * TILE_PX;
            for (int sy = 0; sy < TILE_PX; ++sy) {
                int dy = py0 + sy;
                if ((unsigned)dy >= DISPLAY_H) break;
                uint16_t *row = &fb[dy * DISPLAY_W + px0];
                const uint16_t *srow = &src[sy * TILE_PX];
                for (int sx = 0; sx < TILE_PX; ++sx) row[sx] = srow[sx];
            }
        }
    }
}

// Rectangle outline clamped to the visible map band [STATUS_H, DISPLAY_H-HUD_H),
// so the cursor's edges stay visible instead of being painted over by the bars.
static void cursor_box(uint16_t *fb, int x, int y, int w, int h, uint16_t c) {
    int y0v = STATUS_H, y1v = DISPLAY_H - HUD_H;
    int cx0 = x, cy0 = y, cx1 = x + w - 1, cy1 = y + h - 1;
    if (cx0 < 0) cx0 = 0; if (cx1 > DISPLAY_W - 1) cx1 = DISPLAY_W - 1;
    if (cy0 < y0v) cy0 = y0v; if (cy1 > y1v - 1) cy1 = y1v - 1;
    if (cx0 > cx1 || cy0 > cy1) return;
    for (int i = cx0; i <= cx1; ++i) { px(fb, i, cy0, c); px(fb, i, cy1, c); }
    for (int j = cy0; j <= cy1; ++j) { px(fb, cx0, j, c); px(fb, cx1, j, c); }
}

static void draw_cursor(uint16_t *fb) {
    int n = engine_tool_size(UI.brush);          // footprint NxN tiles
    int tx = UI.cur_x, ty = UI.cur_y;
    if (n > 1) { tx -= 1; ty -= 1; }             // 3x3/4x4/6x6 build from (x-1,y-1)
    int sx = (tx - UI.scroll_x) * TILE_PX;
    int sy = (ty - UI.scroll_y) * TILE_PX;
    int sz = n * TILE_PX;
    cursor_box(fb, sx, sy, sz, sz, COL_CURSOR);
    cursor_box(fb, sx + 1, sy + 1, sz - 2, sz - 2, COL_CURSOR);
}

// ---- HUD ------------------------------------------------------------
static void draw_status(uint16_t *fb) {
    fill_rect(fb, 0, 0, DISPLAY_W, STATUS_H, COL_STATUS_BG);
    int y = (STATUS_H - FONT_H) / 2;
    // funds with a leading "$", left-aligned so the gap from the left edge to
    // the "$" matches the gap from the right edge to the last digit of the year.
    {
        char buf[16]; int p = 0, n = 0; char d[12];
        long f = engine_funds();
        buf[p++] = '$';
        if (f < 0) { buf[p++] = '-'; f = -f; }
        do { d[n++] = (char)('0' + (f % 10)); f /= 10; } while (f && n < 11);
        while (n) buf[p++] = d[--n];
        buf[p] = 0;
        draw_text(fb, 24, y, buf, COL_TEXT);
    }
    // year, right (clear of the rounded top-right corner)
    draw_num(fb, DISPLAY_W - 24, y, engine_year(), COL_TEXT);
}

static void demand_bar(uint16_t *fb, int x, int base_y, int demand, uint16_t c) {
    int h = demand; if (h < 0) h = 0; if (h > 32) h = 32;
    h = h * 38 / 32;                         // scale to <=38 px (1px taller each end)
    fill_rect(fb, x, base_y + 1 - h, 10, h, c);
    rect_outline(fb, x, base_y - 37, 10, 38, COL_HUD_FRAME);
}

static int slen(const char *s) { int n = 0; while (s[n]) ++n; return n; }

// Mini-map in the bottom HUD: water=blue, land=earth, structures=white,
// with a blinking frame around the current viewport.
static void draw_minimap(uint16_t *fb) {
    const int MW = 43, MH = 36;                 // same height as the RCI bar frames
    int x0 = 72;                                // gap to the right of the industry bar (ends x66)
    int y0 = (DISPLAY_H - 12) - MH;             // top-aligned with the RCI bar frames (y232..268)
    rect_outline(fb, x0 - 1, y0 - 1, MW + 2, MH + 2, COL_HUD_FRAME);
    for (int my = 0; my < MH; ++my) {
        int wy = (my * WORLD_H + WORLD_H / 2) / MH;
        for (int mx = 0; mx < MW; ++mx) {
            int wx = (mx * WORLD_W + WORLD_W / 2) / MW;
            int t = engine_tile(wx, wy);
            uint16_t c = (t >= 64) ? 0xFFFF
                       : (t >= 2 && t <= 20) ? COL_MM_WATER
                       : (t >= 21 && t <= 43) ? COL_MM_TREE
                       : COL_MM_LAND;
            px(fb, x0 + mx, y0 + my, c);
        }
    }
    {                                              // solid current-viewport rectangle
        int rx = UI.scroll_x * MW / WORLD_W, ry = UI.scroll_y * MH / WORLD_H;
        int rw = VIEW_COLS * MW / WORLD_W; if (rw < 3) rw = 3;
        int rh = VIEW_ROWS * MH / WORLD_H; if (rh < 3) rh = 3;
        rect_outline(fb, x0 + rx, y0 + ry, rw, rh, COL_CURSOR);
    }
}

static void draw_hud(uint16_t *fb) {
    int y0 = DISPLAY_H - HUD_H;
    fill_rect(fb, 0, y0, DISPLAY_W, HUD_H, COL_HUD_BG);

    int base_y = DISPLAY_H - 12;
    demand_bar(fb, 22, base_y, engine_demand_r(), COL_R);
    demand_bar(fb, 38, base_y, engine_demand_c(), COL_C);
    demand_bar(fb, 54, base_y, engine_demand_i(), COL_I);

    draw_minimap(fb);

    // active tool: icon (right) with its name + optional price beside it
    int x = DISPLAY_W - ICON_PX - 22, y = y0 + (HUD_H - ICON_PX) / 2;  // 2px right of before
    blit(fb, x, y, tool_icon(UI.brush), ICON_PX, ICON_PX);
    rect_outline(fb, x - 1, y - 1, ICON_PX + 2, ICON_PX + 2, COL_TEXT);

    const char *name = ui_entry_name(UI.brush);
    long cost = ui_entry_cost(UI.brush);
    int right = x - 8;                          // constant gap between text and icon
    int cy = y0 + HUD_H / 2;                     // vertical centre of the HUD band
    int name_x = right - slen(name) * FONT_W;
    if (cost >= 0) {
        draw_text(fb, name_x, cy - FONT_H - 1, name, COL_TEXT);   // name above centre
        draw_num (fb, right,  cy + 1,          cost, COL_TEXT);   // price below, same right edge
    } else {
        draw_text(fb, name_x, cy - FONT_H / 2, name, COL_TEXT);   // name centred (no price)
    }
}

// Darken a cell toward the panel background to show a locked/greyed entry.
static void dim_cell(uint16_t *fb, int x, int y, int w, int h) {
    int br = (COL_HUD_BG >> 11) & 0x1F, bg = (COL_HUD_BG >> 5) & 0x3F, bb = COL_HUD_BG & 0x1F;
    for (int j = 0; j < h; ++j) {
        int yy = y + j; if ((unsigned)yy >= DISPLAY_H) continue;
        for (int i = 0; i < w; ++i) {
            int xx = x + i; if ((unsigned)xx >= DISPLAY_W) continue;
            uint16_t p = fb[yy * DISPLAY_W + xx];
            int r = (p >> 11) & 0x1F, g = (p >> 5) & 0x3F, b = p & 0x1F;
            r = (r * 35 + br * 65) / 100;     // ~35% icon, 65% panel bg
            g = (g * 35 + bg * 65) / 100;
            b = (b * 35 + bb * 65) / 100;
            fb[yy * DISPLAY_W + xx] = (uint16_t)((r << 11) | (g << 5) | b);
        }
    }
}

// Floating tool palette over the map (shown only while open; build is paused).
static void draw_palette(uint16_t *fb) {
    int cols = PALETTE_COLS, rows = ui_grid_rows();
    int gap = 4, cell = ICON_PX + gap;
    int gw = cols * cell - gap, gh = rows * cell - gap;
    int px0 = (DISPLAY_W - gw) / 2;
    int top = STATUS_H + 20;          // 12px lower so the rounded corners don't clip it
    int gridY = top + 16;
    int sel = UI.menu_sel;

    fill_rect(fb, px0 - 6, top - 8, gw + 12, gh + 30, COL_HUD_BG);
    rect_outline(fb, px0 - 6, top - 8, gw + 12, gh + 30, COL_TEXT);

    draw_text(fb, px0, top, ui_entry_name(sel), COL_TEXT);
    long cost = ui_entry_cost(sel);
    if (cost >= 0) draw_num(fb, px0 + gw, top, cost, COL_TEXT);

    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c) {
            int e = ui_grid_cell(r, c);
            if (e < 0) continue;
            int x = px0 + c * cell;
            int y = gridY + r * cell;
            blit(fb, x, y, tool_icon(ui_entry_icon(e)), ICON_PX, ICON_PX);
            if (!ui_entry_available(e)) dim_cell(fb, x, y, ICON_PX, ICON_PX);
            if (e == sel) {
                rect_outline(fb, x - 1, y - 1, ICON_PX + 2, ICON_PX + 2, COL_SEL_RING);
                rect_outline(fb, x - 2, y - 2, ICON_PX + 4, ICON_PX + 4, COL_SEL_RING);
            }
        }
}

// ---- full-screen menu panels ----------------------------------------
#define PANEL_X 6
#define PANEL_Y (STATUS_H + 4)
#define PANEL_W (DISPLAY_W - 12)
#define PANEL_H (DISPLAY_H - HUD_H - STATUS_H - 8)

static void panel_bg(uint16_t *fb, const char *title) {
    fill_rect(fb, PANEL_X, PANEL_Y, PANEL_W, PANEL_H, COL_HUD_BG);
    rect_outline(fb, PANEL_X, PANEL_Y, PANEL_W, PANEL_H, COL_TEXT);
    draw_text(fb, PANEL_X + 6, PANEL_Y + 5, title, COL_TEXT);
}
static void footer(uint16_t *fb, const char *hint) {
    draw_text(fb, PANEL_X + PANEL_W - 8 - slen(hint) * FONT_W,
              PANEL_Y + PANEL_H - FONT_H - 5, hint, COL_HUD_FRAME);
}

static void draw_line(uint16_t *fb, int x0, int y0, int x1, int y1, uint16_t c) {
    int dx = x1 - x0; if (dx < 0) dx = -dx;
    int dy = y1 - y0; if (dy < 0) dy = -dy; dy = -dy;
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (;;) {
        px(fb, x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void draw_eval(uint16_t *fb) {
    static const char *cls[6] = { "VILLAGE", "TOWN", "CITY", "CAPITAL", "METROPOLIS", "MEGALOPOLIS" };
    panel_bg(fb, "EVALUATION");
    int x = PANEL_X + 10, y = PANEL_Y + 28, d = FONT_H + 6, rx = PANEL_X + PANEL_W - 10;
    int cc = engine_eval_class(); if (cc < 0) cc = 0; if (cc > 5) cc = 5;
    draw_text(fb, x, y, "POPULATION", COL_TEXT); draw_num(fb, rx, y, engine_eval_pop(), COL_TEXT); y += d;
    draw_text(fb, x, y, "CLASS", COL_TEXT);      draw_text(fb, rx - slen(cls[cc]) * FONT_W, y, cls[cc], COL_TEXT); y += d;
    draw_text(fb, x, y, "SCORE", COL_TEXT);      draw_num(fb, rx, y, engine_eval_score(), COL_TEXT); y += d;
    draw_text(fb, x, y, "APPROVAL", COL_TEXT);
    draw_glyph(fb, rx - FONT_W, y, '%', COL_TEXT); draw_num(fb, rx - FONT_W, y, engine_eval_approval(), COL_TEXT); y += d;
    draw_text(fb, x, y, "FUNDS", COL_TEXT);      draw_num(fb, rx, y, engine_funds(), COL_TEXT);
    footer(fb, "A/B: BACK");
}

static void draw_graphs(uint16_t *fb) {
    panel_bg(fb, "GRAPHS");
    int gx = PANEL_X + 10, gy = PANEL_Y + 28, gw = PANEL_W - 20, gh = PANEL_H - 28 - 28;
    rect_outline(fb, gx, gy, gw, gh, COL_HUD_FRAME);
    short buf[120];
    const uint16_t col[3] = { COL_R, COL_C, COL_I };
    int mx = 1;
    for (int s = 0; s < 3; ++s) { int n = engine_history(s, buf, 120); for (int i = 0; i < n; ++i) if (buf[i] > mx) mx = buf[i]; }
    for (int s = 0; s < 3; ++s) {
        int n = engine_history(s, buf, 120), lx = -1, ly = -1, den = (n > 1 ? n - 1 : 1);
        for (int i = 0; i < n; ++i) {
            int xx = gx + gw - 1 - i * (gw - 1) / den;   // newest (i=0) at right
            int yy = gy + gh - 1 - buf[i] * (gh - 1) / mx;
            if (lx >= 0) draw_line(fb, lx, ly, xx, yy, col[s]);
            lx = xx; ly = yy;
        }
    }
    const char *lab[3] = { "R", "C", "I" };
    int ly2 = gy + gh + 7;
    for (int s = 0; s < 3; ++s) { int bx = gx + s * 34; fill_rect(fb, bx, ly2, 8, 8, col[s]); draw_text(fb, bx + 11, ly2 - 2, lab[s], COL_TEXT); }
    footer(fb, "A/B: BACK");
}

static void draw_maps(uint16_t *fb) {
    static const char *nm[6] = { "POPULATION", "CRIME", "POLLUTION", "LAND VALUE", "TRAFFIC", "POWER" };
    int t = UI.map_type;
    panel_bg(fb, nm[t]);
    int w, h; engine_overlay_dims(&w, &h);
    int mh = PANEL_H - 28 - 28;                 // same height as the graphs plot box
    int mw = mh * w / h;                         // preserve aspect -> narrower
    int ox = PANEL_X + (PANEL_W - mw) / 2, oy = PANEL_Y + 28;   // top-aligned with graphs
    for (int yy = 0; yy < mh; ++yy) {
        int wy = yy * WORLD_H / mh, cy = yy * h / mh;
        for (int xx = 0; xx < mw; ++xx) {
            int v = engine_overlay(t, xx * w / mw, cy);
            uint16_t c;
            if (v >= 50) {                       // significant data -> hot overlay (yellow..red)
                int s = v - 50; if (s > 205) s = 205;
                int g = 63 - s * 63 / 205;
                c = (uint16_t)((31 << 11) | (g << 5));
            } else {                             // city as dimmable background (mini-map palette)
                int tile = engine_tile(xx * WORLD_W / mw, wy);
                c = (tile >= 64)                ? 0xAD55              // structures: gray
                  : (tile >= 2 && tile <= 20)   ? COL_MM_WATER
                  : (tile >= 21 && tile <= 43)  ? COL_MM_TREE
                  :                               COL_MM_LAND;
            }
            px(fb, ox + xx, oy + yy, c);
        }
    }
    rect_outline(fb, ox - 1, oy - 1, mw + 2, mh + 2, COL_HUD_FRAME);
    {                                            // white frame = current viewport
        int rx = UI.scroll_x * mw / WORLD_W, ry = UI.scroll_y * mh / WORLD_H;
        int rw = VIEW_COLS * mw / WORLD_W, rh = VIEW_ROWS * mh / WORLD_H;
        if (rx < 0) rx = 0; if (ry < 0) ry = 0;
        if (rw < 3) rw = 3; if (rh < 3) rh = 3;
        if (rx + rw > mw) rw = mw - rx; if (ry + rh > mh) rh = mh - ry;
        rect_outline(fb, ox + rx, oy + ry, rw, rh, 0xFFFF);
    }
    draw_text(fb, PANEL_X + 10, PANEL_Y + PANEL_H - FONT_H - 5, "L/R: MAP", COL_HUD_FRAME);
    footer(fb, "A/B: BACK");
}

static void draw_budget(uint16_t *fb) {
    panel_bg(fb, "BUDGET");
    int x = PANEL_X + 10, y = PANEL_Y + 28, d = FONT_H + 6, rx = PANEL_X + PANEL_W - 10;
    uint16_t c0 = (UI.opt == 0) ? COL_CURSOR : COL_TEXT;
    draw_text(fb, x, y, "TAX RATE", c0);
    draw_glyph(fb, rx - FONT_W, y, '%', c0); draw_num(fb, rx - FONT_W, y, engine_tax_rate(), c0); y += d;
    draw_text(fb, x, y, "INCOME", COL_TEXT); draw_num(fb, rx, y, engine_tax_income(), COL_TEXT); y += d + 4;
    const char *fl[3] = { "ROAD", "POLICE", "FIRE" };
    for (int i = 0; i < 3; ++i) {
        uint16_t rc = (UI.opt == i + 1) ? COL_CURSOR : COL_TEXT;
        draw_text(fb, x, y, fl[i], rc);
        draw_glyph(fb, rx - FONT_W, y, '%', rc); draw_num(fb, rx - FONT_W, y, engine_fund_pct(i), rc); y += d;
    }
    y += 4; draw_text(fb, x, y, "FUNDS", COL_TEXT); draw_num(fb, rx, y, engine_funds(), COL_TEXT);
    draw_text(fb, PANEL_X + 10, PANEL_Y + PANEL_H - FONT_H - 5, "U/D L/R", COL_HUD_FRAME);
    footer(fb, "A:BACK B:OK");
}

static void draw_settings(uint16_t *fb) {
    static const char *diff[3] = { "EASY", "MEDIUM", "HARD" };
    static const char *spd[4]  = { "PAUSE", "SLOW", "NORMAL", "FAST" };
    static const char *lbl[5]  = { "DISASTERS", "DIFFICULTY", "SIM SPEED", "AUTO-DOZE", "AUTO-BUDGET" };
    panel_bg(fb, "SETTINGS");
    int gl = engine_get_difficulty(); if (gl < 0) gl = 0; if (gl > 2) gl = 2;
    int sp = engine_get_sim_speed();  if (sp < 0) sp = 0; if (sp > 3) sp = 3;
    const char *vals[5] = {
        engine_get_no_disasters() ? "OFF" : "ON",   // NoDisasters=1 => disasters off
        diff[gl], spd[sp],
        engine_get_auto_bulldoze() ? "ON" : "OFF",
        engine_get_auto_budget()   ? "ON" : "OFF",
    };
    int x = PANEL_X + 12, rx = PANEL_X + PANEL_W - 10, y = PANEL_Y + 28, d = FONT_H + 8, i;
    for (i = 0; i < 5; ++i) {
        int yy = y + i * d;
        uint16_t c = (i == UI.opt) ? COL_CURSOR : COL_TEXT;
        if (i == UI.opt) draw_text(fb, x - 10, yy, ">", COL_CURSOR);
        draw_text(fb, x, yy, lbl[i], c);
        draw_text(fb, rx - slen(vals[i]) * FONT_W, yy, vals[i], c);
    }
    draw_text(fb, PANEL_X + 10, PANEL_Y + PANEL_H - FONT_H - 5, "U/D L/R", COL_HUD_FRAME);
    footer(fb, "A:BACK B:OK");
}

static void draw_disasters(uint16_t *fb) {
    static const char *opt[7] = { "FIRE", "FLOOD", "TORNADO", "EARTHQUAKE",
                                  "MELTDOWN", "MONSTER", "PLANE CRASH" };
    panel_bg(fb, "DISASTERS");
    int x = PANEL_X + 14, y = PANEL_Y + 26, d = FONT_H + 5, i;
    for (i = 0; i < 7; ++i) {
        int yy = y + i * d;
        uint16_t c = (i == UI.opt) ? COL_CURSOR : COL_TEXT;
        if (i == UI.opt) draw_text(fb, x - 10, yy, ">", COL_CURSOR);
        draw_text(fb, x, yy, opt[i], c);
    }
    footer(fb, "A:BACK B:TRIGGER");
}

// All active sprites (disasters + ambient vehicles), original Micropolis art.
static const uint16_t *spr_img(int type, int frame, int *sz) {
    switch (type) {
        case SPR_GOD: *sz = 48; return kGod[frame % 16];
        case SPR_TOR: *sz = 48; return kTor[frame % 3];
        case SPR_AIR: *sz = 48; return kAir[frame % 11];
        case SPR_SHI: *sz = 48; return kShi[frame % 8];
        case SPR_COP: *sz = 32; return kCop[frame % 8];
        case SPR_TRA: *sz = 32; return kTra[frame % 5];
        case SPR_EXP: *sz = 48; return kExp[frame % 6];
    }
    *sz = 0; return 0;
}
static void draw_sprites(uint16_t *fb) {
    int i, n = engine_sprite_count();
    for (i = 0; i < n; ++i) {
        int t = engine_sprite_type(i), sz;
        const uint16_t *img;
        if (t < 0) continue;
        img = spr_img(t, engine_sprite_frame(i), &sz);
        if (!img) continue;
        int sx = engine_sprite_x(i) - UI.scroll_x * TILE_PX - sz / 2 + TILE_PX / 2;
        int sy = engine_sprite_y(i) - UI.scroll_y * TILE_PX - sz / 2 + TILE_PX / 2;
        blit_key(fb, sx, sy, img, sz, sz, SPR_KEY);
    }
}

static void draw_query(uint16_t *fb) {
    static const char *zones[28] = {
        "Clear","Water","Trees","Rubble","Flood","Nuclear Waste","Fire","Road",
        "Power","Rail","Residential","Commercial","Industrial","Seaport","Airport","Coal Power",
        "Fire Dept","Police Dept","Stadium","Nuclear Power","Bridge","Radar","Fountain","Industrial",
        "Stadium","Bridge","Network","Network" };
    static const char *dens[4]  = { "Low","Medium","High","Very High" };
    static const char *val[4]   = { "Slum","Lower Class","Middle Class","High" };
    static const char *crime[4] = { "Safe","Light","Moderate","Dangerous" };
    static const char *poll[4]  = { "None","Moderate","Heavy","Very Heavy" };
    static const char *grow[4]  = { "Declining","Stable","Slow Growth","Fast Growth" };
    query_t q; engine_query(UI.cur_x, UI.cur_y, &q);
    int zi = (q.zone < 0 || q.zone > 27) ? 0 : q.zone;
    const char *lab[5] = { "DENSITY", "VALUE", "CRIME", "POLLUTION", "GROWTH" };
    const char *vv[5];
    vv[0] = dens[q.density & 3];  vv[1] = val[q.value & 3];   vv[2] = crime[q.crime & 3];
    vv[3] = poll[q.pollution & 3]; vv[4] = grow[q.growth & 3];
    panel_bg(fb, "QUERY");
    int x = PANEL_X + 12, y = PANEL_Y + 26, d = FONT_H + 6, rx = PANEL_X + PANEL_W - 12, i;
    draw_text(fb, x, y, zones[zi], COL_CURSOR);            // zone type
    y += d + 4;
    for (i = 0; i < 5; ++i) {
        draw_text(fb, x, y, lab[i], COL_TEXT);
        draw_text(fb, rx - slen(vv[i]) * FONT_W, y, vv[i], COL_TEXT);
        y += d;
    }
    footer(fb, "A/B: BACK");
}

static void draw_screen(uint16_t *fb) {
    switch (UI.screen) {
        case SCR_MAPS:   draw_maps(fb);   break;
        case SCR_GRAPHS: draw_graphs(fb); break;
        case SCR_BUDGET: draw_budget(fb); break;
        case SCR_EVAL:   draw_eval(fb);   break;
        case SCR_SETTINGS:  draw_settings(fb);  break;
        case SCR_DISASTERS: draw_disasters(fb); break;
        case SCR_QUERY:     draw_query(fb);     break;
    }
}

// ---- rounded-rectangle panel (used by the title-screen menu box) ----
static int in_round(int px_, int py_, int x, int y, int w, int h, int r) {
    int rx = -1, ry = -1;
    if      (px_ <  x + r     && py_ <  y + r)     { rx = x + r;         ry = y + r;         }
    else if (px_ >= x + w - r && py_ <  y + r)     { rx = x + w - 1 - r; ry = y + r;         }
    else if (px_ <  x + r     && py_ >= y + h - r) { rx = x + r;         ry = y + h - 1 - r; }
    else if (px_ >= x + w - r && py_ >= y + h - r) { rx = x + w - 1 - r; ry = y + h - 1 - r; }
    if (rx >= 0) { int dx = px_ - rx, dy = py_ - ry; return dx*dx + dy*dy <= r*r; }
    return 1;
}
static void fill_round_rect(uint16_t *fb, int x, int y, int w, int h, int r, uint16_t c) {
    int i, j;
    for (j = 0; j < h; ++j)
        for (i = 0; i < w; ++i)
            if (in_round(x + i, y + j, x, y, w, h, r)) px(fb, x + i, y + j, c);
}
static void round_outline(uint16_t *fb, int x, int y, int w, int h, int r, uint16_t c) {
    int i, j;
    for (j = 0; j < h; ++j)
        for (i = 0; i < w; ++i) {
            if (!in_round(x + i, y + j, x, y, w, h, r)) continue;
            if (!in_round(x+i-1, y+j, x,y,w,h,r) || !in_round(x+i+1, y+j, x,y,w,h,r) ||
                !in_round(x+i, y+j-1, x,y,w,h,r) || !in_round(x+i, y+j+1, x,y,w,h,r))
                px(fb, x + i, y + j, c);
        }
}

#define COL_TITLE_BG   0x0A29   // deep navy panel (matches the cover blue)
#define COL_TITLE_GOLD 0xFE60   // gold accent (matches the logo)

static void draw_title(uint16_t *fb) {
    // Vertical centres of the four baked menu lines (0=Load 1=Easy 2=Medium 3=Hard).
    static const int cy[4] = { 171, 190, 208, 227 };
    int sel = UI.menu_sel; if (sel < 0) sel = 0; if (sel > 3) sel = 3;
    blit(fb, 0, 0, kTitleImage, DISPLAY_W, DISPLAY_H);
    uint16_t col = (sel == 0 && !save_present()) ? COL_HUD_FRAME : 0xD5B1;  // text colour (greyed if no save)

    // Small right-pointing triangle before the selected line, fixed x. Its back
    // edge sits inset from the panel's left border (64) by the same amount the
    // longest entry ("Medium", right edge 168) is inset from the right border (176).
    int bx = 72, tw = 6, th = 11, half = th / 2, dy, x, ccy = cy[sel];
    for (dy = -half; dy <= half; ++dy) {
        int w = tw - (tw * (dy < 0 ? -dy : dy)) / half;     // full width at centre, 0 at the points
        for (x = 0; x <= w; ++x) px(fb, bx + x, ccy + dy, col);
    }
}

void render_frame(uint16_t *fb) {
    g_anim++;
    if (UI.mode == UI_TITLE) { draw_title(fb); return; }
    draw_map(fb);
    draw_sprites(fb);
    draw_cursor(fb);
    draw_status(fb);
    draw_hud(fb);
    if (UI.mode == UI_TOOLBAR)      draw_palette(fb);
    else if (UI.mode == UI_SCREEN)  draw_screen(fb);
}
