/* ard_video.h -- the Arduboy's 128x64 OLED on the ST7789, sent by core 1.
 *
 * Core 0 hands over each finished frame (the SSD1306's 1 KB of pixels);
 * core 1 scales it to the panel while core 0 goes on, and draws the status
 * line below the picture.
 *
 * Display modes (the launcher's numbering, 0 = default):
 *   0 Fit       240x120: the whole width, each pixel 1.875 panel pixels
 *               (where it falls between two, their average)
 *   1 Sharp     192x96: each pixel 1 or 2 panel pixels, no blending
 */
#ifndef ARD_VIDEO_H
#define ARD_VIDEO_H

#include <stdbool.h>
#include <stdint.h>

enum { ARD_MODE_FIT, ARD_MODE_SHARP, ARD_MODES };

void ard_video_init(unsigned mode);     /* core 0, before core 1 starts */
void ard_video_core1_main(void);

/* Core 0: a finished frame (NULL: the display is off). Returns false if
 * core 1 still has the last one (this one is dropped). */
bool ard_video_frame(const uint8_t *screen, bool invert);

/* Status text below the picture; "" clears it. */
void ard_video_set_status(const char *s);

#endif
