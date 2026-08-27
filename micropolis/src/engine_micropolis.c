/* engine_micropolis.c — implements the engine.h seam over the vendored
 * Micropolis simulation core (vendor/micropolis/sim, built with -DHEADLESS).
 *
 * render.c / ui.c are unchanged: they call engine_tile/funds/etc. and never
 * see Micropolis internals. This is the real engine backend used by the build.
 *
 * Verified to compile + link + run on the host harness (tools/host_sim_test.c).
 */
#include "engine.h"
#include <string.h>

/* Cross-core serialization: on the embedded target the simulation runs on
 * core1 and these bulk-state mutators are called from core0 (UI/build). The
 * lock (implemented in core_sync.c) serializes them against engine_tick().
 * On the host build (no PICO_ON_DEVICE) it compiles away to nothing. */
#ifdef PICO_ON_DEVICE
#define ENGINE_LOCK()   engine_lock()
#define ENGINE_UNLOCK() engine_unlock()
#else
#define ENGINE_LOCK()   ((void)0)
#define ENGINE_UNLOCK() ((void)0)
#endif

#ifndef HEADLESS
#define HEADLESS 1
#endif
#include "sim.h"          /* from vendor/micropolis/sim/headers */

/* core entry points (defined in the vendored s_*.c) */
extern void initMapArrays(void);
extern void GenerateNewCity(void);
extern void SimFrame(void);

/* user funding targets (0..1), defined in micropolis_glue.c */
extern float RoadPctTarget, FirePctTarget, PolicePctTarget;

/* model globals (Map, valves, time defined in the core; funds/level/flags
 * in micropolis_glue.c) — all declared by sim.h */

void engine_init(void) {
    /* a zeroed Sim satisfies the headless code paths (sim->map/editor/sprite
     * lists are only walked by the stubbed GUI hooks) */
    static Sim sim_storage;
    memset(&sim_storage, 0, sizeof sim_storage);
    sim = &sim_storage;

    GameLevel    = 0;
    SimSpeed     = 3;          /* 0 = paused; 1..3 = slow..fast */
    autoBulldoze = 1;
    autoBudget   = 1;
    RoadPctTarget = FirePctTarget = PolicePctTarget = 1.0f;
    roadPercent = firePercent = policePercent = 1.0f;
    NoDisasters  = 1;          /* random disasters off by default */
    CityTax      = 7;
    StartingYear = 1900;
    TotalFunds   = 20000;

    initMapArrays();
    GenerateNewCity();     /* procedural terrain (land, water, trees) */
}

/* Start a fresh city with a difficulty level: 0=easy 1=medium 2=hard.
   Difficulty sets the starting treasury and the disaster frequency (GameLevel). */
void engine_new_game(int level) {
    if (level < 0) level = 0;
    if (level > 2) level = 2;
    ENGINE_LOCK();
    GameLevel    = level;
    SimSpeed     = 3;
    autoBulldoze = 1;
    autoBudget   = 1;
    RoadPctTarget = FirePctTarget = PolicePctTarget = 1.0f;
    roadPercent = firePercent = policePercent = 1.0f;
    NoDisasters  = 1;          /* random disasters off by default */
    CityTax      = 7;
    StartingYear = 1900;
    CityTime     = 0;
    TotalFunds   = (level == 0) ? 20000 : (level == 1) ? 10000 : 5000;
    GenerateNewCity();
    ENGINE_UNLOCK();
}

/* disaster + sprite-effect hooks (in micropolis_glue.c / s_disast.c) */
extern void SetFire(void), MakeFlood(void), MakeEarthquake(void), MakeMeltdown(void);
extern void MakeTornado(void), MakeMonster(void), MakeExplosion(int, int), StepDisaster(void);
extern void MakeCityExplosion(void);
extern int  SprCount(void), SprType(int), SprX(int), SprY(int), SprFrame(int);

void engine_tick(void) {
    static int aniclk = 0;
    SimFrame();
    StepDisaster();
    if (SimSpeed && ++aniclk >= 10) {   /* animate traffic/etc. at 20% of frame rate while running */
        aniclk = 0;
        engine_animate_tiles();
    }
}

/* Location of the last user-triggered disaster, so the view can jump there to
 * watch it. Bumped only from engine_trigger_disaster (not random disasters, so
 * the camera never yanks itself away while you're building). */
static int g_jumpX = 0, g_jumpY = 0, g_jumpEvt = 0;

void engine_trigger_disaster(int type) {
    extern short LastPicNum;              /* let the located message (re)fire   */
    extern volatile int MesLocEvt;
    int lx = -1, ly = -1, i, n, want, loc0;
    ENGINE_LOCK();
    LastPicNum = 0;
    loc0 = MesLocEvt;
    switch (type) {
        case DIS_FIRE:     SetFire();           break;
        case DIS_FLOOD:    MakeFlood();         break;
        case DIS_QUAKE:    MakeEarthquake();    break;
        case DIS_MELTDOWN: MakeMeltdown();      break;   /* needs a nuclear plant */
        case DIS_CRASH:    MakeCityExplosion(); break;
        case DIS_TORNADO:  MakeTornado();       break;
        case DIS_MONSTER:  MakeMonster();       break;
        default: break;
    }
    /* Where did it happen? Monster/tornado are roaming sprites; the others post
     * their tile via MesX/MesY — but only trust those if a located message
     * actually fired for THIS disaster (else they'd be stale from a prior one). */
    if (type == DIS_MONSTER || type == DIS_TORNADO) {
        want = (type == DIS_MONSTER) ? SPR_GOD : SPR_TOR;
        n = engine_sprite_count();
        for (i = 0; i < n; ++i)
            if (engine_sprite_type(i) == want) {
                lx = engine_sprite_x(i) >> 4;   /* pixels -> tile */
                ly = engine_sprite_y(i) >> 4;
                break;
            }
    } else if (MesLocEvt != loc0) {
        lx = MesX; ly = MesY;
    }
    if (lx >= 0) { g_jumpX = lx; g_jumpY = ly; g_jumpEvt++; }
    ENGINE_UNLOCK();
}

/* Returns a counter that increments on each triggered disaster and fills the
 * tile to centre on. UI compares the counter to detect a new event. */
int engine_jump_event(int *x, int *y) {
    if (x) *x = g_jumpX;
    if (y) *y = g_jumpY;
    return g_jumpEvt;
}
int engine_sprite_count(void)   { return SprCount(); }
int engine_sprite_type(int i)   { return SprType(i); }
int engine_sprite_x(int i)      { return SprX(i); }
int engine_sprite_y(int i)      { return SprY(i); }
int engine_sprite_frame(int i)  { return SprFrame(i); }

int  engine_get_no_disasters(void)    { return NoDisasters; }
void engine_set_no_disasters(int on)  { NoDisasters = on ? 1 : 0; }
int  engine_get_difficulty(void)      { return GameLevel; }
void engine_set_difficulty(int l)     { if (l < 0) l = 0; if (l > 2) l = 2; GameLevel = (short)l; }
int  engine_get_sim_speed(void)       { return SimSpeed; }
void engine_set_sim_speed(int s)      { if (s < 1) s = 1; if (s > 3) s = 3; SimSpeed = (short)s; }
int  engine_get_auto_bulldoze(void)   { return autoBulldoze; }
void engine_set_auto_bulldoze(int on) { autoBulldoze = on ? 1 : 0; }
int  engine_get_auto_budget(void)     { return autoBudget; }
void engine_set_auto_budget(int on)   { autoBudget = on ? 1 : 0; }

uint16_t engine_tile(int x, int y) {
    if ((unsigned)x >= WORLD_X || (unsigned)y >= WORLD_Y) return 0;
    return (uint16_t)(Map[x][y] & LOMASK);
}

void engine_query(int x, int y, query_t *q) {
    static const short idArray[28] = {
        DIRT, RIVER, TREEBASE, RUBBLE, FLOOD, RADTILE, FIRE, ROADBASE,
        POWERBASE, RAILBASE, RESBASE, COMBASE, INDBASE, PORTBASE, AIRPORTBASE, COALBASE,
        FIRESTBASE, POLICESTBASE, STADIUMBASE, NUCLEARBASE, 827, 832, FOUNTAIN, INDBASE2,
        FOOTBALLGAME1, VBRDG0, 952, 956 };
    int t, i, z, hx, hy, sx, sy;
    q->zone = q->density = q->value = q->crime = q->pollution = q->growth = 0;
    if ((unsigned)x >= WORLD_X || (unsigned)y >= WORLD_Y) return;
    t = Map[x][y] & LOMASK;
    if (t >= COALSMOKE1 && t < FOOTBALLGAME1) t = COALBASE;   /* animated smoke -> coal plant */
    for (i = 1; i < 28 && t >= idArray[i]; ++i) ;            /* first idArray[i] > t */
    q->zone = i - 1;                                          /* 0..27 */

    hx = x >> 1; hy = y >> 1;                                 /* 2x2 data maps (60x50) */
    if (hx >= HWLDX) hx = HWLDX - 1;
    if (hy >= HWLDY) hy = HWLDY - 1;
    z = PopDensity[hx][hy];   q->density = (z >> 6) & 3;
    z = LandValueMem[hx][hy]; q->value   = z < 30 ? 0 : z < 80 ? 1 : z < 150 ? 2 : 3;
    z = CrimeMem[hx][hy];     q->crime   = (z >> 6) & 3;
    z = PollutionMem[hx][hy]; q->pollution = (z == 0) ? 0 : z < 128 ? 1 : z < 192 ? 2 : 3;

    sx = x >> 3; sy = y >> 3;                                 /* 8x8 growth map */
    if (sx >= SmX) sx = SmX - 1;
    if (sy >= SmY) sy = SmY - 1;
    z = RateOGMem[sx][sy];    q->growth  = z < 0 ? 0 : z == 0 ? 1 : z > 100 ? 3 : 2;
}

bool engine_powered(int x, int y) {
    if ((unsigned)x >= WORLD_X || (unsigned)y >= WORLD_Y) return false;
    return (Map[x][y] & PWRBIT) != 0;
}

int engine_zone_powerless(int x, int y) {
    short t;
    if ((unsigned)x >= WORLD_X || (unsigned)y >= WORLD_Y) return 0;
    t = Map[x][y];
    return ((t & ZONEBIT) && !(t & PWRBIT)) ? 1 : 0;   /* zone centre without power */
}

long engine_funds(void) { return (long)TotalFunds; }
int  engine_year(void)  { return (int)(StartingYear + CityTime / 48); }
int  engine_month(void) { return (int)((CityTime % 48) >> 2); }

/* Micropolis valves clamp to roughly +/-2000; map to the HUD's +/-32. */
static int scale_valve(int v) {
    v /= 64;
    if (v >  32) v =  32;
    if (v < -32) v = -32;
    return v;
}
int engine_demand_r(void) { return scale_valve(RValve); }
int engine_demand_c(void) { return scale_valve(CValve); }
int engine_demand_i(void) { return scale_valve(IValve); }
long engine_population(void) { return (long)CityPop; }

// City-condition readouts for the status LED (see led.c). Plain reads of sim
// globals; a torn read only mis-times a blink by one frame.
int engine_crime_average(void)   { return (int)CrimeAverage; }
int engine_traffic_average(void) { return (int)TrafficAverage; }

// Advisory-event counters (bumped in SendMes exactly when the original fires
// the crime siren / traffic honk). led.c blinks on each increment.
extern volatile int MesCrimeEvt, MesTrafficEvt;
int engine_crime_events(void)   { return MesCrimeEvt; }
int engine_traffic_events(void) { return MesTrafficEvt; }

// Footprint per tool (tiles). 3x3 zones/services, 4x4 plants/port/stadium,
// 6x6 airport, everything else 1x1.
int engine_tool_size(tool_t t) {
    static const unsigned char sz[TOOL_COUNT] = {
        1, /*BULLDOZER*/ 1, /*ROAD*/ 1, /*RAIL*/ 1, /*WIRE*/
        3, /*RES*/ 3, /*COM*/ 3, /*IND*/ 1, /*PARK*/
        3, /*POLICE*/ 3, /*FIRE*/ 4, /*POWERPLANT*/ 4, /*NUCLEAR*/
        4, /*SEAPORT*/ 6, /*AIRPORT*/ 4, /*STADIUM*/ 1, /*QUERY*/
    };
    if ((unsigned)t >= TOOL_COUNT) return 1;
    return sz[t];
}

// ---- Menu screens ----------------------------------------------------
long engine_eval_pop(void)         { return (long)CityPop; }
int  engine_eval_class(void)       { return CityClass; }
int  engine_eval_score(void)       { return CityScore; }
int  engine_eval_score_delta(void) { return deltaCityScore; }
int  engine_eval_approval(void)    { int t = CityYes + CityNo; return t ? (CityYes * 100 / t) : 50; }

int  engine_tax_rate(void)         { return CityTax; }
void engine_set_tax(int r)         { if (r < 0) r = 0; if (r > 20) r = 20; CityTax = (short)r; }
long engine_tax_income(void)       { return (long)TaxFund; }
int  engine_fund_pct(int w) {
    float p = (w == 0) ? RoadPctTarget : (w == 1) ? PolicePctTarget : FirePctTarget;
    return (int)(p * 100.0f + 0.5f);
}
void engine_set_fund_pct(int w, int pct) {
    if (pct < 0) pct = 0; if (pct > 100) pct = 100;
    float p = pct / 100.0f;
    if (w == 0) RoadPctTarget = p; else if (w == 1) PolicePctTarget = p; else FirePctTarget = p;
}
long engine_fund_req(int w) {
    return (long)((w == 0) ? RoadFund : (w == 1) ? PoliceFund : FireFund);
}

int engine_history(int which, short *out, int max) {
    short *h = (which == 0) ? ResHis : (which == 1) ? ComHis : (which == 2) ? IndHis :
               (which == 3) ? MoneyHis : (which == 4) ? CrimeHis : PollutionHis;
    int n = 120, i;
    if (n > max) n = max;
    if (!h) { for (i = 0; i < n; ++i) out[i] = 0; return n; }
    for (i = 0; i < n; ++i) out[i] = h[i];   // index 0 = most recent
    return n;
}
void engine_overlay_dims(int *w, int *h) { *w = HWLDX; *h = HWLDY; }
int  engine_overlay(int type, int hx, int hy) {
    if ((unsigned)hx >= HWLDX || (unsigned)hy >= HWLDY) return 0;
    switch (type) {
        case 0: return PopDensity[hx][hy];
        case 1: return CrimeMem[hx][hy];
        case 2: return PollutionMem[hx][hy];
        case 3: return LandValueMem[hx][hy];
        case 4: return TrfDensity[hx][hy];
        case 5: return (Map[hx * 2][hy * 2] & PWRBIT) ? 255 : 0;  // power
        default: return 0;
    }
}

/* Real tool application via the vendored w_tool.c / w_con.c: correct 3x3 /
 * 4x4 / 6x6 footprints, road/rail/wire auto-connection, and cost deduction
 * (Spend -> TotalFunds). DoTool takes map coordinates. */
extern int DoTool();
static SimView g_toolview;   /* zeroed; the tool path only uses it for the
                                stubbed message/sound hooks */

int engine_apply_tool(tool_t tool, int x, int y) {
    static const short to_state[TOOL_COUNT] = {
        dozeState,         /* TOOL_BULLDOZER  */
        roadState,         /* TOOL_ROAD       */
        rrState,           /* TOOL_RAIL       */
        wireState,         /* TOOL_WIRE       */
        residentialState,  /* TOOL_RESIDENTIAL*/
        commercialState,   /* TOOL_COMMERCIAL */
        industrialState,   /* TOOL_INDUSTRIAL */
        parkState,         /* TOOL_PARK       */
        policeState,       /* TOOL_POLICE     */
        fireState,         /* TOOL_FIRE       */
        powerState,        /* TOOL_POWERPLANT */
        nuclearState,      /* TOOL_NUCLEAR    */
        seaportState,      /* TOOL_SEAPORT    */
        airportState,      /* TOOL_AIRPORT    */
        stadiumState,      /* TOOL_STADIUM    */
        queryState,        /* TOOL_QUERY      */
    };
    if (tool < 0 || tool >= TOOL_COUNT) return -1;
    if ((unsigned)x >= WORLD_X || (unsigned)y >= WORLD_Y) return -1;
    {
        int r;
        ENGINE_LOCK();
        r = DoTool(&g_toolview, to_state[tool], (short)x, (short)y);
        ENGINE_UNLOCK();
        return r;
    }
}

/* ---- save / load --------------------------------------------------------
 * Replicates Micropolis's own .cty layout: six history arrays + MiscHis +
 * the Map (column-major), 27120 bytes total. Funds/time/flags/percents are
 * packed into MiscHis exactly where the upstream s_fileio.c keeps them, so
 * the data is faithful — but we read and write on the same little-endian
 * RP2350, so no byte-swap is needed. The Map is copied column-by-column
 * (works whether or not the columns are one contiguous allocation). */
#define MP_H   (HISTLEN / 2)        /* 240 shorts per history array */
#define MP_MH  (MISCHISTLEN / 2)    /* 120 shorts of MiscHis        */
#define MP_MAP (WORLD_X * WORLD_Y)  /* 12000 map cells              */

static short s_blob[6 * MP_H + MP_MH + MP_MAP];   /* 13560 shorts = 27120 B */

static void pack_misc(void) {
    *(int32_t *)(MiscHis + 50) = TotalFunds;
    *(int32_t *)(MiscHis +  8) = (QUAD)CityTime;
    MiscHis[52] = autoBulldoze;
    MiscHis[53] = autoBudget;
    MiscHis[54] = autoGo;
    MiscHis[55] = UserSoundOn;
    MiscHis[56] = CityTax;
    MiscHis[57] = SimSpeed;
    *(int32_t *)(MiscHis + 58) = (QUAD)(PolicePctTarget * 65536.0);
    *(int32_t *)(MiscHis + 60) = (QUAD)(FirePctTarget   * 65536.0);
    *(int32_t *)(MiscHis + 62) = (QUAD)(RoadPctTarget   * 65536.0);
    MiscHis[15] = GameLevel;         /* canonical slot the sim reloads on load */
    MiscHis[64] = NoDisasters;       /* free slot: persist the disasters toggle */
}

const void *engine_state_blob(uint32_t *out_len) {
    short *p = s_blob;
    int x;
    ENGINE_LOCK();
    pack_misc();
    memcpy(p, ResHis,       MP_H * 2); p += MP_H;
    memcpy(p, ComHis,       MP_H * 2); p += MP_H;
    memcpy(p, IndHis,       MP_H * 2); p += MP_H;
    memcpy(p, CrimeHis,     MP_H * 2); p += MP_H;
    memcpy(p, PollutionHis, MP_H * 2); p += MP_H;
    memcpy(p, MoneyHis,     MP_H * 2); p += MP_H;
    memcpy(p, MiscHis,      MP_MH * 2); p += MP_MH;
    for (x = 0; x < WORLD_X; ++x) { memcpy(p, Map[x], WORLD_Y * 2); p += WORLD_Y; }
    *out_len = (uint32_t)((p - s_blob) * sizeof(short));
    ENGINE_UNLOCK();
    return s_blob;
}

void engine_load_blob(const void *blob, uint32_t len) {
    const short *p = (const short *)blob;
    int x;
    if (len < (uint32_t)(sizeof s_blob)) return;   /* not a full city */
    ENGINE_LOCK();

    memcpy(ResHis,       p, MP_H * 2); p += MP_H;
    memcpy(ComHis,       p, MP_H * 2); p += MP_H;
    memcpy(IndHis,       p, MP_H * 2); p += MP_H;
    memcpy(CrimeHis,     p, MP_H * 2); p += MP_H;
    memcpy(PollutionHis, p, MP_H * 2); p += MP_H;
    memcpy(MoneyHis,     p, MP_H * 2); p += MP_H;
    memcpy(MiscHis,      p, MP_MH * 2); p += MP_MH;
    for (x = 0; x < WORLD_X; ++x) { memcpy(Map[x], p, WORLD_Y * 2); p += WORLD_Y; }

    TotalFunds   = *(int32_t *)(MiscHis + 50);
    CityTime     = *(int32_t *)(MiscHis +  8);
    autoBulldoze = MiscHis[52];
    autoBudget   = MiscHis[53];
    autoGo       = MiscHis[54];
    UserSoundOn  = MiscHis[55];
    CityTax      = MiscHis[56];
    SimSpeed     = MiscHis[57];
    PolicePctTarget = *(int32_t *)(MiscHis + 58) / 65536.0;
    FirePctTarget   = *(int32_t *)(MiscHis + 60) / 65536.0;
    RoadPctTarget   = *(int32_t *)(MiscHis + 62) / 65536.0;
    policePercent = PolicePctTarget; firePercent = FirePctTarget; roadPercent = RoadPctTarget;
    if (CityTax < 0 || CityTax > 20) CityTax = 7;
    if (SimSpeed < 0 || SimSpeed > 3) SimSpeed = 3;
    NoDisasters = MiscHis[64] ? 1 : 0;   /* GameLevel is restored by DoSimInit (MiscHis[15]) */

    /* rebuild derived state (power grid, census, valves) */
    InitSimLoad   = 1;
    DoInitialEval = 0;
    ScenarioID    = 0;
    InitWillStuff();
    DoSimInit();

    /* re-assert the saved funds/time in case init perturbed them */
    TotalFunds = *(int32_t *)(MiscHis + 50);
    CityTime   = *(int32_t *)(MiscHis +  8);
    ENGINE_UNLOCK();
}
