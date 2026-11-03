/* cpc_video.h -- CPC scanlines -> ST7789, sent by core 1.
 *
 * There is no frame buffer. The core hands over each of the 280 lines (40
 * of border, the 200 of the standard screen, 40 of border) as hardware
 * colours, 640 wide (mode 2 pixels); core 0 only copies them into a small
 * ring, core 1 turns them into 240 panel pixels while the line before goes
 * out, and lays the status text over the lower border.
 *
 * Display modes (the launcher's numbering, 0 = default):
 *   0 Fit width   the whole 640 scaled to 240 (each panel pixel the average
 *                 of the 8/3 source pixels under it)
 *   1 Crop 1:1    the middle 240 mode 1 pixels (40 cut off each side)
 */
#ifndef PBC_CPC_VIDEO_H
#define PBC_CPC_VIDEO_H

#include <stdbool.h>
#include <stdint.h>

enum { CPC_MODE_FIT, CPC_MODE_CROP, CPC_MODES };

/* Core 0, before core 1 starts. */
void cpc_video_init(unsigned mode);

/* Core 0: a finished line (cpc_frame's callback). */
void cpc_video_line(int y, const uint8_t *pixels);

/* Core 1 entry point. */
void cpc_video_core1_main(void);

/* Core 0: true if the last frame was drawn (counted for the statistics). */
bool cpc_video_frame_drawn(void);

/* Core 0: stop core 1 using the panel (menus), and start it again. */
void cpc_video_pause(void);
void cpc_video_resume(void);

/* Status text over the lower border; "" clears it. */
void cpc_video_set_status(const char *s);

#endif /* PBC_CPC_VIDEO_H */
