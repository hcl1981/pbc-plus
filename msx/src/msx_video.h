/* msx_video.h -- MSX1 scanlines -> ST7789, sent by core 1.
 *
 * There is no frame buffer. The core hands over each of the 192 lines as
 * TMS9918 colour indices (256 wide); core 0 turns it into 240 panel words
 * at once and puts it into a small ring; core 1 sends the lines from there
 * as they come, inside one window per frame that line 0 opens. Core 0 only
 * waits when the ring is full, so the panel always shows whole lines of
 * the newest frame.
 *
 * Display modes (the launcher's numbering, 0 = default):
 *   0 Fit width   all 256 columns scaled to 240 (nearest or the average of
 *                 two), lines 1:1
 *   1 Crop 1:1    the 8 outer columns on each side dropped
 * 192 lines, centred.
 */
#ifndef PBC_MSX_VIDEO_H
#define PBC_MSX_VIDEO_H

#include <stdbool.h>

enum { MSX_MODE_FIT, MSX_MODE_CROP, MSX_MODES };

/* Core 0, before core 1 starts. */
void msx_video_init(unsigned mode);

/* Core 0: a finished line (msx1_frame's callback). */
void msx_video_line(int y, const uint8_t *pixels, int backdrop);

/* Core 1 entry point. */
void msx_video_core1_main(void);

/* Core 0: true if the last frame was drawn (counted for the statistics). */
bool msx_video_frame_drawn(void);

/* Core 0: stop core 1 using the panel (menus), and start it again. */
void msx_video_pause(void);
void msx_video_resume(void);

/* Status line (fps, messages) under the picture; "" clears it. */
void msx_video_set_status(const char *s);

#endif /* PBC_MSX_VIDEO_H */
