#pragma once
#include <stdint.h>
#include <stdbool.h>

// =====================================================================
//  Engine seam
//
//  render.c and ui.c talk ONLY to this interface — never to Micropolis
//  internals. The backend that implements it:
//
//    engine_micropolis.c — wraps the vendored s_*.c core: reads
//                          Map[x][y] & LOMASK for tiles, PWRBIT for power,
//                          TotalFunds / CityTime / R-C-IValve for the HUD,
//                          and calls the tool functions for build actions.
//
//  Keeping the boundary this thin is what makes swapping the backend a
//  one-file change.
// =====================================================================

// ---- Tool / brush identifiers (UI-facing) ----------------------------
typedef enum {
    TOOL_BULLDOZER = 0,
    TOOL_ROAD,
    TOOL_RAIL,
    TOOL_WIRE,
    TOOL_RESIDENTIAL,
    TOOL_COMMERCIAL,
    TOOL_INDUSTRIAL,
    TOOL_PARK,
    TOOL_POLICE,
    TOOL_FIRE,
    TOOL_POWERPLANT,
    TOOL_NUCLEAR,
    TOOL_SEAPORT,
    TOOL_AIRPORT,
    TOOL_STADIUM,
    TOOL_QUERY,
    TOOL_COUNT
} tool_t;

// ---- Lifecycle --------------------------------------------------------
void engine_init(void);     // new game / generate map
void engine_new_game(int level);  // 0=easy 1=medium 2=hard: set difficulty + fresh map
void engine_tick(void);

/* --- Dual-core (RP2350): the simulation runs on core1, the UI/render on
 *     core0. core_sync_start() launches core1; engine_lock/unlock serialize
 *     core0's bulk mutators (build/disaster/new game/save) against the sim.
 *     On the host build these are no-ops / unused. --- */
void core_sync_start(void);
void engine_lock(void);
void engine_unlock(void);
void engine_animate_tiles(void);   // advance ANIMBIT tiles (traffic cars, etc.)     // advance the simulation one step

// Tile inspection (Query tool). All level fields are 0..3.
typedef struct {
    int zone;       // 0..27 index into the zone-name table
    int density;    // population density:  Low/Medium/High/Very High
    int value;      // land value:          Slum/Lower/Middle/High
    int crime;      // crime rate:          Safe/Light/Moderate/Dangerous
    int pollution;  // pollution:           None/Moderate/Heavy/Very Heavy
    int growth;     // growth rate:         Declining/Stable/Slow/Fast
} query_t;
void engine_query(int x, int y, query_t *q);

// ---- Map queries (called per visible tile, must be cheap) ------------
uint16_t engine_tile(int x, int y);   // returns the tile graphic index
bool     engine_powered(int x, int y);

// ---- HUD data ---------------------------------------------------------
long engine_funds(void);
int  engine_year(void);
int  engine_month(void);    // 0..11
// RCI demand, each in range -32..+32 (negative = oversupply).
int  engine_demand_r(void);
int  engine_demand_c(void);
int  engine_demand_i(void);

// City-condition readouts driving the status LED (crime = red/blue, traffic
// = amber). Original thresholds: crime alert > 100, traffic alert > 60.
int  engine_crime_average(void);
int  engine_traffic_average(void);
// Advisory-event counters: increment each time the crime / heavy-traffic
// message fires (as in the original, only now and then).
int  engine_crime_events(void);
int  engine_traffic_events(void);
long engine_population(void);   // city population (for unlocking reward tools)

// ---- Actions ----------------------------------------------------------
// Apply the given tool at map tile (x,y). Returns the cost charged, or
// -1 if the action was rejected (insufficient funds / illegal tile).
int  engine_apply_tool(tool_t tool, int x, int y);

// Footprint of a tool in tiles (1, 3, 4 or 6). Larger tools build from the
// tile above-left of (x,y), matching Micropolis' check3x3/4x4/6x6.
int  engine_tool_size(tool_t tool);

// ---- Menu screens: read-only data (Evaluation / Graphs / Maps / Budget) ----
long engine_eval_pop(void);
int  engine_eval_class(void);          // 0..5 (Village..Megalopolis)
int  engine_eval_score(void);          // 0..1000
int  engine_eval_score_delta(void);
int  engine_eval_approval(void);       // 0..100 (% yes votes)

int  engine_tax_rate(void);            // 0..20
void engine_set_tax(int rate);         // clamps 0..20
long engine_tax_income(void);
int  engine_fund_pct(int which);
void engine_set_fund_pct(int which, int pct);       // which: 0=road 1=police 2=fire -> 0..100
long engine_fund_req(int which);       // requested funding amount

int  engine_history(int which, short *out, int max);  // 0=res 1=com 2=ind 3=money 4=crime 5=pollution; newest first
void engine_overlay_dims(int *w, int *h);
int  engine_overlay(int type, int hx, int hy);        // type 0..5 -> 0..255

// ---- Persistence (raw blob; main.c routes it to save.c) --------------
const void *engine_state_blob(uint32_t *out_len);  // pointer + length
void        engine_load_blob(const void *blob, uint32_t len);

// ---- Disasters & settings -------------------------------------------
enum { DIS_FIRE = 0, DIS_FLOOD, DIS_TORNADO, DIS_QUAKE, DIS_MELTDOWN, DIS_MONSTER, DIS_CRASH, DIS_COUNT };
void engine_trigger_disaster(int type);
// View-jump: counter increments on each triggered disaster; fills the tile to
// centre the view on so the player can watch it.
int  engine_jump_event(int *x, int *y);
// Active sprites for rendering (disasters + ambient vehicles).
enum { SPR_GOD = 0, SPR_TOR, SPR_AIR, SPR_SHI, SPR_COP, SPR_TRA, SPR_EXP };
int  engine_sprite_count(void);        // number of sprite slots
int  engine_sprite_type(int i);        // SPR_* or -1 if the slot is empty
int  engine_sprite_x(int i);           // world pixels (tile << 4)
int  engine_sprite_y(int i);
int  engine_sprite_frame(int i);       // image index within the type's frame set
int  engine_zone_powerless(int x, int y);   // 1 if (x,y) is a zone centre lacking power

int  engine_get_no_disasters(void);    void engine_set_no_disasters(int on);   // 1 = disasters disabled
int  engine_get_difficulty(void);      void engine_set_difficulty(int level);  // 0=easy 1=med 2=hard
int  engine_get_sim_speed(void);       void engine_set_sim_speed(int spd);      // 1=slow 2=normal 3=fast
int  engine_get_auto_bulldoze(void);   void engine_set_auto_bulldoze(int on);
int  engine_get_auto_budget(void);     void engine_set_auto_budget(int on);
