/* pbc_scanline.h -- the "Scanlines" display option of PBCemu
 *
 * Every second row on the panel at three quarters of its brightness, like
 * the dark gaps between the lines of an old television. Chosen per system
 * in the launcher, handed over in the launch record (PBC_LAUNCH_SCANLINES).
 * The emulators dim a row in their own line buffer right before it goes to
 * the panel; the standalone firmwares never do. */
#ifndef PBC_SCANLINE_H
#define PBC_SCANLINE_H

#include <stdint.h>
#include <stdbool.h>
#if PBC_MULTI
#include "pbc_launch.h"
#endif

static inline bool pbc_scanlines_on(void)
{
#if PBC_MULTI
    const pbc_launch_t *l = pbc_launch_get();
    return l && (l->flags & PBC_LAUNCH_SCANLINES);
#else
    return false;
#endif
}

/* 3/4 of each colour: c - c/4 per channel. The mask keeps each channel's
 * quarter inside its own field, so this works for RGB and BGR 565 alike. */
static inline void pbc_scanline_dim(uint16_t *px, int n)
{
    for (int i = 0; i < n; i++)
        px[i] = (uint16_t)(px[i] - ((px[i] >> 2) & 0x39E7));
}

#endif
