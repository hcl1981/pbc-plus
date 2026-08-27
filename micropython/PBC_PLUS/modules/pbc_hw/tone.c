#include "tone.h"

#include "pico/time.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"

#include "pbc_hw_config.h"

static uint tone_slice;
static uint tone_channel;
static bool tone_initialized = false;
static alarm_id_t tone_alarm = 0;

static int64_t tone_alarm_cb(alarm_id_t id, void *user_data) {
    (void)id;
    (void)user_data;
    tone_off();
    return 0; // do not reschedule
}

static void tone_init_once(void) {
    if (tone_initialized) {
        return;
    }
    tone_slice   = pwm_gpio_to_slice_num(SPEAKER_PIN);
    tone_channel = pwm_gpio_to_channel(SPEAKER_PIN);
    tone_initialized = true;
}

void tone_play(uint32_t freq_hz, uint32_t duration_ms) {
    tone_init_once();

    // Always (re-)attach the pin to the PWM peripheral. tone_off()
    // parks it as a SIO output low so the speaker doesn't sit at
    // half-rail; without this re-attach, every tone after the first
    // configures the PWM slice but the pin no longer listens.
    gpio_set_function(SPEAKER_PIN, GPIO_FUNC_PWM);

    if (tone_alarm) {
        cancel_alarm(tone_alarm);
        tone_alarm = 0;
    }

    if (freq_hz == 0) {
        tone_off();
        return;
    }
    if (freq_hz < 20)    freq_hz = 20;
    if (freq_hz > 20000) freq_hz = 20000;

    // Aim for wrap >= 1000 for good duty resolution; bump it up if
    // the divider would overflow the 8.4-bit hardware range. wrap is
    // a uint16, so keep it under 32768 (next double would overflow).
    uint32_t sysclk = clock_get_hz(clk_sys);
    uint32_t wrap   = 1000;
    float    div    = (float)sysclk / ((float)freq_hz * (float)(wrap + 1));

    while (div > 255.0f && wrap < 32768) {
        wrap *= 2;
        div = (float)sysclk / ((float)freq_hz * (float)(wrap + 1));
    }
    if (div < 1.0f)   div = 1.0f;
    if (div > 255.9f) div = 255.9f;

    pwm_set_clkdiv(tone_slice, div);
    pwm_set_wrap(tone_slice, (uint16_t)wrap);
    pwm_set_chan_level(tone_slice, tone_channel, (uint16_t)((wrap + 1) / 2));
    pwm_set_enabled(tone_slice, true);

    if (duration_ms > 0) {
        tone_alarm = add_alarm_in_ms(duration_ms, tone_alarm_cb, NULL, true);
    }
}

void tone_off(void) {
    if (!tone_initialized) {
        return;
    }
    pwm_set_enabled(tone_slice, false);
    // Park the pin low so the speaker isn't held at half-rail.
    // tone_play() will switch it back to GPIO_FUNC_PWM next time.
    gpio_set_function(SPEAKER_PIN, GPIO_FUNC_SIO);
    gpio_set_dir(SPEAKER_PIN, GPIO_OUT);
    gpio_put(SPEAKER_PIN, 0);

    if (tone_alarm) {
        cancel_alarm(tone_alarm);
        tone_alarm = 0;
    }
}
