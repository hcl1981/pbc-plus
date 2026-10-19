/* c64_audio.c -- SID samples from Frodo to the speaker.
 *
 * Frodo's DigitalRenderer calls sid_add_sample() for every sample while it
 * emulates a frame (at the 22222 Hz of audio_pwm.h, see SID_rp2350.cpp).
 * They are collected here and handed to audio_pwm once per frame. */
#include <stdint.h>
#include "pico.h"
#include "audio_pwm.h"

#define FRAME_MAX 1024                 /* 22222 Hz / 50 Hz = 445 per frame */

static int16_t frame_buf[FRAME_MAX];
static int     frame_n;

void __not_in_flash_func(sid_add_sample)(int16_t left, int16_t right)
{
    (void)right;                       /* mono: both channels are the same */
    if (frame_n < FRAME_MAX)
        frame_buf[frame_n++] = left;
}

int sid_get_buffer_fill(void)
{
    return 0;
}

void c64_audio_flush(void)
{
    audio_pwm_push(frame_buf, frame_n);
    frame_n = 0;
}
