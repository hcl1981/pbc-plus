/*
 * st7789_picoboy.h
 *
 * Minimaler ST7789-Treiber fuer PicoBoy Color, optimiert auf scanline-
 * basiertes Rendering wie es Peanut-GB liefert (eine Zeile auf einmal).
 *
 * - SPI0 polled write16, kein DMA (DMA bringt bei 240 Pixel pro Zeile
 *   wenig: das Setup kostet so viel wie der Write).
 * - Adressfenster wird einmal beim Frame-Start gesetzt, dann werden
 *   Zeilen einfach hintereinander geschoben (RAMWR akzeptiert das).
 * - Y-Offset +20 ist hart eingebaut.
 *
 * Single-Header: in genau EINER .cpp-Datei vor dem Include
 *   #define ST7789_PICOBOY_IMPL
 * setzen.
 */

#ifndef ST7789_PICOBOY_H
#define ST7789_PICOBOY_H

#include <pico/stdlib.h>
#include <hardware/spi.h>
#include <hardware/gpio.h>
#include "picoboy_pins.h"

#ifdef __cplusplus
extern "C" {
#endif

// SPI-Takt fuer ST7789. 62.5 MHz ist auf den meisten Modulen problemlos,
// 80 MHz funktioniert oft, 125 MHz ist Glueckssache.
#ifndef ST7789_SPI_HZ
#define ST7789_SPI_HZ  62500000u
#endif

void st7789_init(void);
void st7789_set_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void st7789_write_pixels(const uint16_t *px, size_t count);
void st7789_fill(uint16_t color);
void st7789_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void st7789_backlight(bool on);
void st7789_draw_char(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg, uint8_t scale);
void st7789_draw_text(uint16_t x, uint16_t y, const char *s, uint16_t fg, uint16_t bg, uint8_t scale);

// 5-6-5-Helper
static inline uint16_t st7789_color565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

#ifdef __cplusplus
}
#endif

// =====================================================================
//  Implementierung
// =====================================================================
#ifdef ST7789_PICOBOY_IMPL

#include "font8x8.h"

static void st7789_cmd(uint8_t c) {
    gpio_put(PB_PIN_DISP_DC, 0);
    gpio_put(PB_PIN_DISP_CS, 0);
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    spi_write_blocking(spi0, &c, 1);
    gpio_put(PB_PIN_DISP_CS, 1);
}

static void st7789_data(const uint8_t *d, size_t n) {
    gpio_put(PB_PIN_DISP_DC, 1);
    gpio_put(PB_PIN_DISP_CS, 0);
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    spi_write_blocking(spi0, d, n);
    gpio_put(PB_PIN_DISP_CS, 1);
}

static inline void st7789_data_u8(uint8_t v) { st7789_data(&v, 1); }

void st7789_init(void) {
    // GPIOs setzen
    gpio_init(PB_PIN_DISP_CS);  gpio_set_dir(PB_PIN_DISP_CS, true);  gpio_put(PB_PIN_DISP_CS, 1);
    gpio_init(PB_PIN_DISP_DC);  gpio_set_dir(PB_PIN_DISP_DC, true);  gpio_put(PB_PIN_DISP_DC, 1);
    gpio_init(PB_PIN_DISP_RST); gpio_set_dir(PB_PIN_DISP_RST, true); gpio_put(PB_PIN_DISP_RST, 1);
    gpio_init(PB_PIN_DISP_BL);  gpio_set_dir(PB_PIN_DISP_BL, true);  gpio_put(PB_PIN_DISP_BL, 0);

    // SPI0 init: 62.5 MHz, 8-bit Mode 0
    spi_init(spi0, ST7789_SPI_HZ);
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(PB_PIN_DISP_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PB_PIN_DISP_MOSI, GPIO_FUNC_SPI);
    gpio_set_slew_rate(PB_PIN_DISP_SCK,  GPIO_SLEW_RATE_FAST);
    gpio_set_slew_rate(PB_PIN_DISP_MOSI, GPIO_SLEW_RATE_FAST);

    // Hardware-Reset
    gpio_put(PB_PIN_DISP_RST, 1); sleep_ms(5);
    gpio_put(PB_PIN_DISP_RST, 0); sleep_ms(20);
    gpio_put(PB_PIN_DISP_RST, 1); sleep_ms(150);

    // Init-Sequenz
    st7789_cmd(0x01);                // SWRESET
    sleep_ms(150);
    st7789_cmd(0x11);                // SLPOUT
    sleep_ms(120);

    st7789_cmd(0x3A); st7789_data_u8(0x55);   // COLMOD: 16bpp RGB565
    st7789_cmd(0x36); st7789_data_u8(PB_DISP_MADCTL);  // 0xC8: 180 Grad, BGR

    // Porches & Co. - Defaults sind gut, wir setzen nur das Noetige.
    st7789_cmd(0x21);                // INVON (ST7789-Panels brauchen das fast immer)
    st7789_cmd(0x13);                // NORON
    sleep_ms(10);
    st7789_cmd(0x29);                // DISPON
    sleep_ms(120);

    // Backlight an
    gpio_put(PB_PIN_DISP_BL, 1);
}

void st7789_backlight(bool on) {
    gpio_put(PB_PIN_DISP_BL, on ? 1 : 0);
}

void st7789_set_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    uint16_t x0 = x + PB_DISP_X_OFFSET;
    uint16_t x1 = x0 + w - 1;
    uint16_t y0 = y + PB_DISP_Y_OFFSET;
    uint16_t y1 = y0 + h - 1;

    uint8_t buf[4];

    st7789_cmd(0x2A);                                    // CASET
    buf[0] = x0 >> 8; buf[1] = x0 & 0xFF;
    buf[2] = x1 >> 8; buf[3] = x1 & 0xFF;
    st7789_data(buf, 4);

    st7789_cmd(0x2B);                                    // RASET
    buf[0] = y0 >> 8; buf[1] = y0 & 0xFF;
    buf[2] = y1 >> 8; buf[3] = y1 & 0xFF;
    st7789_data(buf, 4);

    // RAMWR senden, CS unten lassen damit der Pixel-Stream offen bleibt.
    gpio_put(PB_PIN_DISP_DC, 0);
    gpio_put(PB_PIN_DISP_CS, 0);
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    uint8_t ramwr = 0x2C;
    spi_write_blocking(spi0, &ramwr, 1);
    gpio_put(PB_PIN_DISP_DC, 1);
    // CS bleibt unten, naechster write_pixels schreibt direkt rein.
}

// Schreibt 'count' Pixel als 16-bit-Worte. Setzt SPI auf 16-bit-Mode.
void st7789_write_pixels(const uint16_t *px, size_t count) {
    spi_set_format(spi0, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    spi_write16_blocking(spi0, px, count);
}

void st7789_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    st7789_set_window(x, y, w, h);
    spi_set_format(spi0, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    // Wir muessen die Farbe in einem kurzen Buffer halten und blockweise senden.
    static uint16_t linebuf[64];
    for (size_t i = 0; i < 64; ++i) linebuf[i] = color;
    uint32_t total = (uint32_t)w * h;
    while (total) {
        uint32_t chunk = total > 64 ? 64 : total;
        spi_write16_blocking(spi0, linebuf, chunk);
        total -= chunk;
    }
    gpio_put(PB_PIN_DISP_CS, 1);
}

void st7789_fill(uint16_t color) {
    st7789_fill_rect(0, 0, PB_DISP_W, PB_DISP_H, color);
}

// Zeichnet ein einzelnes Zeichen aus font8x8_basic. scale 1..4 vergroessert.
void st7789_draw_char(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg, uint8_t scale) {
    if (c < 0x20 || c > 0x7E) c = ' ';
    if (scale == 0) scale = 1;
    if (scale > 4) scale = 4;
    const uint8_t *glyph = font8x8_basic[(uint8_t)c - 0x20];

    static uint16_t cbuf[8 * 8 * 4 * 4];   // max 32x32 = 1024 px
    int idx = 0;
    for (int row = 0; row < 8; ++row) {
        uint8_t bits = glyph[row];
        for (uint8_t sy = 0; sy < scale; ++sy) {
            for (int col = 0; col < 8; ++col) {
                uint16_t color = (bits & (1u << col)) ? fg : bg;
                for (uint8_t sx = 0; sx < scale; ++sx) {
                    cbuf[idx++] = color;
                }
            }
        }
    }
    uint16_t w = 8 * scale;
    uint16_t h = 8 * scale;
    st7789_set_window(x, y, w, h);
    st7789_write_pixels(cbuf, (size_t)w * h);
    gpio_put(PB_PIN_DISP_CS, 1);
}

void st7789_draw_text(uint16_t x, uint16_t y, const char *s, uint16_t fg, uint16_t bg, uint8_t scale) {
    while (*s) {
        st7789_draw_char(x, y, *s, fg, bg, scale);
        x += 8 * scale;
        ++s;
    }
}

#endif // ST7789_PICOBOY_IMPL
#endif // ST7789_PICOBOY_H
