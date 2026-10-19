/* video.c -- see video.h */
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/sync.h"
#include "board.h"
#include "color565.h"
#include "display.h"
#include "audio_pwm.h"
#include "video.h"

#define OUT_W 240
#define OUT_H 216
#define OUT_Y ((PBC_LCD_H - OUT_H) / 2)

static int mode;                           /* VIDEO_* */

static uint16_t __aligned(4) frames[2][GB_H][GB_W];
static int back;                           /* core 0 draws here */
static volatile int front = -1;            /* core 1 sends this, -1 = idle */
static volatile bool core1_ready;

static char          status_text[32];
static volatile bool status_dirty;

static uint16_t __aligned(4) rowbuf[2][OUT_W];

uint16_t *video_back(void)
{
    return &frames[back][0][0];
}

bool video_present(void)
{
    if (front >= 0)
        return false;
    __dmb();
    front = back;
    back ^= 1;
    __sev();
    return true;
}

void video_wait_ready(void)
{
    while (!core1_ready)
        tight_loop_contents();
}

void video_set_mode(int m)
{
    mode = (m >= 0 && m < VIDEO_MODES) ? m : VIDEO_SCALE;
}

void video_set_status(const char *s)
{
    strncpy(status_text, s, sizeof status_text - 1);
    status_text[sizeof status_text - 1] = 0;
    status_dirty = true;
    __sev();
}

/* Two source pixels a, b -> a, (a+b)/2, b. */
static void __not_in_flash_func(scale_row)(const uint16_t *src, uint16_t *out)
{
    for (int x = 0; x < GB_W; x += 2) {
        uint16_t a = src[x], b = src[x + 1];
        out[0] = a;
        out[1] = pbc_avg565(a, b);
        out[2] = b;
        out += 3;
    }
}

static void __not_in_flash_func(blend_rows)(const uint16_t *a, const uint16_t *b, uint16_t *out)
{
    for (int x = 0; x < OUT_W; x++)
        out[x] = pbc_avg565(a[x], b[x]);
}

static inline void __not_in_flash_func(send_row)(uint16_t *buf, int n)
{
    pbc_display_wait();
    pbc_display_send(buf, n);
}

/* 160x144 -> 240x216: rows a, (a+b)/2, b of every two source rows. */
static void __not_in_flash_func(send_scaled)(const uint16_t (*src)[GB_W])
{
    static uint16_t __aligned(4) ra[OUT_W], rb[OUT_W];
    pbc_display_window(0, OUT_Y, OUT_W, OUT_H);
    pbc_display_begin();
    int b = 0;
    for (int y = 0; y < GB_H; y += 2) {
        /* the DMA sends the previous row meanwhile */
        scale_row(src[y], ra);
        scale_row(src[y + 1], rb);
        memcpy(rowbuf[b], ra, sizeof ra);
        send_row(rowbuf[b], OUT_W); b ^= 1;
        blend_rows(ra, rb, rowbuf[b]);
        send_row(rowbuf[b], OUT_W); b ^= 1;
        memcpy(rowbuf[b], rb, sizeof rb);
        send_row(rowbuf[b], OUT_W); b ^= 1;
    }
    pbc_display_end();
}

/* 1:1 in the middle of the panel, straight out of the frame. */
static void __not_in_flash_func(send_original)(const uint16_t (*src)[GB_W])
{
    pbc_display_window((PBC_LCD_W - GB_W) / 2, (PBC_LCD_H - GB_H) / 2, GB_W, GB_H);
    pbc_display_begin();
    pbc_display_send(&src[0][0], GB_W * GB_H);
    pbc_display_wait();
    pbc_display_end();
}

void __not_in_flash_func(video_core1_main)(void)
{
    /* The 22 kHz sample timer runs here, not on core 0, whose stack goes
     * deep in the emulator (see c64). */
    audio_pwm_set_pool(alarm_pool_create_with_unused_hardware_alarm(4));
    /* Save files are written while a game runs: park this core per sector. */
    multicore_lockout_victim_init();
    pbc_display_fill(0, 0, PBC_LCD_W, PBC_LCD_H, PBC_BLACK);
    __dmb();
    core1_ready = true;

    for (;;) {
        int f = front;
        if (f < 0) {
            /* below the picture, in both modes */
            if (status_dirty) {
                status_dirty = false;
                pbc_display_fill(0, PBC_LCD_H - 14, PBC_LCD_W, 8, PBC_BLACK);
                pbc_display_text_centered(PBC_LCD_H - 14, status_text, PBC_GREY, PBC_BLACK, 1);
            }
            __wfe();
            continue;
        }
        __dmb();
        const uint16_t (*src)[GB_W] = frames[f];
        switch (mode) {
        case VIDEO_ORIGINAL: send_original(src); break;
        default:             send_scaled(src); break;
        }
        __dmb();
        front = -1;
    }
}
