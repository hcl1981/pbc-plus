/* display.c -- ST7789 am SPI0.
 *
 * Wichtige Punkte aus CLAUDE.md Abschnitt 3, die hier verdrahtet sind:
 *   - CS ist ein normaler GPIO (GP10), nicht SPI-CSn.
 *   - Backlight GP26 muss per PWM an, sonst bleibt das Bild schwarz, egal wie
 *     korrekt der SPI-Verkehr ist.
 *   - MADCTL 0xC8 setzt das BGR-Bit; die Umrechnung nach RGB565 passiert
 *     genau an einer Stelle (src/palette.h), nicht hier.
 *   - Sichtbar sind die Controller-Zeilen 20..299, daher der Y-Versatz.
 *   - Kein Vollbildpuffer: die Hauptschleife schiebt 8-Zeilen-Streifen.
 */
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "board.h"
#include "display.h"
#include "../font.h"

static int dma_ch = -1;
static bool dma_running;

static inline void cs(bool low)  { gpio_put(PBC_PIN_LCD_CS, !low); }
static inline void dc(bool data) { gpio_put(PBC_PIN_LCD_DC, data); }

static void spi_bits(int bits)
{
    spi_set_format(PBC_SPI, (uint)bits, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
}

static void wr_cmd(uint8_t c)
{
    dc(false);
    spi_write_blocking(PBC_SPI, &c, 1);
    dc(true);
}

static void wr_data(const uint8_t *d, size_t n)
{
    dc(true);
    spi_write_blocking(PBC_SPI, d, n);
}

static void cmd(uint8_t c, const uint8_t *d, size_t n)
{
    cs(true);
    wr_cmd(c);
    if (n)
        wr_data(d, n);
    cs(false);
}

void pbc_display_backlight(uint8_t level)
{
    pwm_set_gpio_level(PBC_PIN_LCD_BL, (uint16_t)level * level);   /* grob gammakorrigiert */
}

void pbc_display_init(void)
{
    /* --- Pins --- */
    gpio_init(PBC_PIN_LCD_CS);  gpio_set_dir(PBC_PIN_LCD_CS, GPIO_OUT);  gpio_put(PBC_PIN_LCD_CS, 1);
    gpio_init(PBC_PIN_LCD_DC);  gpio_set_dir(PBC_PIN_LCD_DC, GPIO_OUT);  gpio_put(PBC_PIN_LCD_DC, 1);
    gpio_init(PBC_PIN_LCD_RST); gpio_set_dir(PBC_PIN_LCD_RST, GPIO_OUT); gpio_put(PBC_PIN_LCD_RST, 1);

    spi_init(PBC_SPI, PBC_SPI_HZ);
    spi_bits(8);
    gpio_set_function(PBC_PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PBC_PIN_MOSI, GPIO_FUNC_SPI);

    /* --- Backlight: erst am Ende hochfahren, damit kein Muell zu sehen ist --- */
    gpio_set_function(PBC_PIN_LCD_BL, GPIO_FUNC_PWM);
    {
        uint slice = pwm_gpio_to_slice_num(PBC_PIN_LCD_BL);
        pwm_config c = pwm_get_default_config();
        pwm_config_set_wrap(&c, 65535);
        pwm_init(slice, &c, true);
    }
    pbc_display_backlight(0);

    /* --- Reset --- */
    gpio_put(PBC_PIN_LCD_RST, 1); sleep_ms(10);
    gpio_put(PBC_PIN_LCD_RST, 0); sleep_ms(10);
    gpio_put(PBC_PIN_LCD_RST, 1); sleep_ms(120);

    cmd(0x01, NULL, 0); sleep_ms(150);                 /* SWRESET            */
    cmd(0x11, NULL, 0); sleep_ms(120);                 /* SLPOUT             */
    { uint8_t v = 0x55; cmd(0x3A, &v, 1); }            /* COLMOD RGB565      */
    { uint8_t v = 0xC8; cmd(0x36, &v, 1); }            /* MADCTL MY|MX|BGR   */
    cmd(0x21, NULL, 0);                                /* INVON (IPS)        */
    cmd(0x13, NULL, 0);                                /* NORON              */
    cmd(0x29, NULL, 0); sleep_ms(20);                  /* DISPON             */

    dma_ch = dma_claim_unused_channel(true);

    /* Voller Controller-Speicher 240x320 loeschen, nicht nur 240x280 --
     * sonst steht in den unsichtbaren Zeilen alter Inhalt, der beim Scrollen
     * oder nach einem Soft-Reset auftaucht. */
    {
        static uint16_t zero[PBC_LCD_W];
        int y;
        memset(zero, 0, sizeof zero);
        pbc_display_window(0, -PBC_LCD_YOFF, PBC_LCD_W, 320);
        pbc_display_begin();
        for (y = 0; y < 320; y++) {
            pbc_display_send(zero, PBC_LCD_W);
            pbc_display_wait();
        }
        pbc_display_end();
    }
}

void pbc_display_window(int x, int y, int w, int h)
{
    int x1 = x + w - 1;
    int y0 = y + PBC_LCD_YOFF, y1 = y0 + h - 1;
    uint8_t d[4];

    d[0] = (uint8_t)(x >> 8);  d[1] = (uint8_t)x;
    d[2] = (uint8_t)(x1 >> 8); d[3] = (uint8_t)x1;
    cmd(0x2A, d, 4);                                   /* CASET              */

    d[0] = (uint8_t)(y0 >> 8); d[1] = (uint8_t)y0;
    d[2] = (uint8_t)(y1 >> 8); d[3] = (uint8_t)y1;
    cmd(0x2B, d, 4);                                   /* RASET              */

}

/* CS bleibt vom RAMWR bis pbc_display_end() durchgehend aktiv -- der
 * Controller sieht damit einen einzigen Schreibvorgang. */
void pbc_display_begin(void)
{
    cs(true);
    wr_cmd(0x2C);                                      /* RAMWR              */
    dc(true);
    spi_bits(16);                                      /* halb so viele FIFO-Zugriffe */
}

void pbc_display_send(const uint16_t *px, int n)
{
    dma_channel_config c = dma_channel_get_default_config((uint)dma_ch);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, spi_get_dreq(PBC_SPI, true));
    dma_channel_configure((uint)dma_ch, &c, &spi_get_hw(PBC_SPI)->dr, px, (uint)n, true);
    dma_running = true;
}

void pbc_display_wait(void)
{
    if (!dma_running)
        return;
    dma_channel_wait_for_finish_blocking((uint)dma_ch);
    dma_running = false;
}

void pbc_display_end(void)
{
    pbc_display_wait();
    while (spi_is_busy(PBC_SPI))
        tight_loop_contents();
    spi_bits(8);
    cs(false);
}

/* ---- blockierende Notausgabe (Fehlerbildschirm) ------------------------- */
void pbc_display_fill(int x, int y, int w, int h, uint16_t colour)
{
    int i;
    static uint16_t line[PBC_LCD_W];
    if (w > PBC_LCD_W) w = PBC_LCD_W;
    for (i = 0; i < w; i++)
        line[i] = colour;
    pbc_display_window(x, y, w, h);
    pbc_display_begin();
    for (i = 0; i < h; i++)
        spi_write16_blocking(PBC_SPI, line, (size_t)w);
    pbc_display_end();
}

void pbc_display_text(int x, int y, const font_t *f, const char *s, uint16_t fg, uint16_t bg)
{
    static uint16_t line[PBC_LCD_W];       /* eine Bildzeile, kein Textpuffer */
    int w = font_text_w(f, s) + 6;
    int h = f->line_h;
    int py;

    if (w <= 6 || w > PBC_LCD_W)
        return;

    pbc_display_window(x, y, w, h);
    pbc_display_begin();
    for (py = 0; py < h; py++) {
        int i;
        for (i = 0; i < w; i++)
            line[i] = bg;
        font_row(line, w, py, 3, f->ascent, f, s, fg);
        spi_write16_blocking(PBC_SPI, line, (size_t)w);
    }
    pbc_display_end();
}
