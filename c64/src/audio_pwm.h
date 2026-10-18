/* audio_pwm.h -- SID sound on the speaker at GP15 (PWM + RC low-pass).
 *
 * The same scheme PicoBoyGB uses on this board: a repeating timer takes one
 * sample per tick from a ring buffer and writes it to the PWM compare
 * value. The tick is 45 us, so the rate is exactly 22222 Hz, and the SID is
 * asked for exactly that rate.
 *
 * The emulator's frame pacing and the sample timer are two clocks; the ring
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

#include "pico/time.h"

/* After the final system clock is set. */
void audio_pwm_init(void);

/* Run the sample timer from this alarm pool instead of the default one on
 * core 0 (its interrupt then fires on the core that created the pool).
 * Call before the first audio_pwm_set_volume(). */
void audio_pwm_set_pool(alarm_pool_t *pool);

/* 0 = off (timer stopped, speaker pin held low), 1..AUDIO_VOL_MAX. */
void audio_pwm_set_volume(int vol);

/* Core 0, once per frame: queue the frame's mono samples. */
void audio_pwm_push(const int16_t *s, int n);

#endif /* PBC_AUDIO_PWM_H */
