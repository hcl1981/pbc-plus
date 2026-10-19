/* c64_video.h -- VIC buffer -> ST7789 on core 1.
 *
 * Frodo draws 384x272 including the border. By default the 320x240 window
 * frank-c64 shows on HDMI (x 32.., y 16..) is scaled by 3/4 to 240x180,
 * the same way megadrive scales H40: of every four pixels the outer two are
 * kept and the inner two averaged, horizontally and vertically, so the
 * 8-pixel C64 font stays readable. Other display modes below.
 *
 * Core 1 converts and streams the frame while core 0 already emulates the
 * next one. It reads the buffer from the top faster than the VIC rewrites
 * it, so a single buffer is enough.
 */
#ifndef PBC_C64_VIDEO_H
#define PBC_C64_VIDEO_H

#include <stdbool.h>

/* Display modes (pbcemu's launcher shows them under the same numbers):
 *   0  Standard     320x240 window (screen + some border) at 3/4: 240x180
 *   1  Full border  all of 384x272 at 5/8: 240x170
 *   2  No border    the 320x200 screen at 3/4: 240x150
 * All keep the aspect ratio. */
enum { C64_VIDEO_STANDARD, C64_VIDEO_FULL_BORDER, C64_VIDEO_NO_BORDER, C64_VIDEO_MODES };

/* Before core 1 starts; unknown numbers mean Standard. */
void c64_video_set_mode(unsigned mode);

void c64_video_core1_main(void);

/* Core 0: wait until core 1 has set up the audio timer pool (main.c then
 * starts the sound, so its interrupts land on core 1). */
void c64_video_wait_ready(void);

/* Core 0, after each emulated frame. Returns false (dropped) if core 1 is
 * still sending the previous one. */
bool c64_video_present(void);

void c64_video_set_status(const char *s);

#endif /* PBC_C64_VIDEO_H */
