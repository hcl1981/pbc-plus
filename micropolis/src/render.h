#pragma once
#include <stdint.h>

// Draw one full frame into the RGB565 framebuffer (DISPLAY_W*DISPLAY_H):
// visible map tiles, the selection cursor, the top status bar and the
// bottom HUD (RCI demand bars + active tool / tool strip).
void render_frame(uint16_t *fb);
