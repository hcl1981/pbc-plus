#pragma once
#include <stdint.h>
#include "engine.h"

enum { UI_INGAME = 0, UI_TOOLBAR, UI_SCREEN, UI_TITLE };
enum { SCR_MAPS = 0, SCR_GRAPHS, SCR_BUDGET, SCR_EVAL, SCR_SETTINGS, SCR_DISASTERS, SCR_QUERY };

typedef struct {
    int    scroll_x, scroll_y;   // top-left visible tile
    int    cur_x, cur_y;         // selected map tile
    tool_t brush;                // active tool
    int    mode;                 // UI_INGAME / UI_TOOLBAR / UI_SCREEN
    int    menu_sel;             // highlighted tool in the toolbar
    int    screen;               // active SCR_* when mode == UI_SCREEN
    int    map_type;             // overlay shown on the maps screen (0..5)
    int    opt;                  // selected row on the settings/disasters screen
} ui_t;

extern ui_t UI;

void ui_init(void);

// Consume this frame's fired-input bitmask (from input_poll) and update
// state: move cursor / scroll, open the toolbar (A), build (B),
// pick a tool, or trigger save/load.
void ui_update(uint32_t fired);

// Representative RGB565 swatch colour for a tool (used by the HUD).
uint16_t ui_tool_swatch(int tool);

// Palette = tools followed by system actions (Save, Load). render.c reads
// these to draw the icon strip + the selected entry's name and cost.
int         ui_entry_count(void);
const char *ui_entry_name(int i);
long        ui_entry_cost(int i);        // -1 = no cost (menu action)
int         ui_entry_icon(int i);        // index into kToolIcons (tool_icon)
bool        ui_entry_available(int i);   // false = locked tool (draw greyed, not selectable)

// Palette grid layout: rows count, and the entry index at (row,col) or -1 if empty.
int         ui_grid_rows(void);
int         ui_grid_cell(int row, int col);
