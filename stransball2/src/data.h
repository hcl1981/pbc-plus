/* data.h -- vorab gewandelte Spieldaten (siehe tools/mkdata.py). */
#ifndef STB_DATA_H
#define STB_DATA_H

#include <stdint.h>

#define TILE_W 16
#define TILE_H 16
#define TILE_NUM 500

/* Kachelbilder im Panelformat; 0x0000 = durchsichtig */
extern const uint16_t stb_tile[TILE_NUM][TILE_W * TILE_H];
/* Kollisionsmasken, 1 Bit je Punkt, gesetzt = fest */
extern const uint8_t  stb_mask[TILE_NUM][TILE_W * TILE_H / 8];

typedef struct { int16_t x, y; uint8_t type; } stb_tank_t;
typedef struct { uint8_t state, event; } stb_door_t;
typedef struct {
    int16_t sx, sy; int32_t off; uint8_t bg;
    int16_t toff; uint8_t tn;
    int16_t doff; uint8_t dn;
    char name[12];
} stb_mapinfo_t;
extern const stb_tank_t stb_tank[];
extern const stb_door_t stb_doordata[];
extern const int16_t stb_mapdata[];
extern const stb_mapinfo_t stb_map[];
extern const uint16_t stb_map_num;

#endif

/* ---- Schiffe: 32x32, gedreht wird zur Laufzeit --------------------------- */
#define SHIP_W 32
#define SHIP_H 32
#define SHIP_FRAMES 23
extern const uint16_t stb_ship[SHIP_FRAMES][SHIP_W * SHIP_H];
extern const uint8_t  stb_shipmask[SHIP_FRAMES][SHIP_W * SHIP_H / 8];
extern const uint8_t  stb_ship_base[3];
extern const uint8_t  stb_ship_nanim[3];
