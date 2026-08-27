/* micropolis_glue.c — headless glue for the Micropolis sim core.
 * Defines the model globals and stubs the X11/Tk/sound/sprite/graph
 * hooks the s_*.c expect from the (removed) frontend. Auto-derived from
 * the core's external-symbol set; verified to link + run on host.
 * Allocators (ckalloc/NewPtr) and TickCount have real behaviour. */

#include "sim.h"
#include "animtab.h"          /* short aniTile[1024] — traffic/animation frame table */
#include <stdlib.h>
#include <string.h>

/* model globals normally defined by the upstream X11/Tk frontend */
char * CityFileName;
short CrashX;
short CrashX, CrashY;
/* Bumped by SendMes() each time the crime (msg 11) / heavy-traffic (msg 12)
 * advisory actually fires — i.e. exactly when the original plays its siren /
 * honk. The status LED (led.c) watches these to blink only now and then. */
volatile int MesCrimeEvt = 0;
volatile int MesTrafficEvt = 0;
/* Bumped by SendMesAt() whenever a *located* message actually fires (disasters
 * post their tile this way); engine_trigger_disaster uses it to tell whether
 * MesX/MesY are fresh for THIS disaster. */
volatile int MesLocEvt = 0;
short GameLevel;
short Graph10Max;
short Graph10Max, Graph120Max;
short InitSimLoad;
QUAD LastR, LastC;
QUAD LastCityMonth;
QUAD LastCityTime;
QUAD LastCityYear;
QUAD LastFunds;
QUAD LastR, LastC, LastI;
QUAD LastMesTime;
QUAD LastR;
short MesNum;
short MustUpdateOptions;
short NewGraph;
short NoDisasters;
int OverRide;
int PendingTool;
char *HomeDir, * ResourceDir;
short ScenarioID;
short SimSpeed;
QUAD TotalFunds;
int UpdateDelayed;
short UserSoundOn;
short autoBudget;
short autoBulldoze;
float roadPercent, policePercent, firePercent;
float roadPercent, policePercent;
float roadPercent;
Sim * sim;
struct timeval start_time;

/* allocators / timing (real behaviour) */
char *ckalloc(unsigned n){return (char*)malloc(n);}
void ckfree(void*p){if(p)free(p);}
Ptr NewPtr(long n){return (Ptr)malloc(n);}
SimSprite *GetSprite(){return 0;}
long TickCount(){return 0;}

/* frontend hooks with header prototypes */
int SetGameLevel(short level) { return (int)0; }
int setSkips(int skips) { return (int)0; }
int setSpeed(short speed) { return (int)0; }

/* frontend hooks without prototypes (void no-ops) */
void ChangeCensus() {}
void ChangeEval() {}
void DestroyAllSprites() {}
/* Auto-budget: collect this year's tax, fund road/police/fire as far as the
   treasury allows, set the funding percentages, and update the service
   effects (RoadEffect/PoliceEffect/FireEffect) the simulation reads.
   CollectTax() has already filled TaxFund and the *Fund requests and only
   calls this when TotalPop > 0. */
/* User funding targets (0..1); the budget screen sets these. Default = full. */
float RoadPctTarget = 1.0f, FirePctTarget = 1.0f, PolicePctTarget = 1.0f;

void DoBudget(void) {
    QUAD wantRoad   = (QUAD)(RoadFund   * RoadPctTarget);
    QUAD wantFire   = (QUAD)(FireFund   * FirePctTarget);
    QUAD wantPolice = (QUAD)(PoliceFund * PolicePctTarget);
    QUAD total = wantRoad + wantFire + wantPolice;
    QUAD avail = (QUAD)TotalFunds + TaxFund;        /* spendable this year */
    if (total <= 0 || avail >= total) {             /* targets fit the treasury */
        RoadSpend = wantRoad; FireSpend = wantFire; PoliceSpend = wantPolice;
    } else {                                        /* not enough money: scale down */
        double p = (avail > 0) ? (double)avail / (double)total : 0.0;
        RoadSpend   = (QUAD)(wantRoad   * p);
        FireSpend   = (QUAD)(wantFire   * p);
        PoliceSpend = (QUAD)(wantPolice * p);
    }
    /* effects reflect what was actually spent */
    roadPercent   = RoadFund   ? (float)RoadSpend   / (float)RoadFund   : RoadPctTarget;
    policePercent = PoliceFund ? (float)PoliceSpend / (float)PoliceFund : PolicePctTarget;
    firePercent   = FireFund   ? (float)FireSpend   / (float)FireFund   : FirePctTarget;
    TotalFunds += TaxFund - (RoadSpend + FireSpend + PoliceSpend);
    UpdateFundEffects();                            /* RoadEffect/PoliceEffect/FireEffect */
}
void DoEarthQuake() {}
void DoNewGame() {}
void DoUpdateHeads() {}
void DropFireBombs() {}
void Eval() {}
void GenerateCopter() {}
void GeneratePlane() {}
void GenerateShip() {}
void GenerateTrain() {}
void GetIndString() {}
void InitFundingLevel() {}
void InitGraphMax() {}
void InvalidateEditors() {}
void InvalidateMaps() {}
void Kick() {}
void MakeAirCrash() {}
/* --- sprite system: disasters (monster/tornado/explosion) + ambient
 * vehicles (plane/ship/copter/train). The upstream sprite engine (w_sprite.c)
 * is not vendored; this is a compact reimplementation driving the original
 * Micropolis sprite art. Positions are world pixels (tile << 4). Direction
 * convention matches Micropolis: dir 1..8 = N,NE,E,SE,S,SW,W,NW. */
enum { ST_GOD = 0, ST_TOR, ST_AIR, ST_SHI, ST_COP, ST_TRA, ST_EXP };
#define SPR_MAX 8
typedef struct { int used, type, x, y, dir, life, frame, count; } Spr;
static Spr g_spr[SPR_MAX];
static int g_clk;
static const int VDx[9] = { 0, 0, 1, 1, 1, 0, -1, -1, -1 };
static const int VDy[9] = { 0, -1, -1, 0, 1, 1, 1, 0, -1 };

static Spr *spr_new(int type) {
    int i;
    for (i = 0; i < SPR_MAX; ++i) if (!g_spr[i].used) {
        Spr *s = &g_spr[i];
        s->used = 1; s->type = type;
        s->x = s->y = s->dir = s->life = s->frame = s->count = 0;
        return s;
    }
    return 0;
}
static int spr_have(int type) { int i, n = 0; for (i = 0; i < SPR_MAX; ++i) if (g_spr[i].used && g_spr[i].type == type) n++; return n; }

/* ---- tile destruction (monster / tornado / explosion) ---- */
static void dz_hit(int x, int y) {
    int m;
    if (x < 1 || y < 1 || x >= WORLD_X - 1 || y >= WORLD_Y - 1) return;
    m = Map[x][y] & LOMASK;
    if (m <= LASTRIVEDGE) return;              /* water / dirt: nothing to wreck */
    if (m >= RUBBLE && m <= LASTFIRE) return;  /* already rubble/flood/rad/fire   */
    if (Rand16() & 3) Map[x][y] = (RUBBLE + BULLBIT) + (Rand16() & 3);
    else              Map[x][y] = (FIRE + ANIMBIT) + (Rand16() & 7);
}
static void dz_blast(int cx, int cy, int r) {
    int x, y;
    for (y = cy - r; y <= cy + r; ++y)
        for (x = cx - r; x <= cx + r; ++x) dz_hit(x, y);
}
static void dz_find_built(int *bx, int *by) {
    int tries, x, y;
    for (tries = 0; tries < 400; ++tries) {
        x = 2 + (Rand16() % (WORLD_X - 4));
        y = 2 + (Rand16() % (WORLD_Y - 4));
        if ((Map[x][y] & LOMASK) >= ROADBASE) { *bx = x; *by = y; return; }
    }
    *bx = 6 + (Rand16() % (WORLD_X - 12));
    *by = 6 + (Rand16() % (WORLD_Y - 12));
}
/* pick a random water tile — the monster rises from the shoreline, as in the original */
static void dz_find_water(int *bx, int *by) {
    int tries, x, y, m;
    for (tries = 0; tries < 400; ++tries) {
        x = 8 + (Rand16() % (WORLD_X - 16));
        y = 5 + (Rand16() % (WORLD_Y - 10));
        m = Map[x][y] & LOMASK;
        if (m >= RIVER && m <= LASTRIVEDGE) { *bx = x; *by = y; return; }
    }
    *bx = WORLD_X / 2; *by = WORLD_Y / 2;   /* fallback: map centre */
}

/* ---- spawners (Make* hooks called by the sim/UI) ---- */
void MakeMonster(void) {
    int bx, by; Spr *s;
    if (spr_have(ST_GOD)) return;
    dz_find_water(&bx, &by);                 /* rises from the water, like the original */
    s = spr_new(ST_GOD); if (!s) return;
    s->x = bx << 4; s->y = by << 4; s->life = 600; s->dir = 1 + (Rand16() & 7);
}
void MakeTornado(void) {
    int bx, by; Spr *s;
    if (spr_have(ST_TOR)) return;
    bx = 4 + (Rand16() % (WORLD_X - 8));     /* anywhere on the map, not forced onto the city */
    by = 4 + (Rand16() % (WORLD_Y - 8));
    s = spr_new(ST_TOR); if (!s) return;
    s->x = bx << 4; s->y = by << 4; s->life = 400; s->dir = 1 + (Rand16() & 7);
}
static void spawn_explosion(int px, int py) {
    Spr *s = spr_new(ST_EXP); if (!s) return;
    s->x = px; s->y = py; s->frame = 0; s->count = 0;
}
void MakeExplosion(int x, int y)   { dz_blast(x, y, 1);        spawn_explosion(x << 4, y << 4); }
void MakeExplosionAt(int x, int y) { dz_blast(x >> 4, y >> 4, 1); spawn_explosion(x, y); }
void MakeCityExplosion(void) {
    int bx, by; dz_find_built(&bx, &by); dz_blast(bx, by, 2); spawn_explosion(bx << 4, by << 4);
}
void MakeSound() {}

/* ---- ambient vehicle spawners ---- */
static void spawn_plane(void) {
    Spr *s;
    if (spr_have(ST_AIR)) return;
    s = spr_new(ST_AIR); if (!s) return;
    s->y = (8 + (Rand16() % (WORLD_Y - 16))) << 4;
    s->life = 4000;
    if (Rand16() & 1) { s->x = -(48 << 4); s->dir = 3; }   /* enter left, fly east */
    else              { s->x = WORLD_X << 4; s->dir = 7; } /* enter right, fly west */
}
static void spawn_copter(void) {
    int bx, by; Spr *s;
    if (spr_have(ST_COP)) return;
    dz_find_built(&bx, &by); s = spr_new(ST_COP); if (!s) return;
    s->x = bx << 4; s->y = by << 4; s->dir = 1 + (Rand16() & 7); s->life = 700;
}
static void spawn_ship(void) {
    int tries, x, y, dir, nx, ny, m; Spr *s;
    if (spr_have(ST_SHI)) return;
    for (tries = 0; tries < 600; ++tries) {
        x = 2 + (Rand16() % (WORLD_X - 4));
        y = 2 + (Rand16() % (WORLD_Y - 4));
        m = Map[x][y] & LOMASK;
        if (m < RIVER || m > LASTRIVEDGE) continue;
        for (dir = 1; dir <= 7; dir += 2) {            /* cardinal only */
            nx = x + VDx[dir]; ny = y + VDy[dir];
            if (nx < 0 || ny < 0 || nx >= WORLD_X || ny >= WORLD_Y) continue;
            m = Map[nx][ny] & LOMASK;
            if (m >= RIVER && m <= LASTRIVEDGE) {
                s = spr_new(ST_SHI); if (!s) return;
                s->x = x << 4; s->y = y << 4; s->dir = dir; s->life = 1500;
                return;
            }
        }
    }
}
/* A tile a train may run on: plain rail, or a rail crossing a power line. */
#define IS_RAIL(m) (((m) >= RAILBASE && (m) <= LASTRAIL) || (m) == RAILHPOWERV || (m) == RAILVPOWERH)

static void spawn_train(void) {
    int tries, x, y, dir, nx, ny, m; Spr *s;
    if (spr_have(ST_TRA)) return;
    for (tries = 0; tries < 600; ++tries) {
        x = 2 + (Rand16() % (WORLD_X - 4));
        y = 2 + (Rand16() % (WORLD_Y - 4));
        m = Map[x][y] & LOMASK;
        if (!IS_RAIL(m)) continue;
        for (dir = 1; dir <= 7; dir += 2) {
            nx = x + VDx[dir]; ny = y + VDy[dir];
            if (nx < 0 || ny < 0 || nx >= WORLD_X || ny >= WORLD_Y) continue;
            m = Map[nx][ny] & LOMASK;
            if (IS_RAIL(m)) {
                s = spr_new(ST_TRA); if (!s) return;
                s->x = x << 4; s->y = y << 4; s->dir = dir; s->life = 800;
                return;
            }
        }
    }
}

/* ---- per-type stepping (tuned slow; movement gated on g_clk) ---- */
static void step_mover(Spr *s, int rad) {
    s->count--;
    if (s->count <= 0) { s->dir = 1 + (Rand16() & 7); s->count = 16 + (Rand16() % 16); }
    if (g_clk % 4 == 0) {                                      /* ~0.5 px/frame, like the original */
        s->x += VDx[s->dir] * 2; s->y += VDy[s->dir] * 2;
        if (s->x < (2 << 4))             { s->x = (2 << 4);             s->count = 0; }
        if (s->x > ((WORLD_X - 3) << 4)) { s->x = ((WORLD_X - 3) << 4); s->count = 0; }
        if (s->y < (2 << 4))             { s->y = (2 << 4);             s->count = 0; }
        if (s->y > ((WORLD_Y - 3) << 4)) { s->y = ((WORLD_Y - 3) << 4); s->count = 0; }
    }
    if (g_clk % 12 == 0) dz_blast(s->x >> 4, s->y >> 4, rad);
    if (s->type == ST_GOD) s->frame = (s->dir - 1) * 2 + ((g_clk >> 5) & 1);
    else                   s->frame = (g_clk >> 4) % 3;        /* tornado animation */
    if (--s->life <= 0) s->used = 0;
}
static void step_plane(Spr *s) {
    s->x += VDx[s->dir] * 2;                                    /* 2 px/frame */
    s->frame = (s->dir == 3) ? 10 : 6;                          /* obj3 east / west */
    if (s->x < -(64 << 4) || s->x > ((WORLD_X << 4) + (64 << 4))) s->used = 0;
    if (--s->life <= 0) s->used = 0;
}
static void step_copter(Spr *s) {
    s->count--;
    if (s->count <= 0) { s->dir = 1 + (Rand16() & 7); s->count = 80 + (Rand16() % 80); }
    if ((g_clk & 1) == 0) {                                     /* ~1 px/frame */
        s->x += VDx[s->dir] * 2; s->y += VDy[s->dir] * 2;
        if (s->x < 0) s->x = 0; if (s->x > ((WORLD_X - 2) << 4)) s->x = (WORLD_X - 2) << 4;
        if (s->y < 0) s->y = 0; if (s->y > ((WORLD_Y - 2) << 4)) s->y = (WORLD_Y - 2) << 4;
    }
    s->frame = s->dir - 1;                                      /* obj2-0..7 */
    if (--s->life <= 0) s->used = 0;
}
static void step_ship(Spr *s) {
    int nx, ny, m;
    if (g_clk % 3 == 0) {                                       /* slow boat */
        nx = (s->x >> 4) + VDx[s->dir]; ny = (s->y >> 4) + VDy[s->dir];
        if (nx < 0 || ny < 0 || nx >= WORLD_X || ny >= WORLD_Y) { s->used = 0; return; }
        m = Map[nx][ny] & LOMASK;
        if (m < RIVER || m > LASTRIVEDGE) { s->used = 0; return; }  /* left the water */
        s->x += VDx[s->dir] * 2; s->y += VDy[s->dir] * 2;
    }
    s->frame = s->dir - 1;                                      /* obj4-0..7 */
    if (--s->life <= 0) s->used = 0;
}
static void step_train(Spr *s) {
    int tx, ty, m, d, dir, order[4];
    if (g_clk & 1) return;                       /* ~1 px/frame, smooth like the copter */
    if ((s->x & 15) == 0 && (s->y & 15) == 0) {  /* on a tile boundary: choose the next rail */
        tx = s->x >> 4; ty = s->y >> 4;
        order[0] = s->dir;                       /* prefer straight, then turns, then back */
        order[1] = ((s->dir + 1) & 7) + 1;
        order[2] = ((s->dir + 5) & 7) + 1;
        order[3] = ((s->dir + 3) & 7) + 1;
        for (d = 0; d < 4; ++d) {
            int nx, ny;
            dir = order[d];
            nx = tx + VDx[dir]; ny = ty + VDy[dir];
            if (nx < 0 || ny < 0 || nx >= WORLD_X || ny >= WORLD_Y) continue;
            m = Map[nx][ny] & LOMASK;
            if (IS_RAIL(m)) { s->dir = dir; break; }
        }
        if (d == 4) { s->used = 0; return; }     /* dead end */
    }
    s->x += VDx[s->dir] * 2; s->y += VDy[s->dir] * 2;
    s->frame = (s->dir == 3 || s->dir == 7) ? 1 : 0;   /* EW / NS image (obj1) */
    if (--s->life <= 0) s->used = 0;
}
static void step_explosion(Spr *s) {
    if (g_clk % 5 == 0) s->frame++;
    if (s->frame >= 6) s->used = 0;
}

void StepDisaster(void) {        /* called once per engine_tick() */
    int i;
    g_clk++;
    if (g_clk % 240 == 0) {      /* occasionally add an ambient vehicle */
        switch (Rand16() % 4) {
            case 0: spawn_plane();  break;
            case 1: spawn_copter(); break;
            case 2: spawn_ship();   break;
            default: spawn_train(); break;
        }
    }
    for (i = 0; i < SPR_MAX; ++i) {
        Spr *s = &g_spr[i];
        if (!s->used) continue;
        switch (s->type) {
            case ST_GOD: step_mover(s, 1);   break;
            case ST_TOR: step_mover(s, 0);   break;
            case ST_AIR: step_plane(s);      break;
            case ST_SHI: step_ship(s);       break;
            case ST_COP: step_copter(s);     break;
            case ST_TRA: step_train(s);      break;
            case ST_EXP: step_explosion(s);  break;
        }
    }
}
int SprCount(void) { return SPR_MAX; }
int SprType(int i) { return g_spr[i].used ? g_spr[i].type : -1; }
int SprX(int i)    { return g_spr[i].x; }
int SprY(int i)    { return g_spr[i].y; }
int SprFrame(int i){ return g_spr[i].frame; }
void ResetLastKeys() {}
void SetFunds() {}
void UpdateFunds() {}
void doAllGraphs() {}
void drawCurrPercents() {}
void setAnyCityName() {}
void setCityName() {}

char *rindex(const char *s,int c){return strrchr(s,c);}

/* --- hooks for the vendored tool layer (w_tool.c / w_con.c) --------------- */
void Spend(int dollars) { TotalFunds -= dollars; }   /* real: deduct build cost */
int  sim_skip;
int  DoAnimation;
void MakeSoundOn() {}
void EventuallyRedrawView() {}
void ViewToPixelCoords() {}
char *Tk_PathName() { return ""; }
void NewInk() {}
void StartInk() {}
void AddInk() {}
void FreeInk() {}

/* Advance every ANIMBIT tile to its next animation frame (traffic cars, etc.).
 * Faithful to the original animateTiles(), but never turns a road tile into a
 * non-road (guards the table's tile-80 -> 0 quirk so light traffic can't eat
 * the road). Call this at display rate while the sim is running. */
void engine_animate_tiles(void) {
    unsigned short *p = (unsigned short *)&Map[0][0];
    int i, t, nv, fl;
    for (i = WORLD_X * WORLD_Y; i > 0; --i, ++p) {
        if (*p & ANIMBIT) {
            fl = *p & ALLBITS;
            t  = *p & LOMASK;
            nv = aniTile[t];
            if (t >= ROADBASE && t <= LASTROAD && (nv < ROADBASE || nv > LASTROAD))
                continue;                 /* don't let the animation delete a road */
            *p = (unsigned short)(nv | fl);
        }
    }
}
