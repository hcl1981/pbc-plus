/*
 * audio_pwm.h
 *
 * PWM-Audio-Layer fuer den Piezo am PicoBoy Color (GP15, mit RC-Tiefpass davor).
 *
 * Strategie:
 *  - PWM-Slice 7B auf GP15, Wrap=1023, Trager bei ~195 kHz @ 200 MHz Sysclk.
 *  - Repeating Timer alle 22.7 us (44100 Hz) konsumiert Samples aus einem
 *    Ringpuffer und schreibt sie in das PWM Compare-Register.
 *  - Stereo-16-bit-Samples vom minigb_apu werden zu 10-bit-Mono gemischt.
 *  - Ringpuffer ist 4096 Samples gross (~93 ms Audio-Pufferung).
 *
 * Single-Header: in genau EINER .cpp vor dem Include
 *   #define AUDIO_PWM_IMPL
 * setzen.
 */

#ifndef AUDIO_PWM_H
#define AUDIO_PWM_H

#include <Arduino.h>
#include <hardware/pwm.h>
#include <hardware/gpio.h>
#include <pico/time.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef AUDIO_PWM_PIN
#define AUDIO_PWM_PIN  15
#endif

#ifndef AUDIO_PWM_WRAP
#define AUDIO_PWM_WRAP 1023u    // 10-bit
#endif

#ifndef AUDIO_PWM_BUF
#define AUDIO_PWM_BUF  4096u    // muss Zweierpotenz sein
#endif

void audio_pwm_init(uint32_t sample_rate);
void audio_pwm_push(const int16_t *stereo, size_t samples);
void audio_pwm_set_volume(uint8_t v);   // 0..8 (0=mute, 4=normal)
void audio_pwm_mute(bool m);

#ifdef __cplusplus
}
#endif

// =====================================================================
//  Implementierung
// =====================================================================
#ifdef AUDIO_PWM_IMPL

static volatile uint16_t g_audio_buf[AUDIO_PWM_BUF];
static volatile uint32_t g_audio_head = 0;     // schreiben (Producer)
static volatile uint32_t g_audio_tail = 0;     // lesen (Consumer/IRQ)
static uint8_t  g_audio_volume = 16;           // 0..32, 8 = unity, 16 = 2x, 32 = 4x
static volatile bool g_audio_mute = false;
static uint     g_audio_slice = 0;
static uint     g_audio_chan  = 0;
static repeating_timer_t g_audio_timer;

static bool __not_in_flash_func(audio_pwm_timer_cb)(repeating_timer_t *t) {
    (void)t;
    uint32_t tail = g_audio_tail;
    uint16_t s;
    if (tail == g_audio_head) {
        s = AUDIO_PWM_WRAP / 2;   // Pufferleer: Mittelwert (Stille auf RC)
    } else {
        s = g_audio_buf[tail & (AUDIO_PWM_BUF - 1)];
        g_audio_tail = tail + 1;
    }
    if (g_audio_mute) s = AUDIO_PWM_WRAP / 2;
    pwm_set_chan_level(g_audio_slice, g_audio_chan, s);
    return true;
}

void audio_pwm_init(uint32_t sample_rate) {
    gpio_set_function(AUDIO_PWM_PIN, GPIO_FUNC_PWM);
    g_audio_slice = pwm_gpio_to_slice_num(AUDIO_PWM_PIN);
    g_audio_chan  = pwm_gpio_to_channel(AUDIO_PWM_PIN);

    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_wrap(&cfg, AUDIO_PWM_WRAP);
    pwm_config_set_clkdiv(&cfg, 1.0f);     // voller Sysclock-Trager
    pwm_init(g_audio_slice, &cfg, true);
    pwm_set_chan_level(g_audio_slice, g_audio_chan, AUDIO_PWM_WRAP / 2);

    // Mittelwert in den Ringpuffer als sichere Anfangsfuellung
    for (uint32_t i = 0; i < AUDIO_PWM_BUF; ++i) g_audio_buf[i] = AUDIO_PWM_WRAP / 2;
    g_audio_head = AUDIO_PWM_BUF / 2;       // halbvoll starten
    g_audio_tail = 0;

    // Sample-Timer: -1us heisst "1us nach dem letzten Aufruf" (constant rate)
    int64_t period_us = -(int64_t)(1000000 / sample_rate);
    add_repeating_timer_us(period_us, audio_pwm_timer_cb, NULL, &g_audio_timer);
}

void audio_pwm_set_volume(uint8_t v) {
    if (v > 32) v = 32;
    g_audio_volume = v;
}

void audio_pwm_mute(bool m) { g_audio_mute = m; }

void __not_in_flash_func(audio_pwm_push)(const int16_t *stereo, size_t samples) {
    uint32_t head = g_audio_head;
    uint32_t tail = g_audio_tail;
    uint32_t mask = AUDIO_PWM_BUF - 1;
    int vol = g_audio_volume;
    for (size_t i = 0; i < samples; ++i) {
        // Stereo zu Mono mischen
        int32_t s = ((int32_t)stereo[2*i] + (int32_t)stereo[2*i+1]) >> 1;
        // Lautstaerke 0..8 mappen (>>3 = unity)
        s = (s * vol) >> 3;
        if (s >  32767) s =  32767;
        if (s < -32768) s = -32768;
        // 16-bit signed -> 10-bit unsigned mit Mittelpunkt
        uint16_t u = (uint16_t)((s + 32768) >> 6);
        if (u > AUDIO_PWM_WRAP) u = AUDIO_PWM_WRAP;

        // Wenn voll, ueberschreiben wir den aeltesten Sample
        if (((head + 1) & mask) == (tail & mask)) {
            tail = (tail + 1) & 0xFFFFFFFFu;
            g_audio_tail = tail;
        }
        g_audio_buf[head & mask] = u;
        head = (head + 1) & 0xFFFFFFFFu;
    }
    g_audio_head = head;
}

#endif // AUDIO_PWM_IMPL
#endif // AUDIO_PWM_H
