// ST7789 driver implementation. See st7789.h for design notes.

#include "st7789.h"

#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"

// ---- Pin assignment (fixed for PBC+) -------------------------------------

#define PIN_SCK   18
#define PIN_MOSI  19
#define PIN_CS    10
#define PIN_DC     8
#define PIN_RST    9
#define PIN_BL    26

#define SPI_PORT  spi0

// ---- Panel offsets ------------------------------------------------------
//
// The 240x280 ST7789 panel uses controller rows 20..299 of its 320-row
// frame buffer. With MADCTL = MX|MY (180 deg rotation, matching the
// PBC+ enclosure orientation), the offsets below land pixel (0, 0)
// at the top-left of the visible area as the user sees it.
//
// If a future panel revision flips orientation, only these two values
// (and the MADCTL byte in the init sequence) need to change.

#define X_OFFSET  0
#define Y_OFFSET  20

// ---- ST7789 commands ----------------------------------------------------

#define ST_SWRESET 0x01
#define ST_SLPOUT  0x11
#define ST_NORON   0x13
#define ST_INVON   0x21
#define ST_DISPON  0x29
#define ST_CASET   0x2A
#define ST_RASET   0x2B
#define ST_RAMWR   0x2C
#define ST_MADCTL  0x36
#define ST_COLMOD  0x3A

#define MADCTL_MY  0x80
#define MADCTL_MX  0x40

// ---- State --------------------------------------------------------------

static int dma_chan = -1;
static bool initialised = false;

// ---- Helpers ------------------------------------------------------------

static inline void cs_lo(void) { gpio_put(PIN_CS, 0); }
static inline void cs_hi(void) { gpio_put(PIN_CS, 1); }
static inline void dc_cmd(void) { gpio_put(PIN_DC, 0); }
static inline void dc_data(void) { gpio_put(PIN_DC, 1); }

static inline void spi_set_8bit(void) {
    spi_set_format(SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
}

static inline void spi_set_16bit(void) {
    spi_set_format(SPI_PORT, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
}

static void send_cmd(uint8_t cmd, const uint8_t *data, size_t len) {
    spi_set_8bit();
    cs_lo();
    dc_cmd();
    spi_write_blocking(SPI_PORT, &cmd, 1);
    if (len > 0) {
        dc_data();
        spi_write_blocking(SPI_PORT, data, len);
    }
    cs_hi();
}

// ---- Init ---------------------------------------------------------------

void st7789_init(uint32_t baud_hz) {
    if (initialised) {
        return;
    }

    // Control GPIOs (CS, DC, RST) -- standard digital outputs.
    gpio_init(PIN_CS);   gpio_set_dir(PIN_CS,  GPIO_OUT); gpio_put(PIN_CS,  1);
    gpio_init(PIN_DC);   gpio_set_dir(PIN_DC,  GPIO_OUT); gpio_put(PIN_DC,  1);
    gpio_init(PIN_RST);  gpio_set_dir(PIN_RST, GPIO_OUT); gpio_put(PIN_RST, 1);

    // SPI peripheral.
    spi_init(SPI_PORT, baud_hz);
    spi_set_8bit();
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    // Hardware reset pulse: LOW >= 10us, then >= 120ms before commands.
    sleep_ms(10);
    gpio_put(PIN_RST, 0);
    sleep_ms(50);
    gpio_put(PIN_RST, 1);
    sleep_ms(150);

    // Standard ST7789 init sequence.
    send_cmd(ST_SWRESET, NULL, 0);
    sleep_ms(150);

    send_cmd(ST_SLPOUT, NULL, 0);
    sleep_ms(120);

    uint8_t colmod = 0x55;  // 16-bit / pixel (RGB565)
    send_cmd(ST_COLMOD, &colmod, 1);

    uint8_t madctl = MADCTL_MX | MADCTL_MY;  // 180 deg rotation, RGB
    send_cmd(ST_MADCTL, &madctl, 1);

    send_cmd(ST_INVON, NULL, 0);   // ST7789 panels are inverted by default
    sleep_ms(10);

    send_cmd(ST_NORON, NULL, 0);
    sleep_ms(10);

    send_cmd(ST_DISPON, NULL, 0);
    sleep_ms(100);

    // DMA channel for fast pixel streaming. Claimed for our exclusive
    // use until the firmware reboots.
    dma_chan = dma_claim_unused_channel(true);

    // Backlight on PWM. Default to full brightness.
    gpio_set_function(PIN_BL, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(PIN_BL);
    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_wrap(&cfg, 255);
    pwm_init(slice, &cfg, true);
    pwm_set_gpio_level(PIN_BL, 255);

    initialised = true;
}

// ---- Window setup -------------------------------------------------------

void st7789_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint16_t xs = x0 + X_OFFSET, xe = x1 + X_OFFSET;
    uint16_t ys = y0 + Y_OFFSET, ye = y1 + Y_OFFSET;
    uint8_t buf[4];

    buf[0] = xs >> 8; buf[1] = xs & 0xFF;
    buf[2] = xe >> 8; buf[3] = xe & 0xFF;
    send_cmd(ST_CASET, buf, 4);

    buf[0] = ys >> 8; buf[1] = ys & 0xFF;
    buf[2] = ye >> 8; buf[3] = ye & 0xFF;
    send_cmd(ST_RASET, buf, 4);

    send_cmd(ST_RAMWR, NULL, 0);
}

// ---- Pixel streaming ----------------------------------------------------
//
// Both fill and blit follow the same shape:
//   1. Switch SPI to 16-bit mode (MSB first).
//   2. Pull CS low, set DC high (data phase).
//   3. Configure DMA: 16-bit transfers, paced by SPI TX DREQ.
//   4. Kick off DMA, wait for DMA done, drain SPI FIFO.
//   5. Pull CS high, restore 8-bit SPI mode for subsequent commands.

static void stream_dma_blocking(const volatile void *src, uint32_t count, bool incr) {
    spi_set_16bit();
    cs_lo();
    dc_data();

    dma_channel_config c = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
    channel_config_set_read_increment(&c, incr);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, spi_get_dreq(SPI_PORT, true));

    dma_channel_configure(
        dma_chan, &c,
        &spi_get_hw(SPI_PORT)->dr,  // write to SPI TX FIFO
        src,                         // source: buffer or single value
        count,                       // number of 16-bit transfers
        true                         // start now
    );

    dma_channel_wait_for_finish_blocking(dma_chan);
    while (spi_is_busy(SPI_PORT)) {
        tight_loop_contents();
    }

    cs_hi();
    spi_set_8bit();
}

void st7789_fill_color(uint16_t color, uint32_t npixels) {
    // DMA reads from a single static address (no read increment).
    // `color` lives on the stack here, which is fine -- DMA finishes
    // before we return.
    stream_dma_blocking(&color, npixels, false);
}

void st7789_blit_pixels(const uint16_t *pixels, uint32_t npixels) {
    stream_dma_blocking(pixels, npixels, true);
}

// ---- Single pixel -------------------------------------------------------

void st7789_pixel(uint16_t x, uint16_t y, uint16_t color) {
    if (x >= ST7789_WIDTH || y >= ST7789_HEIGHT) {
        return;
    }
    st7789_set_window(x, y, x, y);
    st7789_fill_color(color, 1);
}

// ---- Backlight ----------------------------------------------------------

void st7789_set_backlight(uint8_t value) {
    pwm_set_gpio_level(PIN_BL, value);
}
