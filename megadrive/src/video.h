/* video.h -- Genesis framebuffer -> scaled ST7789 output.
 *
 * The VDP renders 8-bit palette indices into a 320x240 buffer, one line at
 * a time. The panel is 240 wide and used upright, so every frame is scaled
 * down to 240 pixels wide with the aspect ratio kept:
 *
 *     H40 (320 wide) -> 3/4  : 320x224 -> 240x168, PAL 320x240 -> 240x180
 *     H32 (256 wide) -> 15/16: 256x224 -> 240x210, PAL 256x240 -> 240x225
 *
 * (default mode; see video_set_mode for 1:1 crop)
 *
 * Dropped pixels are not simply skipped: the two source pixels/lines that
 * share one output pixel/line are averaged, which keeps small text legible.
 *
 * Output runs on core 1 while core 0 is still emulating: as soon as the
 * source lines an output row needs have been drawn, that row is converted
 * and DMA'd to the panel. The full frame never sits in RAM as RGB.
 */
#ifndef PBC_VIDEO_H
#define PBC_VIDEO_H

#include <stdint.h>
#include <stdbool.h>

#define VIDEO_SRC_W 320
#define VIDEO_SRC_H 240

extern uint8_t video_screen[VIDEO_SRC_H][VIDEO_SRC_W];

/* Core 0, before the first line of a frame that will be drawn. Waits until
 * the previous frame has been fully sent (its source lines are about to be
 * overwritten). */
void video_frame_begin(int src_w, int src_h);

/* Whichever core drew source line `line` calls this right after. Lines are
 * always completed in order. */
void video_line_done(int line);

/* Core 1, polled from its idle loop: sends every output row whose source
 * lines are ready. Never blocks on the SPI. */
void video_service(void);

/* Core 1: overlay a short status text (e.g. FPS) under the picture.
 * Only drawn between frames. */
void video_set_status(const char *s);

/* Display mode, set before the first frame (launcher setting):
 *   VIDEO_FIT   width scaled to 240 (3/4 or 15/16), aspect kept -- default
 *   VIDEO_CROP  1:1 pixels, the middle 240 of each line */
enum { VIDEO_FIT, VIDEO_CROP };
void video_set_mode(int mode);

/* True once all rows of the current frame have been sent. */
bool video_idle(void);

#endif /* PBC_VIDEO_H */
