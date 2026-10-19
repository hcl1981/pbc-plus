/* video.h -- Game Boy frame -> ST7789 on core 1 (1.5x by default).
 *
 * The emulator draws each line straight into a 160x144 frame of panel words
 * (BGR565, see color565.h). There are two such frames: core 0 fills one
 * while core 1 scales and streams the other. 160x144 becomes 240x216, centred
 * on the 240x280 panel: of every two source pixels a and b, three are output
 * -- a, the average of a and b, and b -- horizontally and vertically, so the
 * picture stays sharp where it can and no line is simply doubled.
 *
 * The other mode: 1:1 in the middle.
 *
 * Core 1 also hosts the sample timer of the speaker (its own alarm pool) and
 * arms the multicore lockout, so save files can be written while a game runs.
 */
#ifndef PBC_VIDEO_H
#define PBC_VIDEO_H

#include <stdint.h>
#include <stdbool.h>

#define GB_W 160
#define GB_H 144

/* The frame core 0 currently draws into. */
uint16_t *video_back(void);

/* Core 0, at the end of a frame: hand the back frame to core 1 and draw the
 * next one into the other buffer. Returns false (frame dropped, keep drawing
 * into the same buffer) if core 1 is still sending the previous one. */
bool video_present(void);

/* Display modes, as numbered in the PBCemu launcher (pbcemu/src/devices.c). */
enum {
    VIDEO_SCALE,        /* "Scale 1.5x": 240x216, the default */
    VIDEO_ORIGINAL,     /* "Original": 160x144, 1:1, centred */
    VIDEO_MODES
};

/* Before core 1 starts; unknown numbers mean VIDEO_SCALE. */
void video_set_mode(int mode);

void video_core1_main(void);
void video_wait_ready(void);
void video_set_status(const char *s);

#endif /* PBC_VIDEO_H */
