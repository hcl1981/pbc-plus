#include "ui.h"
#include "config.h"
#include "engine.h"
#include "save.h"

ui_t UI;

// ---- palette entries: tools (recognisable Micropolis tiles as icons) +
//      system actions. Tile ids are zone-centre / network tiles from the
//      real tileset, so the icon already reads as R/C/I, road, rail, etc.
// ---- palette entries. `icon` indexes kToolIcons. min_year>0 = locked.
//      Grid positions are defined by k_layout below (entries stay in this order).
enum { ACT_TOOL = 0, ACT_SAVE, ACT_LOAD, ACT_MAPS, ACT_GRAPHS, ACT_BUDGET, ACT_EVAL, ACT_SETTINGS, ACT_DISASTER };
typedef struct { const char *name; long cost; uint8_t action; int min_year; uint8_t icon; } entry_t;
static const entry_t k_entries[] = {
    { "DOZE",     1,     ACT_TOOL,   0,     0 },
    { "ROAD",     10,    ACT_TOOL,   0,     1 },
    { "RAIL",     20,    ACT_TOOL,   0,     2 },
    { "WIRE",     5,     ACT_TOOL,   0,     3 },
    { "RES",      100,   ACT_TOOL,   0,     4 },
    { "COM",      100,   ACT_TOOL,   0,     5 },
    { "IND",      100,   ACT_TOOL,   0,     6 },
    { "PARK",     10,    ACT_TOOL,   0,     7 },
    { "POLICE",   500,   ACT_TOOL,   0,     8 },
    { "FIRE",     500,   ACT_TOOL,   0,     9 },
    { "POWER",    3000,  ACT_TOOL,   0,    10 },
    { "NUCLEAR",  5000,  ACT_TOOL,   1955, 11 },  // locked until 1955
    { "SEAPORT",  5000,  ACT_TOOL,   0,    12 },
    { "AIRPORT",  10000, ACT_TOOL,   0,    13 },
    { "STADIUM",  5000,  ACT_TOOL,   0,    14 },
    { "QUERY",    -1,    ACT_TOOL,   0,    15 },
    { "MAPS",     -1,    ACT_MAPS,   0,    18 },
    { "GRAPHS",   -1,    ACT_GRAPHS, 0,    19 },
    { "BUDGET",   -1,    ACT_BUDGET, 0,    20 },
    { "EVAL",     -1,    ACT_EVAL,   0,    21 },
    { "SAVE",     -1,    ACT_SAVE,   0,    16 },
    { "LOAD",     -1,    ACT_LOAD,   0,    17 },
    { "SETTINGS", -1,    ACT_SETTINGS, 0,  22 },
    { "DISASTERS",-1,    ACT_DISASTER, 0,  23 },
};
#define ENTRY_COUNT ((int)(sizeof k_entries / sizeof k_entries[0]))

int         ui_entry_count(void)      { return ENTRY_COUNT; }
const char *ui_entry_name(int i)      { return k_entries[i].name; }
long        ui_entry_cost(int i)      { return k_entries[i].cost; }
int         ui_entry_icon(int i)      { return k_entries[i].icon; }
// Locked tools (e.g. nuclear before 1955) are greyed out and cannot be picked.
bool        ui_entry_available(int i) { return k_entries[i].min_year == 0 ||
                                               engine_year() >= k_entries[i].min_year; }

// Explicit palette layout (row, col) -> entry index, or -1 for an empty cell.
// Save/Load sit in the last row at the far right, under Maps and Graphs.
#define GRID_ROWS 4
static const signed char k_layout[GRID_ROWS][PALETTE_COLS] = {
    {  0,  1,  2,  3,  4,  5 },   // doze road rail wire res com
    {  6,  7,  8,  9, 10, 11 },   // ind park police fire coal nuclear
    { 12, 13, 14, 15, 16, 17 },   // seaport airport stadium query MAPS GRAPHS
    { 18, 19, 23, 22, 20, 21 },   // budget eval DISASTERS SETTINGS SAVE LOAD
};
int ui_grid_rows(void)        { return GRID_ROWS; }
int ui_grid_cell(int r, int c){ if (r < 0 || r >= GRID_ROWS || c < 0 || c >= PALETTE_COLS) return -1;
                                 return k_layout[r][c]; }
static void sel_to_rc(int sel, int *pr, int *pc) {
    for (int r = 0; r < GRID_ROWS; ++r)
        for (int c = 0; c < PALETTE_COLS; ++c)
            if (k_layout[r][c] == sel) { *pr = r; *pc = c; return; }
    *pr = 0; *pc = 0;
}

void ui_init(void) {
    UI.scroll_x = 20; UI.scroll_y = 16;
    UI.cur_x = UI.scroll_x + VIEW_COLS / 2;
    UI.cur_y = UI.scroll_y + VIEW_ROWS / 2;
    UI.brush = TOOL_ROAD;
    UI.mode  = UI_TITLE;       // show the title/start screen on power-on
    UI.menu_sel = 1;           // title menu: 0=load 1=easy 2=medium 3=hard
}

static void clamp_scroll(void) {
    // The status bar (top) and HUD (bottom) overlay the map, so let the vertical
    // scroll push the world's top/bottom edges right up to the visible area —
    // symmetric with how the left/right edges reach the screen sides.
    int min_y = -(STATUS_H / TILE_PX);                                   // world row 0 just below the status bar
    int max_y = WORLD_H - VIEW_ROWS + ((HUD_H + TILE_PX - 1) / TILE_PX); // world row 99 just above the HUD
    if (UI.scroll_x < 0) UI.scroll_x = 0;
    if (UI.scroll_x > MAX_SCROLL_X) UI.scroll_x = MAX_SCROLL_X;
    if (UI.scroll_y < min_y) UI.scroll_y = min_y;
    if (UI.scroll_y > max_y) UI.scroll_y = max_y;
}

// Keep the cursor inside an equal margin from the visible map on every side;
// scroll the map when it reaches the edge.
static void follow_cursor(void) {
    const int m = 2;  // tiles of clearance from the visible edge, all four sides
    if (UI.cur_x < 0) UI.cur_x = 0;
    if (UI.cur_y < 0) UI.cur_y = 0;
    if (UI.cur_x > WORLD_W - 1) UI.cur_x = WORLD_W - 1;
    if (UI.cur_y > WORLD_H - 1) UI.cur_y = WORLD_H - 1;

    if (UI.cur_x - UI.scroll_x < m)                 UI.scroll_x = UI.cur_x - m;
    if (UI.cur_x - UI.scroll_x > VIEW_COLS - 1 - m) UI.scroll_x = UI.cur_x - (VIEW_COLS - 1 - m);
    // top/bottom add the status-bar / HUD bands so the clearance matches the sides
    int top_rows = (STATUS_H / TILE_PX) + m;                  // status bar ~1 tile (floor)
    int bot_rows = ((HUD_H + TILE_PX - 1) / TILE_PX) + m;     // HUD ~4 tiles (ceil)
    if (UI.cur_y - UI.scroll_y < top_rows)                 UI.scroll_y = UI.cur_y - top_rows;
    if (UI.cur_y - UI.scroll_y > VIEW_ROWS - 1 - bot_rows) UI.scroll_y = UI.cur_y - (VIEW_ROWS - 1 - bot_rows);
    clamp_scroll();
}

// Snap the view to centre on a world tile (used to jump to a disaster).
static void center_view(int x, int y) {
    UI.cur_x = x; UI.cur_y = y;
    UI.scroll_x = x - VIEW_COLS / 2;
    UI.scroll_y = y - VIEW_ROWS / 2;
    clamp_scroll();
}

static void do_save(void) {
    uint32_t len; const void *blob = engine_state_blob(&len);
    save_write(blob, len);
}
static void do_load(void) {
    if (!save_present()) return;
    static uint8_t buf[SAVE_REGION_BYTES];
    if (save_read(buf, sizeof buf)) {
        uint32_t len; engine_state_blob(&len);
        engine_load_blob(buf, len);
    }
}

// Snapshots taken when Settings/Budget open, so A can cancel (revert) and B confirms.
static int snapTax, snapRoad, snapPol, snapFire;
static int snapNoDis, snapDiff, snapSpd, snapDoze, snapABud;
static void snap_budget(void)   { snapTax = engine_tax_rate(); snapRoad = engine_fund_pct(0);
                                  snapPol = engine_fund_pct(1); snapFire = engine_fund_pct(2); }
static void revert_budget(void) { engine_set_tax(snapTax); engine_set_fund_pct(0, snapRoad);
                                  engine_set_fund_pct(1, snapPol); engine_set_fund_pct(2, snapFire); }
static void snap_settings(void) { snapNoDis = engine_get_no_disasters(); snapDiff = engine_get_difficulty();
                                  snapSpd = engine_get_sim_speed(); snapDoze = engine_get_auto_bulldoze();
                                  snapABud = engine_get_auto_budget(); }
static void revert_settings(void){ engine_set_no_disasters(snapNoDis); engine_set_difficulty(snapDiff);
                                  engine_set_sim_speed(snapSpd); engine_set_auto_bulldoze(snapDoze);
                                  engine_set_auto_budget(snapABud); }

void ui_update(uint32_t fired) {
    if (UI.mode == UI_TITLE) {
        int n = 4;                                // 0=Load 1=Easy 2=Medium 3=Hard
        if (fired & IN_UP)   UI.menu_sel = (UI.menu_sel + n - 1) % n;
        if (fired & IN_DOWN) UI.menu_sel = (UI.menu_sel + 1) % n;
        if (fired & (IN_A | IN_B)) {
            if (UI.menu_sel == 0) {
                if (!save_present()) return;      // nothing to load yet -> ignore
                do_load();
            } else {
                engine_new_game(UI.menu_sel - 1); // 1/2/3 -> easy/med/hard (0/1/2)
            }
            UI.scroll_x = (WORLD_W - VIEW_COLS) / 2;             // centre the map
            UI.scroll_y = (WORLD_H - VIEW_ROWS) / 2;
            UI.cur_x = UI.scroll_x + VIEW_COLS / 2;
            UI.cur_y = UI.scroll_y + VIEW_ROWS / 2;
            UI.brush = TOOL_ROAD;
            UI.menu_sel = TOOL_ROAD;     // initial palette cursor; remembered afterwards
            UI.mode  = UI_INGAME;
        }
        return;
    }

    if (UI.mode == UI_INGAME) {
        if (fired & IN_LEFT)  UI.cur_x--;
        if (fired & IN_RIGHT) UI.cur_x++;
        if (fired & IN_UP)    UI.cur_y--;
        if (fired & IN_DOWN)  UI.cur_y++;
        follow_cursor();

        if (fired & IN_A) {                 // open toolbar at the last cursor position
            UI.mode = UI_TOOLBAR;
        }
        if (fired & IN_B) {                 // build with active tool (Query opens the info panel)
            if (UI.brush == TOOL_QUERY) { UI.mode = UI_SCREEN; UI.screen = SCR_QUERY; }
            else engine_apply_tool(UI.brush, UI.cur_x, UI.cur_y);
        }
    } else if (UI.mode == UI_TOOLBAR) { // palette overlay
        int r, c; sel_to_rc(UI.menu_sel, &r, &c);
        if (fired & IN_RIGHT) { int x = c, i; for (i = 0; i < PALETTE_COLS; ++i) { x = (x + 1) % PALETTE_COLS;             if (k_layout[r][x] >= 0) { UI.menu_sel = k_layout[r][x]; break; } } }
        if (fired & IN_LEFT)  { int x = c, i; for (i = 0; i < PALETTE_COLS; ++i) { x = (x + PALETTE_COLS - 1) % PALETTE_COLS; if (k_layout[r][x] >= 0) { UI.menu_sel = k_layout[r][x]; break; } } }
        if (fired & IN_DOWN)  { int y = r, i; for (i = 0; i < GRID_ROWS;    ++i) { y = (y + 1) % GRID_ROWS;                if (k_layout[y][c] >= 0) { UI.menu_sel = k_layout[y][c]; break; } } }
        if (fired & IN_UP)    { int y = r, i; for (i = 0; i < GRID_ROWS;    ++i) { y = (y + GRID_ROWS - 1) % GRID_ROWS;    if (k_layout[y][c] >= 0) { UI.menu_sel = k_layout[y][c]; break; } } }
        if (fired & IN_B) {                       // B = select entry
            int i = UI.menu_sel;
            if (ui_entry_available(i)) {          // locked entries are ignored
                switch (k_entries[i].action) {
                    case ACT_TOOL:   UI.brush = (tool_t)i; UI.mode = UI_INGAME; break;
                    case ACT_SAVE:   do_save();            UI.mode = UI_INGAME; break;
                    case ACT_LOAD:   do_load();            UI.mode = UI_INGAME; break;
                    case ACT_MAPS:   UI.mode = UI_SCREEN; UI.screen = SCR_MAPS;   break;
                    case ACT_GRAPHS: UI.mode = UI_SCREEN; UI.screen = SCR_GRAPHS; break;
                    case ACT_BUDGET: UI.mode = UI_SCREEN; UI.screen = SCR_BUDGET; UI.opt = 0; snap_budget(); break;
                    case ACT_EVAL:   UI.mode = UI_SCREEN; UI.screen = SCR_EVAL;   break;
                    case ACT_SETTINGS: UI.mode = UI_SCREEN; UI.screen = SCR_SETTINGS;  UI.opt = 0; snap_settings(); break;
                    case ACT_DISASTER: UI.mode = UI_SCREEN; UI.screen = SCR_DISASTERS; UI.opt = 0; break;
                    default:         UI.mode = UI_INGAME; break;
                }
            }
        }
        if (fired & IN_A) UI.mode = UI_INGAME;     // A opens and closes the palette
    } else { // UI_SCREEN — full-screen menu (maps / graphs / budget / evaluation)
        if (UI.screen == SCR_MAPS) {
            if (fired & IN_LEFT)  UI.map_type = (UI.map_type + 5) % 6;
            if (fired & IN_RIGHT) UI.map_type = (UI.map_type + 1) % 6;
        } else if (UI.screen == SCR_BUDGET) {
            if (fired & IN_UP)   UI.opt = (UI.opt + 3) % 4;   // rows: 0 TAX, 1 ROAD, 2 POLICE, 3 FIRE
            if (fired & IN_DOWN) UI.opt = (UI.opt + 1) % 4;
            int d = (fired & IN_RIGHT) ? 1 : (fired & IN_LEFT) ? -1 : 0;
            if (d) {
                if (UI.opt == 0) engine_set_tax(engine_tax_rate() + d);
                else { int w = UI.opt - 1; engine_set_fund_pct(w, engine_fund_pct(w) + d * 10); }
            }
            if (fired & IN_A)      { revert_budget(); UI.mode = UI_TOOLBAR; }  // A: back without saving
            else if (fired & IN_B) { UI.mode = UI_TOOLBAR; }                  // B: OK (keep changes)
            return;
        } else if (UI.screen == SCR_SETTINGS) {
            if (fired & IN_UP)   UI.opt = (UI.opt + 4) % 5;
            if (fired & IN_DOWN) UI.opt = (UI.opt + 1) % 5;
            if (fired & (IN_LEFT | IN_RIGHT)) {
                int d = (fired & IN_RIGHT) ? 1 : -1;
                switch (UI.opt) {
                    case 0: engine_set_no_disasters(!engine_get_no_disasters());   break;
                    case 1: engine_set_difficulty(engine_get_difficulty() + d);    break;
                    case 2: engine_set_sim_speed(engine_get_sim_speed() + d);      break;
                    case 3: engine_set_auto_bulldoze(!engine_get_auto_bulldoze()); break;
                    case 4: engine_set_auto_budget(!engine_get_auto_budget());     break;
                }
            }
            if (fired & IN_A)      { revert_settings(); UI.mode = UI_TOOLBAR; }  // A: back without saving
            else if (fired & IN_B) { UI.mode = UI_TOOLBAR; }                    // B: OK (keep changes)
            return;
        } else if (UI.screen == SCR_DISASTERS) {
            static const int dmap[7] = { DIS_FIRE, DIS_FLOOD, DIS_TORNADO, DIS_QUAKE,
                                         DIS_MELTDOWN, DIS_MONSTER, DIS_CRASH };
            if (fired & IN_UP)   UI.opt = (UI.opt + 6) % 7;
            if (fired & IN_DOWN) UI.opt = (UI.opt + 1) % 7;
            if (fired & IN_A) UI.mode = UI_TOOLBAR;                                      // A: back to palette
            if (fired & IN_B) {                 // B: trigger, then jump the view to it (watch)
                int j0 = engine_jump_event(0, 0);
                engine_trigger_disaster(dmap[UI.opt]);
                { int jx, jy; if (engine_jump_event(&jx, &jy) != j0) center_view(jx, jy); }
                UI.mode = UI_INGAME;
            }
            return;   // A/B already consumed; skip generic close
        } else if (UI.screen == SCR_QUERY) {
            if (fired & (IN_A | IN_B)) UI.mode = UI_INGAME;   // back to the map/cursor
            return;
        }
        if (fired & (IN_A | IN_B)) UI.mode = UI_TOOLBAR;  // close screen -> back to the palette/HUD
    }
}

uint16_t ui_tool_swatch(int tool) {
    switch (tool) {
        case TOOL_BULLDOZER:   return 0x8410; // grey
        case TOOL_ROAD:        return 0x9CD3;
        case TOOL_RAIL:        return 0x7BEF;
        case TOOL_WIRE:        return 0xFFE0;
        case TOOL_RESIDENTIAL: return 0x5DCB;
        case TOOL_COMMERCIAL:  return 0x35BF;
        case TOOL_INDUSTRIAL:  return 0xEDA6;
        case TOOL_PARK:        return 0x3666;
        case TOOL_POLICE:      return 0x041F;
        case TOOL_FIRE:        return 0xF800;
        case TOOL_POWERPLANT:  return 0xC618;
        default:               return 0xFFFF;
    }
}
