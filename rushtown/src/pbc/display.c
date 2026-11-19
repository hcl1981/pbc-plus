/* display.c -- ST7789 on SPI0.
 *
 * What the other PBC+ projects found out the hard way: CS is a plain GPIO;
 * the backlight has to be driven by PWM or the panel stays dark; MADCTL
 * 0xC8 sets BGR (the colours are made for that in rt.h); the visible rows
 * start at the controller's row 20; and the whole 240 x 320 of its memory
 * is cleared once, not only what is seen.
 */
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include "board.h"
#include "pbc.h"
#include "../font8x8.h"

static int dma_ch = -1;
static bool sending;

static inline void cs(bool active) { gpio_put(PBC_PIN_LCD_CS, !active); }
static inline void dc(bool data)   { gpio_put(PBC_PIN_LCD_DC, data); }

static void cmd(uint8_t c, const uint8_t *d, size_t n)
{
    cs(true);
    dc(false);
    spi_write_blocking(PBC_SPI, &c, 1);
    dc(true);
    if (n)
        spi_write_blocking(PBC_SPI, d, n);
    cs(false);
}

static void window(int x, int y, int w, int h)
{
    int x1 = x + w - 1, y0 = y + PBC_LCD_YOFF, y1 = y0 + h - 1;
    uint8_t d[4] = { (uint8_t)(x >> 8), (uint8_t)x, (uint8_t)(x1 >> 8), (uint8_t)x1 };
    cmd(0x2A, d, 4);
    d[0] = (uint8_t)(y0 >> 8); d[1] = (uint8_t)y0; d[2] = (uint8_t)(y1 >> 8); d[3] = (uint8_t)y1;
    cmd(0x2B, d, 4);
}

static void begin(void)                     /* RAMWR, then pixels as 16 bit words */
{
    uint8_t c = 0x2C;
    cs(true);
    dc(false);
    spi_write_blocking(PBC_SPI, &c, 1);
    dc(true);
    spi_set_format(PBC_SPI, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
}

static void end(void)
{
    while (spi_is_busy(PBC_SPI))
        tight_loop_contents();
    spi_set_format(PBC_SPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    cs(false);
}

void pbc_display_backlight(uint8_t level)
{
    pwm_set_gpio_level(PBC_PIN_LCD_BL, (uint16_t)level * level);
}

void pbc_display_fill(int x, int y, int w, int h, uint16_t colour)
{
    static uint16_t line[PBC_LCD_W];
    pbc_display_wait();
    for (int i = 0; i < PBC_LCD_W; i++)
        line[i] = colour;
    window(x, y, w, h);
    begin();
    for (int i = 0; i < h; i++)
        spi_write16_blocking(PBC_SPI, line, (size_t)w);
    end();
}

void pbc_display_text(int x, int y, const char *s, uint16_t fg, uint16_t bg)
{
    static uint16_t line[PBC_LCD_W];
    int len = (int)strlen(s);
    pbc_display_wait();
    if (x + len * 8 > PBC_LCD_W)
        len = (PBC_LCD_W - x) / 8;
    if (len <= 0)
        return;
    window(x, y, len * 8, 8);
    begin();
    for (int row = 0; row < 8; row++) {
        for (int c = 0; c < len; c++) {
            unsigned ch = (unsigned char)s[c];
            if (ch < 0x20 || ch > 0x7e)
                ch = '?';
            for (int col = 0; col < 8; col++)
                line[c * 8 + col] = font8x8_basic[ch - 0x20][row] & (1u << col) ? fg : bg;
        }
        spi_write16_blocking(PBC_SPI, line, (size_t)len * 8);
    }
    end();
}

void pbc_display_init(void)
{
    gpio_init(PBC_PIN_LCD_CS);  gpio_set_dir(PBC_PIN_LCD_CS, GPIO_OUT);  gpio_put(PBC_PIN_LCD_CS, 1);
    gpio_init(PBC_PIN_LCD_DC);  gpio_set_dir(PBC_PIN_LCD_DC, GPIO_OUT);  gpio_put(PBC_PIN_LCD_DC, 1);
    gpio_init(PBC_PIN_LCD_RST); gpio_set_dir(PBC_PIN_LCD_RST, GPIO_OUT); gpio_put(PBC_PIN_LCD_RST, 1);

    spi_init(PBC_SPI, PBC_SPI_HZ);
    spi_set_format(PBC_SPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(PBC_PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PBC_PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_slew_rate(PBC_PIN_SCK, GPIO_SLEW_RATE_FAST);
    gpio_set_slew_rate(PBC_PIN_MOSI, GPIO_SLEW_RATE_FAST);

    /* dark until the controller's memory is clean */
    gpio_set_function(PBC_PIN_LCD_BL, GPIO_FUNC_PWM);
    {
        pwm_config c = pwm_get_default_config();
        pwm_config_set_wrap(&c, 65535);
        pwm_init(pwm_gpio_to_slice_num(PBC_PIN_LCD_BL), &c, true);
    }
    pbc_display_backlight(0);

    gpio_put(PBC_PIN_LCD_RST, 1); sleep_ms(10);
    gpio_put(PBC_PIN_LCD_RST, 0); sleep_ms(10);
    gpio_put(PBC_PIN_LCD_RST, 1); sleep_ms(120);
    cmd(0x01, NULL, 0); sleep_ms(150);                  /* SWRESET */
    cmd(0x11, NULL, 0); sleep_ms(120);                  /* SLPOUT */
    { uint8_t v = 0x55; cmd(0x3A, &v, 1); }             /* 16 bits per pixel */
    { uint8_t v = 0xC8; cmd(0x36, &v, 1); }             /* MADCTL MY | MX | BGR */
    cmd(0x21, NULL, 0);                                 /* INVON (IPS) */
    cmd(0x13, NULL, 0);                                 /* NORON */
    cmd(0x29, NULL, 0); sleep_ms(20);                   /* DISPON */

    dma_ch = dma_claim_unused_channel(true);
    pbc_display_fill(0, -PBC_LCD_YOFF, PBC_LCD_W, 320, 0);
}

void pbc_display_wait(void)
{
    if (!sending)
        return;
    dma_channel_wait_for_finish_blocking((uint)dma_ch);
    end();
    sending = false;
}

void pbc_display_frame(const uint16_t *px)
{
    pbc_display_wait();
    window(0, 0, PBC_LCD_W, PBC_LCD_H);
    begin();
    dma_channel_config c = dma_channel_get_default_config((uint)dma_ch);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, spi_get_dreq(PBC_SPI, true));
    dma_channel_configure((uint)dma_ch, &c, &spi_get_hw(PBC_SPI)->dr, px, PBC_LCD_W * PBC_LCD_H, true);
    sending = true;
}
