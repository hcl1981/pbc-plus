#pragma once
#include <stdint.h>

// =====================================================================
//  Tileset seam
//
//  render.c blits 16x16 RGB565 tiles via tile_get(id) -> 256 pixels.
//
//  tiles_micropolis.c provides a const RGB565 array of the ~960 original
//  Micropolis tiles living in flash, implementing tile_get().
// =====================================================================

void            tiles_init(void);              // no-op: tiles live in flash
const uint16_t *tile_get(uint16_t id);         // -> 256 RGB565 pixels

// Real Micropolis tool-palette icons (ICON_PX x ICON_PX RGB565), by tool_t.
const uint16_t *tool_icon(int tool);
