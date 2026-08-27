#include "rgb_led.h"
#include "pbc_hw_config.h"

#include "hardware/pio.h"
#include "hardware/clocks.h"

// WS2812 / SK6805-compatible PIO program. Same instruction stream
// the pico-examples ship with — replicated here so we don't need a
// separate .pio file or a pico_generate_pio_header() build step.
//
// Bit period = T1 + T2 + T3 = 2 + 5 + 3 = 10 cycles.
// Clock divider is set so 10 cycles = 1.25 µs (= 800 kHz bit rate).
//
//     out  x, 1   side 0 [2]   ; pull next bit, drive LOW
//     jmp !x, 3   side 1 [1]   ; if 0: short HIGH, jump to long LOW
//     jmp 0       side 1 [4]   ; if 1: long  HIGH, then back to top
//     nop         side 0 [4]   ; (zero-bit tail) long LOW
//
static const uint16_t ws2812_program_instructions[] = {
    0x6221, // out  x, 1   side 0 [2]
    0x1123, // jmp !x, 3   side 1 [1]
    0x1400, // jmp 0       side 1 [4]
    0xa442, // nop         side 0 [4]
};

static const struct pio_program ws2812_program = {
    .instructions = ws2812_program_instructions,
    .length = sizeof(ws2812_program_instructions) / sizeof(uint16_t),
    .origin = -1,
};

static PIO  rgb_pio    = NULL;
static int  rgb_sm     = -1;
static uint rgb_offset = 0;
static bool rgb_initialized = false;

bool rgb_led_init(void) {
    if (rgb_initialized) {
        return true;
    }

    // Try PIO0 first, fall back to PIO1.
    if (pio_can_add_program(pio0, &ws2812_program)) {
        rgb_pio = pio0;
    } else if (pio_can_add_program(pio1, &ws2812_program)) {
        rgb_pio = pio1;
    } else {
        return false;
    }

    rgb_sm = pio_claim_unused_sm(rgb_pio, false);
    if (rgb_sm < 0) {
        rgb_pio = NULL;
        return false;
    }
    rgb_offset = pio_add_program(rgb_pio, &ws2812_program);

    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, rgb_offset, rgb_offset + ws2812_program.length - 1);

    // Side-set: 1 bit, drives the data line.
    sm_config_set_sideset(&c, 1, false, false);
    sm_config_set_sideset_pins(&c, RGB_LED_PIN);

    // OSR shifts left, autopulls every 24 bits — meaning the MSB of
    // the 32-bit FIFO entry is the first bit on the wire. We left-
    // align the GRB payload in rgb_led_set() to match.
    sm_config_set_out_shift(&c, false /* shift_left */, true /* autopull */, 24);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    // 10 cycles per bit at 800 kHz → SM clock = 8 MHz.
    float div = (float)clock_get_hz(clk_sys) / (800000.0f * 10.0f);
    sm_config_set_clkdiv(&c, div);

    pio_gpio_init(rgb_pio, RGB_LED_PIN);
    pio_sm_set_consecutive_pindirs(rgb_pio, rgb_sm, RGB_LED_PIN, 1, true);

    pio_sm_init(rgb_pio, rgb_sm, rgb_offset, &c);
    pio_sm_set_enabled(rgb_pio, rgb_sm, true);

    rgb_initialized = true;

    rgb_led_off();
    return true;
}

void rgb_led_set(uint8_t r, uint8_t g, uint8_t b) {
    if (!rgb_initialized && !rgb_led_init()) {
        return;
    }
    // SK6805 expects GRB order. Pack into the upper 24 bits of a
    // 32-bit word so the OSR clocks them out MSB-first.
    uint32_t pixel = ((uint32_t)g << 16) | ((uint32_t)r << 8) | (uint32_t)b;
    pio_sm_put_blocking(rgb_pio, rgb_sm, pixel << 8);
}

void rgb_led_off(void) {
    rgb_led_set(0, 0, 0);
}
