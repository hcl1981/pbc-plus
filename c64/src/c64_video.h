/* c64_video.h -- VIC buffer -> ST7789 on core 1.
 *
 * Frodo draws 384x272 including the border. The 320x240 window
 * frank-c64 shows on HDMI (x 32.., y 16..) is scaled by 3/4 to 240x180,
 * the same way megadrive scales H40: of every four pixels the outer two are
 * kept and the inner two averaged, horizontally and vertically, so the
 * 8-pixel C64 font stays readable.
 *
 * Core 1 converts and streams the frame while core 0 already emulates the
 * next one. It reads the buffer from the top faster than the VIC rewrites
 * it, so a single buffer is enough.
 */
#ifndef PBC_C64_VIDEO_H
#define PBC_C64_VIDEO_H

#include <stdbool.h>

void c64_video_core1_main(void);

/* Core 0: wait until core 1 has set up the audio timer pool (main.c then
 * starts the sound, so its interrupts land on core 1). */
void c64_video_wait_ready(void);

/* Core 0, after each emulated frame. Returns false (dropped) if core 1 is
 * still sending the previous one. */
bool c64_video_present(void);

void c64_video_set_status(const char *s);

#endif /* PBC_C64_VIDEO_H */
