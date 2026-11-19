/* audio.c -- the speaker: PWM on GP15 through an RC low-pass.
 *
 * A DMA channel paced by a DMA timer plays a ring of samples for ever, with
 * no interrupt anywhere; the main loop keeps the ring filled a little ahead
 * of where the DMA reads. GP15 is channel B of its slice, so the DMA writes
 * to the upper half of the compare register -- the lower half would be a
 * silent output and no error.
 */
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "board.h"
#include "pbc.h"
#include "../rt.h"

#define NSAMP     2048                      /* a power of two */
#define RING_BITS 12                        /* log2(NSAMP * 2 bytes) */
#define AHEAD     1300                      /* about 60 ms: covers a slow frame */

static uint16_t ring[NSAMP] __attribute__((aligned(4096)));
static int dma_ch = -1, dma_timer = -1;
static bool running;
static uint32_t wr;

static void endless(void)
{
    dma_hw->ch[dma_ch].al1_transfer_count_trig =
        (DMA_CH0_TRANS_COUNT_MODE_VALUE_ENDLESS << DMA_CH0_TRANS_COUNT_MODE_LSB) | NSAMP;
}

bool pbc_audio_init(void)
{
    uint32_t sys = clock_get_hz(clk_sys);
    uint16_t numer = 0, denom = 0;
    for (int i = 0; i < NSAMP; i++)
        ring[i] = 128;

    /* 8 bits of level; the carrier is sys / 256, far above what is heard */
    gpio_set_function(PBC_PIN_AUDIO, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(PBC_PIN_AUDIO);
    pwm_config pc = pwm_get_default_config();
    pwm_config_set_wrap(&pc, 255);
    pwm_config_set_clkdiv_int(&pc, 1);
    pwm_init(slice, &pc, true);
    pwm_set_chan_level(slice, PWM_CHAN_B, 128);

    for (int n = 15; n >= 1; n--) {         /* DMA timer: sys * numer / denom */
        uint64_t d = ((uint64_t)sys * (uint64_t)n) / SND_RATE;
        if (d > 0 && d <= 65535u) {
            numer = (uint16_t)n;
            denom = (uint16_t)d;
            break;
        }
    }
    if (!numer)
        return false;
    dma_timer = dma_claim_unused_timer(false);
    dma_ch = dma_claim_unused_channel(false);
    if (dma_timer < 0 || dma_ch < 0)
        return false;
    dma_timer_set_fraction((uint)dma_timer, numer, denom);

    dma_channel_config c = dma_channel_get_default_config((uint)dma_ch);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_ring(&c, false, RING_BITS);
    channel_config_set_dreq(&c, dma_get_timer_dreq((uint)dma_timer));
    dma_channel_configure((uint)dma_ch, &c, (volatile uint16_t *)&pwm_hw->slice[slice].cc + 1,
                          ring, NSAMP, false);
    endless();
    running = true;
    return true;
}

void pbc_audio_fill(void)
{
    static uint8_t buf[256];
    if (!running)
        return;
    uint32_t rd = (uint32_t)(((uintptr_t)dma_hw->ch[dma_ch].read_addr - (uintptr_t)ring) / 2u) & (NSAMP - 1u);
    int have = (int)((wr - rd) & (NSAMP - 1u));
    /* the DMA has overtaken the writing (a frame took far too long): what
     * looks like a nearly full ring is an empty one */
    if (have > NSAMP - 256) {
        wr = rd;
        have = 0;
    }
    while (have < AHEAD) {
        int n = AHEAD - have > (int)sizeof buf ? (int)sizeof buf : AHEAD - have;
        snd_render(buf, n);
        for (int i = 0; i < n; i++)
            ring[(wr + (uint32_t)i) & (NSAMP - 1u)] = buf[i];
        wr += (uint32_t)n;
        have += n;
    }
    if (!dma_channel_is_busy((uint)dma_ch)) {   /* should it ever stop */
        dma_channel_set_read_addr((uint)dma_ch, ring, false);
        endless();
    }
}
