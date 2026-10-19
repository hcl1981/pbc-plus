/* sms_video.h -- SMS Plus line output -> ST7789, sent by core 1.
 *
 * SMS Plus hands over one line of 8-bit palette indices at a time
 * (sms_render_line). Each line is converted to panel words right away, with
 * the palette in force on that line, so mid-frame palette changes survive,
 * and stored in one of two frame buffers. Core 1 sends the finished frame
 * while core 0 emulates the next one into the other buffer.
 *
 * Display modes (the launcher's numbering, 0 = default):
 *   Master System 256x192 (or 224 lines)
 *     0 Crop 1:1     the 8 outer columns on each side dropped (the left ones
 *                    are often blanked by the game anyway), 240 wide, 1:1
 *     1 Fit width    all 256 columns scaled to 240, height to match (180)
 *     2 Zoom 1.25x   the middle 192 columns to 240, 1.25x high (240/280)
 *   Game Gear 160x144
 *     0 Scale 1.5x   240x216: of two source pixels a, b three are shown --
 *                    a, their average, b -- across and down
 *     1 Original     160x144, 1:1
 * No mode distorts the picture: both directions get the same factor.
 * Scaling picks the nearest source pixel, or the average of two where the
 * output pixel falls between them. Master System columns are scaled on
 * core 0 while the line is converted (the frame buffer stays 240 wide),
 * everything else on core 1 while it sends.
 */
#ifndef PBC_SMS_VIDEO_H
#define PBC_SMS_VIDEO_H

#include <stdbool.h>

/* Core 1 entry point; sets up the audio timer pool first (see main.c). */
void sms_video_core1_main(void);

/* Core 0: wait until core 1 has created the audio timer pool. */
void sms_video_wait_ready(void);

enum { SMS_MODE_CROP, SMS_MODE_FIT, SMS_MODE_ZOOM, SMS_MODES };
enum { GG_MODE_SCALE, GG_MODE_ORIGINAL, GG_MODES };

/* Core 0, before the first frame: Game Gear or Master System and the
 * display mode (unknown numbers fall back to 0). */
void sms_video_set_mode(bool gg, unsigned mode);

/* Core 0, after each emulated frame. Returns false (dropped) if core 1 is
 * still sending the previous one. */
bool sms_video_present(void);

/* Status line (fps etc.): under the picture, or over its top left corner
 * when the picture leaves no room. */
void sms_video_set_status(const char *s);

#endif /* PBC_SMS_VIDEO_H */
