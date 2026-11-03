/* a26_video.h -- Atari 2600 lines -> ST7789, sent by core 1.
 *
 * No frame buffer: the core hands over each line as 160 TIA colour
 * indices into a ring; core 1 turns each into 240 panel words (each pair
 * of pixels becomes three, the middle one their average) and sends it. Of
 * a frame, 240 lines are shown: NTSC lines 26..265 of 262 (the games put
 * their picture between about 37 and 230), PAL lines 42..281 of 312.
 */
#ifndef PBC_A26_VIDEO_H
#define PBC_A26_VIDEO_H

#include <stdbool.h>
#include <stdint.h>

/* Core 0, before core 1 starts. */
void a26_video_init(unsigned mode);

/* Core 0: PAL colours and window (true) or NTSC (false). */
void a26_video_set_pal(bool pal);

/* Core 0: a finished line (a2600_frame's callback). */
void a26_video_line(int y, const uint8_t *pixels, bool blank);

/* Core 0: a 7800 line (a7800.h): 320 palette indices, y 0..239 */
void a26_video_line320(int y, const uint8_t *pixels);

/* Core 1 entry point. */
void a26_video_core1_main(void);

/* Core 0: true if the last frame was drawn (counted for the statistics). */
bool a26_video_frame_drawn(void);

/* Core 0: stop core 1 using the panel (menus), and start it again. */
void a26_video_pause(void);
void a26_video_resume(void);

/* Status line (fps, messages) under the picture; "" clears it. */
void a26_video_set_status(const char *s);

#endif /* PBC_A26_VIDEO_H */
