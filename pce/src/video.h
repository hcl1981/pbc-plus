/* video.h -- PC Engine frame -> ST7789 on core 1.
 *
 * pce-go draws 8-bit colours (GGGRRRBB, a fixed palette) into a frame
 * buffer with a pitch of XBUF_WIDTH. The PC Engine changes its width per
 * game (mostly 256, also 320, 336 or 352 pixels) and is 224-242 lines high.
 * The panel is 240 wide and used upright:
 *
 *   up to 240 wide   centred, 1:1
 *   241-256 wide     the outer columns cropped (8 each side at 256), 1:1
 *   wider            scaled to 240, and the lines by the same factor so the
 *                    picture keeps its shape (336 x 224 -> 240 x 160); where
 *                    an output pixel or line falls between two source ones
 *                    the two are averaged, as in megadrive
 *
 * Otherwise lines are shown 1:1; the picture is centred vertically on the
 * 280 rows. That is
 * display mode 0; mode 1 "Crop 1:1" (video_set_mode) never scales and
 * shows the middle 240 columns of wider lines. The status text (fps etc.)
 * goes under the picture.
 *
 * Two frame buffers: pce-go renders the next frame into one while core 1
 * converts and streams the other.
 */
#ifndef PBC_VIDEO_H
#define PBC_VIDEO_H

#include <stdint.h>
#include <stdbool.h>

/* Core 1 entry point; it also owns the audio timer pool (audio_pwm.h). */
void video_core1_main(void);

/* Core 0: wait until core 1 is up (the audio timer then lands on core 1). */
void video_wait_ready(void);

/* Core 0: buffer pce-go should render the next frame into (top-left pixel
 * of line 0; pitch XBUF_WIDTH, 16 bytes of scratch on either side). */
uint8_t *video_back_buffer(void);

/* Core 0, after a frame: show the back buffer, `w` x `h` pixels, and swap.
 * Returns false (frame dropped, buffers kept) if core 1 is still busy. */
bool video_present(int w, int h);

/* Before core 1 starts: display mode (see above; unknown numbers -> 0). */
#define VIDEO_MODES 2
void video_set_mode(unsigned mode);

/* Core 0: status text under the picture ("" removes it). */
void video_set_status(const char *s);

#endif /* PBC_VIDEO_H */
