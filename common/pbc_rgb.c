/* pbc_rgb.c -- see pbc_rgb.h */
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "pbc_rgb.h"

/* ws2812.pio of the SDK's examples (T1 = 2, T2 = 5, T3 = 3), assembled:
 *   bitloop: out x, 1       side 0 [2]
 *            jmp !x do_zero side 1 [1]
 *            jmp bitloop    side 1 [4]
 *   do_zero: nop            side 0 [4]
 * Ten cycles a bit, 800 kHz. */
static const uint16_t ws2812_code[] = { 0x6221, 0x1123, 0x1400, 0xa442 };
static const pio_program_t ws2812_program = {
    .instructions = ws2812_code, .length = 4, .origin = -1,
};

static PIO pio;
static int sm = -1;

static bool rgb_init(void)
{
    uint u, offset;
    if (!pio_claim_free_sm_and_add_program(&ws2812_program, &pio, &u, &offset)) {
        sm = -2;                        /* no PIO free: stay dark */
        return false;
    }
    sm = (int)u;
    pio_gpio_init(pio, PBC_PIN_RGB);
    pio_sm_set_consecutive_pindirs(pio, sm, PBC_PIN_RGB, 1, true);
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset, offset + 3);
    sm_config_set_sideset(&c, 1, false, false);
    sm_config_set_sideset_pins(&c, PBC_PIN_RGB);
    sm_config_set_out_shift(&c, false, true, 24);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    sm_config_set_clkdiv(&c, (float)clock_get_hz(clk_sys) / (800000 * 10));
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
    return true;
}

void pbc_rgb_set(uint8_t r, uint8_t g, uint8_t b)
{
    if (sm == -1)
        rgb_init();
    if (sm < 0 || pio_sm_is_tx_fifo_full(pio, sm))
        return;
    pio_sm_put(pio, sm, (uint32_t)g << 24 | (uint32_t)r << 16 | (uint32_t)b << 8);
}
