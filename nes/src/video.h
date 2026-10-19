/* video.h -- NES frame -> ST7789 on core 1.
 *
 * The NES picture is 256x240; the panel is 240x280, used upright. How the
 * picture is fitted is the display mode (VIDEO_MODE_*), chosen once before
 * core 1 starts:
 *
 *   CROP     240x240 1:1, eight columns dropped on each side -- the strip a
 *            real TV hides in its overscan anyway, and where many games
 *            leave scroll garbage. The default.
 *   FIT      the whole 256x240 scaled by 15/16 to 240x225. Where two source
 *            pixels (rows) share an output pixel (row) they are averaged,
 *            so thin lines and small text survive.
 *   NO_OVERSCAN  like CROP, and the 8 top and bottom lines go as well
 *            (240x224 1:1) -- what a TV showed.
 *
 * None distorts the picture; it sits centred, the rest of the panel black.
 *
 * QuickNES renders into a pair of pixel buffers and swaps them after every
 * frame. Core 0 hands a finished frame plus its palette to core 1, which
 * converts it row by row to panel words and streams them out by DMA while
 * core 0 emulates the next frame into the other buffer.
 */
#ifndef PBC_VIDEO_H
#define PBC_VIDEO_H

#include <stdint.h>
#include <stdbool.h>

enum {
    VIDEO_MODE_CROP,
    VIDEO_MODE_FIT,
    VIDEO_MODE_NO_OVERSCAN,
    VIDEO_MODE_COUNT
};

/* Before core 1 starts. Unknown modes fall back to VIDEO_MODE_CROP. */
void video_set_mode(unsigned mode);

/* Core 1 entry point. */
void video_core1_main(void);

/* Core 0: has core 1 finished with `pixels`? Call before letting the
 * emulator write into a buffer that might still be on its way out. */
bool video_busy_with(const uint8_t *pixels);

/* Core 0: offer a finished frame. `pixels` points at the top-left pixel of
 * the 256-wide image, `pitch` bytes per row; `palette` holds `count` NES
 * colours (0..511) for the pixel values. Returns false (frame dropped) if
 * core 1 is still sending the previous one. */
bool video_present(const uint8_t *pixels, long pitch, const int16_t *palette, int count);

/* Core 0: status text (fps etc.) under the picture. NULL or "" hides it. */
void video_set_status(const char *s);

#endif /* PBC_VIDEO_H */
