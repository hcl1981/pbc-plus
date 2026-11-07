/* zx_video.h -- Spectrum scanlines -> ST7789, sent by core 1.
 *
 * There is no frame buffer. The core hands over each of the 280 lines (44
 * of border, the 192 of the screen, 44 of border) as colour indices, 256
 * wide; core 0 draws them straight into a small ring, core 1 turns them into
 * 240 panel pixels while the line before goes out, and lays the status
 * text over the lower border. The picture fills the panel's height.
 *
 * Display modes (the launcher's numbering, 0 = default):
 *   0 Fit width   all 256 columns scaled to 240 (nearest or the average of two)
 *   1 Crop 1:1    the 8 outer columns on each side dropped
 */
#ifndef PBC_ZX_VIDEO_H
#define PBC_ZX_VIDEO_H

#include <stdbool.h>
#include <stdint.h>

enum { ZX_MODE_FIT, ZX_MODE_CROP, ZX_MODES };

/* Core 0, before core 1 starts. */
void zx_video_init(unsigned mode);

/* Core 0, zx_frame's callbacks: where line y goes (a ring slot, NULL if
 * not shown now), and that it is drawn there. */
uint8_t *zx_video_line_begin(int y);
void zx_video_line_commit(int y);

/* Core 1 entry point. */
void zx_video_core1_main(void);

/* Core 0: true if the last frame was drawn (counted for the statistics). */
bool zx_video_frame_drawn(void);

/* Core 0: stop core 1 using the panel (menus), and start it again. */
void zx_video_pause(void);
void zx_video_resume(void);

/* Status text over the lower border; "" clears it. */
void zx_video_set_status(const char *s);

#endif /* PBC_ZX_VIDEO_H */
