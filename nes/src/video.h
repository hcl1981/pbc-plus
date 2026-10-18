/* video.h -- NES frame -> ST7789, 1:1 on core 1.
 *
 * The NES picture is 256x240. The panel is 240 wide and used upright, so
 * eight columns are dropped on each side -- the strip a real TV hides in
 * its overscan anyway, and where many games leave scroll garbage. What
 * remains is 240x240, shown pixel for pixel without scaling, centred
 * vertically on the 280-row panel.
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

#define VIDEO_W 240
#define VIDEO_H 240

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

/* Core 0: status text under the picture (e.g. fps), drawn between frames. */
void video_set_status(const char *s);

#endif /* PBC_VIDEO_H */
