/* pce_glue.c -- host callbacks pce-go expects (pce-go.h), PicoBoy side.
 *
 * Video: pce-go is given a whole frame buffer (video.c) instead of the
 * one-line buffer pico-pcePlus uses, so it can render as many lines per
 * call as it likes.
 *
 * Audio: as in pico-pcePlus, sample-accurate. Before every PSG register
 * write pce-go calls osd_psg_sync() with the frame-relative cycle, and the
 * samples up to that point are synthesised with the old register state; the
 * last scanline flushes the rest of the frame.
 */
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pce-go.h"
#include "pce.h"
#include "psg.h"
#include "audio_pwm.h"
#include "video.h"
#include "pce_glue.h"

int osd_gfx_render_line = 0;

static int cur_w = 256, cur_h = 224;

uint8_t *__not_in_flash_func(osd_gfx_framebuffer)(int width, int height)
{
    cur_w = width;
    cur_h = height;
    return video_back_buffer();
}

void __not_in_flash_func(osd_gfx_lines_rendered)(int first_line, int last_line)
{
    (void)first_line; (void)last_line;  /* the whole frame is shown at once */
}

void osd_input_read(uint8_t joypads[8])
{
    (void)joypads;                      /* main.c sets PCE.Joypad.regs per frame */
}

void osd_vsync(void)
{
}

void pce_glue_frame_size(int *w, int *h)
{
    *w = cur_w;
    *h = cur_h;
}

/* ---- audio ---------------------------------------------------------------- */

/* pce-go counts CPU cycles: 113 per line at 1.79 MHz, 455 after CSH at
 * 7.16 MHz, which nearly every game switches to. frame_cycle below is in
 * the same clock, so the frame length has to follow it too; a fixed
 * 263 * 113 put every PSG write after about line 65 at the frame end. */
#define PCE_CYCLES_PER_FRAME (263 * PCE.Timer.cycles_per_line)
#define PCE_FPS_X10000       598261u            /* 59.8261 Hz */
#define SAMPLES_MAX          400                /* 22222 / 59.8 = 372 */

static int16_t stereo_buf[SAMPLES_MAX * 2];
static int16_t mono_buf[SAMPLES_MAX];
static int audio_pos;
static int samples_this_frame = AUDIO_RATE * 10000 / PCE_FPS_X10000;

/* Exact rational accumulator, so the long-term rate is AUDIO_RATE. */
static int advance_samples(void)
{
    static uint32_t accum;
    accum += (uint32_t)AUDIO_RATE * 10000u;
    int n = (int)(accum / PCE_FPS_X10000);
    accum -= (uint32_t)n * PCE_FPS_X10000;
    return n > SAMPLES_MAX ? SAMPLES_MAX : n;
}

void __not_in_flash_func(osd_psg_sync)(int frame_cycle)
{
    int target = (int)((int64_t)frame_cycle * samples_this_frame / PCE_CYCLES_PER_FRAME);  /* < 2^31 */
    if (target > samples_this_frame)
        target = samples_this_frame;
    int n = target - audio_pos;
    if (n > 0) {
        psg_update(stereo_buf + audio_pos * 2, (size_t)n, 0x3F);
        audio_pos += n;
    }
}

void __not_in_flash_func(osd_psg_scanline)(void)
{
    if (PCE.Scanline >= 262)
        osd_psg_sync(PCE_CYCLES_PER_FRAME);
}

void pce_glue_audio_frame(void)
{
    for (int i = 0; i < audio_pos; i++)
        mono_buf[i] = (int16_t)(((int)stereo_buf[2 * i] + stereo_buf[2 * i + 1]) / 2);
    audio_pwm_push(mono_buf, audio_pos);
    audio_pos = 0;
    samples_this_frame = advance_samples();
}

/* ---- memory ----------------------------------------------------------------
 * pce-go puts CD-ROM buffers and the SuperGrafx's second VRAM in PSRAM via
 * these. Neither is supported here (no PSRAM); plain malloc keeps the core
 * linking, and those paths are never reached for HuCards. */
void *frens_f_malloc(size_t size) { return malloc(size); }
void  frens_f_free(void *ptr)     { free(ptr); }
