/* pce_glue.h -- what the PicoBoy side needs from the pce-go host glue. */
#ifndef PBC_PCE_GLUE_H
#define PBC_PCE_GLUE_H

#include <stdint.h>

/* PC Engine frame rate: 7.16 MHz / (455 * 263) = 59.826 Hz */
#define PCE_FRAME_US 16715u

/* Width and height pce-go reported for the frame just rendered. */
void pce_glue_frame_size(int *w, int *h);

/* After pce_run(): finish this frame's audio and hand it to the speaker
 * (mono, AUDIO_RATE), then prepare the next frame. */
void pce_glue_audio_frame(void);

#endif /* PBC_PCE_GLUE_H */
