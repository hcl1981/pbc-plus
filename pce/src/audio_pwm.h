/* audio_pwm.h -- PC Engine sound on the speaker at GP15 (PWM + RC low-pass).
 *
 * A DMA channel paced by a DMA timer takes one sample per tick from a ring
 * buffer and writes it to the PWM compare value, without an interrupt per
 * sample. The tick is 45 us, so the rate is exactly 22222 Hz, and the PSG is
 * asked for exactly that rate. Uses one DMA channel and one DMA
 * timer.
 *
 * The emulator's frame pacing and the sample clock are two clocks; the ring
 * fill level is checked on every push and nudged by dropping or repeating a
 * single sample.
 */
#ifndef PBC_AUDIO_PWM_H
#define PBC_AUDIO_PWM_H

#include <stdint.h>
#include <stdbool.h>

#define AUDIO_TICK_US   45
#define AUDIO_RATE      (1000000 / AUDIO_TICK_US)   /* 22222 Hz */
#define AUDIO_VOL_MAX   12   /* = VOLUME_MAX in settings.h */

/* After the final system clock is set. */
void audio_pwm_init(void);

/* 0 = off (DMA stopped, speaker pin held low), 1..AUDIO_VOL_MAX. */
void audio_pwm_set_volume(int vol);

/* Once per frame, from one core only: queue the frame's mono samples. */
void audio_pwm_push(const int16_t *s, int n);

#endif /* PBC_AUDIO_PWM_H */
