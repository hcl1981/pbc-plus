//
// PicoBoy Color SFX engine for rp2040-doom-rp2.
//
// rp2040-doom uses a custom WAD format ("WHX") in which sound effects
// are stored as IMA-ADPCM blocks. The Doom engine routes sound through
// the sound_module_t function pointer table in i_sound.c. The original
// rp2040-doom build's i_picosound.c implements a full mixer that talks
// to the audio_i2s driver from pico-extras; we don't have I2S hardware,
// so this file replaces the *output* stage with a 1-channel 8-bit PWM
// DAC on GP15 (the PicoBoy piezo speaker pin), driven by DMA + IRQ.
//
// The ADPCM decoder, channel state, and per-frame mixing logic are
// adapted from the original i_picosound.c (Graham Sanderson, GPLv2).
//
// Design summary:
//
//   * 1 kHz of "audio render" via a hardware timer IRQ. Each render
//     pass mixes all currently-playing channels into a 256-sample mono
//     buffer at 22050 Hz, then DMA-streams it to the PWM slice.
//   * PWM slice runs at sysclock / 256 = ~488 kHz carrier; duty cycle
//     in 0..255 represents 0..3.3V averaged through the on-board RC
//     low-pass filter.
//   * No music output (OPL2 emulation is too heavy for what the piezo
//     can reproduce anyway).
//
// GPLv2 (same as rp2040-doom).
//

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"

#include "doomtype.h"
#include "i_sound.h"
#include "i_picosound.h"
#include "deh_str.h"
#include "m_misc.h"
#include "w_wad.h"
#include "z_zone.h"

#include "picoboy_config.h"

// ---------------------------------------------------------------------------
// PWM/DMA audio output
// ---------------------------------------------------------------------------
//
// Architecture: instead of an IRQ-driven ring of small buffers, we use a
// single big buffer played in a continuous loop by a self-retriggering
// DMA pair:
//
//   * "audio" DMA channel: streams uint16_t samples from the buffer to
//     the PWM compare register, paced by the PWM-wrap DREQ of the pacer
//     slice (~22050 Hz).
//   * "ctrl" DMA channel: when the audio channel finishes a pass over
//     the buffer, ctrl writes the buffer's start address back into the
//     audio channel's read-address-trigger register. That instantly
//     restarts the audio channel from the top.
//
// Result: the audio buffer plays in a tight loop with zero CPU
// involvement, no IRQ, no shared-handler dance. The mixer just writes
// fresh samples into the buffer in the foreground (called from
// sfx_Update / on demand from the engine).

#define PWM_AUDIO_PIN       PICOBOY_PIN_SPEAKER     // GP15
#define PWM_TOP             255                     // 8-bit PWM resolution
#define SAMPLE_RATE_HZ      22050
#define BUFFER_SAMPLES      512                     // 23 ms loop

// Single mono buffer, played in an endless DMA loop. Init to silence.
static uint16_t audio_buffer[BUFFER_SAMPLES] __attribute__((aligned(4)));
static volatile uint32_t audio_buffer_addr;        // for ctrl DMA reload
static int audio_dma_chan = -1;
static int ctrl_dma_chan  = -1;

// Forward decls
static void mix_audio_buffer(int16_t *mono_s16, int n);
static int  pwm_audio_init(void);

static int pwm_audio_init(void)
{
    // IMPORTANT: no floating point in here. This build links pico_float_none
    // (-msoft-float, LIB_PICO_FLOAT_NONE) because Doom is pure fixed-point, so
    // any float op calls a stubbed soft-float routine that panics/hangs. The
    // original code used pwm_config_set_clkdiv(...) with a float divider,
    // which is exactly what froze sound init. We use the integer int.frac4
    // clkdiv API instead.

    gpio_set_function(PWM_AUDIO_PIN, GPIO_FUNC_PWM);
    uint audio_slice = pwm_gpio_to_slice_num(PWM_AUDIO_PIN);
    uint audio_chan  = pwm_gpio_to_channel(PWM_AUDIO_PIN);

    pwm_config audio_cfg = pwm_get_default_config();
    pwm_config_set_wrap(&audio_cfg, PWM_TOP);
    pwm_config_set_clkdiv_int(&audio_cfg, 1);       // integer div = 1 (no float)
    pwm_init(audio_slice, &audio_cfg, true);
    pwm_set_chan_level(audio_slice, audio_chan, PWM_TOP / 2);


    uint pacer_slice = (audio_slice + 1) & 7;
    pwm_config pacer_cfg = pwm_get_default_config();
    pwm_config_set_wrap(&pacer_cfg, 999);
    // Pacer wraps 1000 counts and must overflow at SAMPLE_RATE_HZ, so
    // clkdiv = sysclk / (1000 * SAMPLE_RATE_HZ). Computed in 1/16 units as
    // an int.frac4 divider -- entirely integer arithmetic.
    uint32_t sysclock_hz = clock_get_hz(clk_sys);
    uint32_t denom       = 1000u * SAMPLE_RATE_HZ;
    uint64_t clkdiv_x16  = ((uint64_t)sysclock_hz * 16u + denom / 2) / denom;
    if (clkdiv_x16 < 16) clkdiv_x16 = 16;           // clamp to >= 1.0
    if (clkdiv_x16 > 255u * 16u + 15u) clkdiv_x16 = 255u * 16u + 15u;
    pwm_config_set_clkdiv_int_frac4(&pacer_cfg,
                                    (uint32_t)(clkdiv_x16 / 16),
                                    (uint8_t)(clkdiv_x16 % 16));
    pwm_init(pacer_slice, &pacer_cfg, true);


    // Initialise the buffer with silence (mid-rail).
    for (int i = 0; i < BUFFER_SAMPLES; i++) audio_buffer[i] = PWM_TOP / 2;

    audio_dma_chan = dma_claim_unused_channel(true);
    ctrl_dma_chan  = dma_claim_unused_channel(true);
    audio_buffer_addr = (uint32_t)audio_buffer;


    // ---- audio channel: stream samples to PWM cc -----------------------
    dma_channel_config audio_cfg2 = dma_channel_get_default_config(audio_dma_chan);
    channel_config_set_transfer_data_size(&audio_cfg2, DMA_SIZE_16);
    channel_config_set_read_increment(&audio_cfg2, true);
    channel_config_set_write_increment(&audio_cfg2, false);
    channel_config_set_dreq(&audio_cfg2, DREQ_PWM_WRAP0 + pacer_slice);
    // When the audio channel finishes one pass over the buffer, fire the
    // ctrl channel so it reloads our read pointer.
    channel_config_set_chain_to(&audio_cfg2, ctrl_dma_chan);

    volatile void *write_addr = &pwm_hw->slice[audio_slice].cc;
    if (audio_chan == PWM_CHAN_B) {
        write_addr = (volatile uint8_t *)write_addr + 2;
    }

    dma_channel_configure(
        audio_dma_chan, &audio_cfg2,
        write_addr,
        audio_buffer,
        BUFFER_SAMPLES,
        false   // don't start; ctrl will trigger us
    );


    // ---- ctrl channel: write our buffer addr back to audio's al3_read_addr_trig
    // The "al3_read_addr_trig" alias of the audio channel's config writes
    // the read address AND restarts the channel in one go. By chaining
    // ctrl from audio, we get a perpetual playback loop.
    dma_channel_config ctrl_cfg = dma_channel_get_default_config(ctrl_dma_chan);
    channel_config_set_transfer_data_size(&ctrl_cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&ctrl_cfg, false);
    channel_config_set_write_increment(&ctrl_cfg, false);
    // ctrl runs unpaced; one transfer per trigger from audio's chain.

    dma_channel_configure(
        ctrl_dma_chan, &ctrl_cfg,
        &dma_hw->ch[audio_dma_chan].al3_read_addr_trig,
        &audio_buffer_addr,
        1,
        false
    );

    // Kick off the audio channel; it will then auto-loop forever.
    dma_channel_start(audio_dma_chan);

    return 1;
}

// ---------------------------------------------------------------------------
// ADPCM decoder (adapted from rp2040-doom's i_picosound.c)
// ---------------------------------------------------------------------------

#define ADPCM_BLOCK_SIZE              128
#define ADPCM_SAMPLES_PER_BLOCK_SIZE  249
// MIN comes from the pico-sdk's pico/platform/compiler.h

#define CLIP(d, lo, hi) do { if ((d) < (lo)) (d) = (lo); else if ((d) > (hi)) (d) = (hi); } while (0)

static const uint16_t step_table[89] = {
    7, 8, 9, 10, 11, 12, 13, 14,
    16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66,
    73, 80, 88, 97, 107, 118, 130, 143,
    157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658,
    724, 796, 876, 963, 1060, 1166, 1282, 1411,
    1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,
    3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484,
    7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
    32767
};

static const int index_table[] = {
    -1, -1, -1, -1, 2, 4, 6, 8
};

static int adpcm_decode_block_s8(int8_t *outbuf, const uint8_t *inbuf, int inbufsize)
{
    int samples = 1, chunks;
    if (inbufsize < 4) return 0;

    int32_t pcmdata = (int16_t)(inbuf[0] | (inbuf[1] << 8));
    *outbuf++ = pcmdata >> 8u;
    int index = inbuf[2];
    if (index < 0 || index > 88 || inbuf[3]) return 0;

    inbufsize -= 4;
    inbuf += 4;
    chunks = inbufsize / 4;
    samples += chunks * 8;

    while (chunks--) {
        for (int i = 0; i < 4; ++i) {
            int step = step_table[index], delta = step >> 3;
            if (*inbuf & 1) delta += (step >> 2);
            if (*inbuf & 2) delta += (step >> 1);
            if (*inbuf & 4) delta += step;
            if (*inbuf & 8) delta = -delta;
            pcmdata += delta;
            index += index_table[*inbuf & 0x7];
            CLIP(index, 0, 88);
            CLIP(pcmdata, -32768, 32767);
            outbuf[i * 2] = pcmdata >> 8u;

            step = step_table[index]; delta = step >> 3;
            if (*inbuf & 0x10) delta += (step >> 2);
            if (*inbuf & 0x20) delta += (step >> 1);
            if (*inbuf & 0x40) delta += step;
            if (*inbuf & 0x80) delta = -delta;
            pcmdata += delta;
            index += index_table[(*inbuf >> 4) & 0x7];
            CLIP(index, 0, 88);
            CLIP(pcmdata, -32768, 32767);
            outbuf[i * 2 + 1] = pcmdata >> 8u;
            inbuf++;
        }
        outbuf += 8;
    }
    return samples;
}

// ---------------------------------------------------------------------------
// SFX channel state
// ---------------------------------------------------------------------------

#define NUM_SFX_CHANNELS  8

typedef struct {
    const uint8_t *data;
    const uint8_t *data_end;
    uint32_t       offset;          // 16.16 fixed-point sample index
    uint32_t       step;            // 16.16 increment per output sample
    uint8_t        volume;          // 0..255
    uint8_t        decompressed_size;
    int8_t         decompressed[ADPCM_SAMPLES_PER_BLOCK_SIZE];
} sfx_channel_t;

static sfx_channel_t sfx_channels[NUM_SFX_CHANNELS];
static bool          sound_initialized = false;
static bool          use_sfx_prefix    = false;

static inline bool channel_active(int ch) {
    return sfx_channels[ch].decompressed_size != 0;
}

static inline void channel_stop(int ch) {
    sfx_channels[ch].decompressed_size = 0;
}

static void decompress_next(sfx_channel_t *c)
{
    if (c->data == c->data_end) {
        c->decompressed_size = 0;
        return;
    }
    int block = MIN(ADPCM_BLOCK_SIZE, c->data_end - c->data);
    c->decompressed_size = adpcm_decode_block_s8(c->decompressed, c->data, block);
    c->data += block;
    if (!c->decompressed_size) {
        c->decompressed_size = 0;
    }
}

// Mix all active channels into a mono int16 buffer of `n` samples.
// Each output sample is the sum of all active channels' current sample
// scaled by their volume. Output is then offset and clamped to fit
// 8-bit unsigned for PWM (0..255, with 128 = silence).
static void mix_audio_buffer(int16_t *mono_s16, int n)
{
    memset(mono_s16, 0, sizeof(int16_t) * n);

    for (int ch = 0; ch < NUM_SFX_CHANNELS; ch++) {
        if (!channel_active(ch)) continue;
        sfx_channel_t *c = &sfx_channels[ch];
        uint vol = c->volume;
        uint32_t offset_end = c->decompressed_size * 65536u;

        for (int s = 0; s < n; s++) {
            int sample = c->decompressed[c->offset >> 16];
            mono_s16[s] += (int16_t)(sample * vol);   // sample is int8 (-128..127), vol 0..255 -> -32768..32513
            c->offset += c->step;
            while (c->offset >= offset_end) {
                c->offset -= offset_end;
                decompress_next(c);
                if (!c->decompressed_size) {
                    channel_stop(ch);
                    goto next_channel;
                }
                offset_end = c->decompressed_size * 65536u;
            }
        }
        next_channel: ;
    }
}

// ---------------------------------------------------------------------------
// Buffer refill -- called from sfx_Update (which the game calls each frame).
//
// We track our own "play cursor" approximation by reading the audio
// channel's current read address. We mix from the cursor forward up to
// some safe distance so we don't overwrite samples that DMA hasn't read
// yet. With a 23 ms loop and Doom's ~35 fps tic rate (28 ms per tic),
// one mix-per-tic over ~half the buffer is comfortably ahead.
// ---------------------------------------------------------------------------

static int16_t mix_scratch[BUFFER_SAMPLES];
static int     mix_cursor = 0;       // next sample we'll write

static void refill_some(int n_samples)
{
    if (n_samples <= 0 || n_samples > BUFFER_SAMPLES) n_samples = BUFFER_SAMPLES / 2;

    mix_audio_buffer(mix_scratch, n_samples);

    for (int i = 0; i < n_samples; i++) {
        // Volume scaling: mix_scratch[i] is the sum of (int8 * vol) for
        // all active channels. With one channel at full volume the raw
        // value is up to ~32k; with 8 channels max ~256k. We shift down
        // to fit the PWM 8-bit range (mid-rail at PWM_TOP/2). Smaller
        // shift = louder; clipping (clamp below) catches overflow.
        int32_t v = mix_scratch[i];
        v >>= 5;
        v += PWM_TOP / 2;
        if (v < 0) v = 0;
        if (v > PWM_TOP) v = PWM_TOP;
        audio_buffer[mix_cursor] = (uint16_t)v;
        mix_cursor++;
        if (mix_cursor >= BUFFER_SAMPLES) mix_cursor = 0;
    }
}

// ---------------------------------------------------------------------------
// sound_module_t implementation -- called by i_sound.c dispatcher
// ---------------------------------------------------------------------------

static snddevice_t sfx_devices[] = { SNDDEVICE_SB };

static void GetSfxLumpName(const sfxinfo_t *sfx, char *buf, size_t buf_len)
{
    if (use_sfx_prefix) {
        M_snprintf(buf, buf_len, "ds%s", DEH_String(sfx->name));
    } else {
        M_StringCopy(buf, DEH_String(sfx->name), buf_len);
    }
}

static int sfx_GetSfxLumpNum(should_be_const sfxinfo_t *sfx)
{
    char namebuf[9];
    GetSfxLumpName(sfx, namebuf, sizeof(namebuf));
    return W_GetNumForName(namebuf);
}

static boolean sfx_Init(boolean _use_sfx_prefix)
{
    use_sfx_prefix = _use_sfx_prefix;
    if (sound_initialized) return true;

#ifndef PICOBOY_DISABLE_AUDIO
    if (!pwm_audio_init()) return false;
    sound_initialized = true;
    return true;
#else
    // Audio bypass: report success so the engine doesn't keep trying
    // other modules, but never play any sound. Used to isolate PWM
    // audio init bugs from the rest of the boot.
    return true;
#endif
}

static void sfx_Shutdown(void) { /* not implemented */ }
static void sfx_Update(void)
{
#ifdef PICOBOY_DISABLE_AUDIO
    return;
#else
    if (!sound_initialized) return;
    // Refill ~half the loop buffer per call. At Doom's ~35 fps the mix
    // cursor stays a comfortable distance ahead of the DMA play cursor.
    refill_some(BUFFER_SAMPLES / 2);
#endif
}

static void sfx_UpdateSoundParams(int channel, int vol, int sep)
{
    (void) sep;        // mono output, separation ignored
    if ((unsigned)channel >= NUM_SFX_CHANNELS) return;
    if (vol < 0)   vol = 0;
    if (vol > 127) vol = 127;
    sfx_channels[channel].volume = (uint8_t)(vol * 2);   // 0..127 -> 0..254
}

static int sfx_StartSound(should_be_const sfxinfo_t *sfxinfo,
                          int channel, int vol, int sep, int pitch)
{
#ifdef PICOBOY_DISABLE_AUDIO
    (void) sfxinfo; (void) channel; (void) vol; (void) sep; (void) pitch;
    return -1;
#else
    if (!sound_initialized) return -1;
    if ((unsigned)channel >= NUM_SFX_CHANNELS) return -1;

    int lumpnum = sfx_mut(sfxinfo)->lumpnum;
    int lumplen = W_LumpLength(lumpnum);
    const uint8_t *data = W_CacheLumpNum(lumpnum, PU_STATIC);

    if (lumplen < 8 || data[0] != 0x03 || data[1] != 0x80) {
        return -1;
    }

    sfx_channel_t *c = &sfx_channels[channel];
    channel_stop(channel);

    int length = lumplen - 8;
    c->data     = data + 8;
    c->data_end = c->data + length;

    uint32_t sample_freq = (data[3] << 8) | data[2];
    if (pitch == NORM_PITCH) {
        c->step = sample_freq * 65536u / SAMPLE_RATE_HZ;
    } else {
        c->step = (uint32_t)((uint64_t)sample_freq * pitch * 65536ull
                             / ((uint64_t)SAMPLE_RATE_HZ * NORM_PITCH));
    }
    c->offset = 0;
    decompress_next(c);

    sfx_UpdateSoundParams(channel, vol, sep);
    return channel;
#endif
}

static void sfx_StopSound(int channel)
{
    if ((unsigned)channel < NUM_SFX_CHANNELS) channel_stop(channel);
}

static boolean sfx_SoundIsPlaying(int channel)
{
    if ((unsigned)channel >= NUM_SFX_CHANNELS) return false;
    return channel_active(channel);
}

static void sfx_CacheSounds(should_be_const sfxinfo_t *sounds, int num)
{
    (void) sounds; (void) num;
}

sound_module_t sound_pico_module =
{
    sfx_devices,
    sizeof(sfx_devices) / sizeof(*sfx_devices),
    sfx_Init,
    sfx_Shutdown,
    sfx_GetSfxLumpNum,
    sfx_Update,
    sfx_UpdateSoundParams,
    sfx_StartSound,
    sfx_StopSound,
    sfx_SoundIsPlaying,
    sfx_CacheSounds,
};

// ---- PicoSound helpers (called directly from opl_pico.c & pd_render.cpp) --

bool I_PicoSoundIsInitialized(void)         { return sound_initialized; }
void I_PicoSoundSetMusicGenerator(void (*g)(struct audio_buffer *b)) { (void) g; }
#if PICO_ON_DEVICE
void I_PicoSoundFade(bool in)               { (void) in; }
bool I_PicoSoundFading(void)                { return false; }
#endif
