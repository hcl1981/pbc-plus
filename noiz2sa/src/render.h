/* render.h -- erzeugt Bildzeilen im Panelformat (RGB565), 240 px breit. */
#ifndef NOIZ_RENDER_H
#define NOIZ_RENDER_H

#include <stdint.h>
#include "game.h"

typedef struct {
    int blink;
    int show_debug;
    int ms_frame, ms_cpu;
    int mute;
} noiz_render_cfg_t;

extern noiz_render_cfg_t noiz_render;

void noiz_render_init(void);
void noiz_render_rows(uint16_t *dst, int y0, int rows);

#endif
