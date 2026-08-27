#include "st7789.h"
#include "config.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"

static int s_dma_chan = -1;

// ---- low-level command/data helpers (8-bit SPI) ----------------------
static inline void cs_low(void)  { gpio_put(PIN_CS, 0); }
static inline void cs_high(void) { gpio_put(PIN_CS, 1); }

static void wr_cmd(uint8_t c) {
    gpio_put(PIN_DC, 0);
    cs_low();
    spi_write_blocking(DISPLAY_SPI, &c, 1);
    cs_high();
}

static void wr_data(const uint8_t *d, size_t n) {
    gpio_put(PIN_DC, 1);
    cs_low();
    spi_write_blocking(DISPLAY_SPI, d, n);
    cs_high();
}

static void wr_data8(uint8_t d) { wr_data(&d, 1); }

static void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    x0 += PANEL_COL_OFFSET; x1 += PANEL_COL_OFFSET;
    y0 += PANEL_ROW_OFFSET; y1 += PANEL_ROW_OFFSET;
    uint8_t buf[4];
    wr_cmd(0x2A); // CASET
    buf[0] = x0 >> 8; buf[1] = x0 & 0xFF; buf[2] = x1 >> 8; buf[3] = x1 & 0xFF;
    wr_data(buf, 4);
    wr_cmd(0x2B); // RASET
    buf[0] = y0 >> 8; buf[1] = y0 & 0xFF; buf[2] = y1 >> 8; buf[3] = y1 & 0xFF;
    wr_data(buf, 4);
    wr_cmd(0x2C); // RAMWR
}

void st7789_backlight(uint8_t level) {
    gpio_set_function(PIN_BL, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(PIN_BL);
    pwm_set_wrap(slice, 255);
    pwm_set_gpio_level(PIN_BL, level);
    pwm_set_enabled(slice, true);
}

void st7789_init(void) {
    // SPI bus, 8-bit frames for init/commands
    spi_init(DISPLAY_SPI, DISPLAY_BAUD);
    spi_set_format(DISPLAY_SPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    // Control lines as plain GPIO (driver toggles CS/DC in software)
    gpio_init(PIN_CS);  gpio_set_dir(PIN_CS, GPIO_OUT);  gpio_put(PIN_CS, 1);
    gpio_init(PIN_DC);  gpio_set_dir(PIN_DC, GPIO_OUT);  gpio_put(PIN_DC, 1);
    gpio_init(PIN_RST); gpio_set_dir(PIN_RST, GPIO_OUT);

    // Hardware reset
    gpio_put(PIN_RST, 1); sleep_ms(10);
    gpio_put(PIN_RST, 0); sleep_ms(10);
    gpio_put(PIN_RST, 1); sleep_ms(120);

    wr_cmd(0x01); sleep_ms(120);          // SWRESET
    wr_cmd(0x11); sleep_ms(120);          // SLPOUT
    wr_cmd(0x3A); wr_data8(0x55);         // COLMOD = 16bpp (RGB565)
    wr_cmd(0x36); wr_data8(PANEL_MADCTL); // MADCTL orientation (see config.h)
    wr_cmd(0x21);                         // INVON (ST7789 panels need inversion)
    wr_cmd(0x13);                         // NORON
    sleep_ms(10);
    wr_cmd(0x29); sleep_ms(10);           // DISPON

    // Claim a DMA channel for blits
    s_dma_chan = dma_claim_unused_channel(true);

    st7789_backlight(255);
}

void st7789_blit(const uint16_t *framebuffer) {
    set_window(0, 0, DISPLAY_W - 1, DISPLAY_H - 1);

    // Pixel stream: 16-bit SPI frames, DC=1, CS held low for the burst
    gpio_put(PIN_DC, 1);
    cs_low();
    spi_set_format(DISPLAY_SPI, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    dma_channel_config c = dma_channel_get_default_config(s_dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, spi_get_dreq(DISPLAY_SPI, true));

    dma_channel_configure(
        s_dma_chan, &c,
        &spi_get_hw(DISPLAY_SPI)->dr,   // write to SPI TX FIFO
        framebuffer,                    // read from framebuffer
        (uint32_t)DISPLAY_W * DISPLAY_H,
        true);                          // start now

    dma_channel_wait_for_finish_blocking(s_dma_chan);

    // Wait for SPI to drain, then restore 8-bit framing for next commands
    while (spi_is_busy(DISPLAY_SPI)) tight_loop_contents();
    spi_set_format(DISPLAY_SPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    cs_high();
}
