/* c64_video.c -- see c64_video.h */
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/sync.h"
#include "pico/multicore.h"
#include "board.h"
#include "color565.h"
#include "display.h"
#include "c64_glue.h"
#include "c64_video.h"
#include "audio_pwm.h"

#define SRC_X  32
#define SRC_Y  16
#define OUT_W  240
#define OUT_H  180
#define OUT_Y  ((PBC_LCD_H - OUT_H) / 2)

/* C64 "Pepto" palette, as in frank-c64 */
static const uint32_t pepto[16] = {
    0x000000, 0xFFFFFF, 0x68372B, 0x70A4B2, 0x6F3D86, 0x588D43, 0x352879, 0xB8C76F,
    0x6F4F25, 0x433900, 0x9A6759, 0x444444, 0x6C6C6C, 0x9AD284, 0x6C5EB5, 0x959595,
};
static uint16_t pal[256];              /* indexed by the raw byte, masked to 16 colours */

static volatile bool frame_ready;
static volatile bool core1_ready;
static char          status_text[32];
static volatile bool status_dirty;
static uint16_t __aligned(4) rowbuf[2][OUT_W];

/* Output index -> (first, second) source index, groups of 4 -> 3:
 * a | (b+c)/2 | d  (the same mapping megadrive uses for H40). */
static uint16_t xa[OUT_W], xb[OUT_W], ya[OUT_H], yb[OUT_H];

static void build_map(uint16_t *a, uint16_t *b, int n)
{
    for (int i = 0; i < n; i++) {
        int g = i / 3, j = i % 3;
        a[i] = (uint16_t)(4 * g + (j == 0 ? 0 : j == 1 ? 1 : 3));
        b[i] = (uint16_t)(4 * g + (j == 0 ? 0 : j == 1 ? 2 : 3));
    }
}

void c64_video_wait_ready(void)
{
    while (!core1_ready)
        tight_loop_contents();
}

bool c64_video_present(void)
{
    if (frame_ready)
        return false;
    __dmb();
    frame_ready = true;
    __sev();
    return true;
}

void c64_video_set_status(const char *s)
{
    strncpy(status_text, s, sizeof status_text - 1);
    status_text[sizeof status_text - 1] = 0;
    status_dirty = true;
    __sev();
}

static void __not_in_flash_func(convert_row)(int r, uint16_t *out)
{
    const uint8_t *ra = g_pixels + (SRC_Y + ya[r]) * C64_FB_W + SRC_X;
    const uint8_t *rb = g_pixels + (SRC_Y + yb[r]) * C64_FB_W + SRC_X;
    for (int x = 0; x < OUT_W; x++) {
        int i0 = xa[x], i1 = xb[x];
        uint16_t top = pbc_avg565(pal[ra[i0]], pal[ra[i1]]);
        uint16_t bot = pbc_avg565(pal[rb[i0]], pal[rb[i1]]);
        out[x] = pbc_avg565(top, bot);
    }
}

void __not_in_flash_func(c64_video_core1_main)(void)
{
    for (int i = 0; i < 256; i++)
        pal[i] = pbc_rgb24(pepto[i & 15]);
    build_map(xa, xb, OUT_W);
    build_map(ya, yb, OUT_H);

    /* The 22 kHz sample timer runs here, not on core 0: core 0's stack goes
     * deep while the CPU emulation reaches into the disk drive and FatFs,
     * and an interrupt landing at that moment could overrun its 4 KB. Core
     * 1 has a shallow stack and time to spare. */
    audio_pwm_set_pool(alarm_pool_create_with_unused_hardware_alarm(4));

    /* Disk writes park this core while a flash sector is rewritten. */
    multicore_lockout_victim_init();
    __dmb();
    core1_ready = true;

    for (;;) {
        if (!frame_ready) {
            if (status_dirty) {
                status_dirty = false;
                pbc_display_fill(0, PBC_LCD_H - 22, PBC_LCD_W, 8, PBC_BLACK);
                pbc_display_text_centered(PBC_LCD_H - 22, status_text, PBC_GREY, PBC_BLACK, 1);
            }
            __wfe();
            continue;
        }
        __dmb();
        pbc_display_window(0, OUT_Y, OUT_W, OUT_H);
        pbc_display_begin();
        int b = 0;
        for (int y = 0; y < OUT_H; y++) {
            convert_row(y, rowbuf[b]);  /* the DMA sends the other buffer meanwhile */
            pbc_display_wait();
            pbc_display_send(rowbuf[b], OUT_W);
            b ^= 1;
        }
        pbc_display_end();
        __dmb();
        frame_ready = false;
    }
}
