// Low-level ST7789 driver for the PBC+ panel (240x280, SPI0, DMA).
//
// Pinout (fixed, matches the PBC+ schematic):
//   SCK   GP18    (SPI0 SCK)
//   MOSI  GP19    (SPI0 TX)
//   CS    GP10
//   DC    GP8
//   RST   GP9
//   BL    GP26    (PWM, dimmable)
//
// Endianness note: pixel data on the wire is big-endian RGB565
// (high byte first). We achieve this for free by using 16-bit SPI
// transfers with a 16-bit DMA channel: the RP2350 reads each 16-bit
// halfword from RAM little-endian (so byte [LO, HI] becomes value
// 0xHILO), and the SPI peripheral in 16-bit MSB-first mode shifts
// out the MSB byte first -- which is the high byte (HI). End result
// on the wire: [HI, LO] = big-endian. No byte-swap needed in code.

#ifndef PBC_DISPLAY_ST7789_H
#define PBC_DISPLAY_ST7789_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define ST7789_WIDTH   240
#define ST7789_HEIGHT  280

// Initialise SPI, GPIOs, DMA channel, PWM backlight, and send the
// ST7789 init sequence. Backlight comes up at full brightness.
// `baud_hz` is the requested SPI clock; the SDK picks the closest
// achievable divisor.
void st7789_init(uint32_t baud_hz);

// Set the active drawing window. Coordinates are inclusive and in
// "user space" (panel offsets are added internally).
void st7789_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

// Stream `npixels` of a single RGB565 colour to the current window
// via DMA. Blocks until done.
void st7789_fill_color(uint16_t color, uint32_t npixels);

// Stream `npixels` of RGB565 pixels (LE in RAM, framebuf-format)
// from the given buffer to the current window via DMA. Blocks.
void st7789_blit_pixels(const uint16_t *pixels, uint32_t npixels);

// Single-pixel write. Sets a 1x1 window and writes one colour.
// Bounds-checked; a pixel outside the panel is ignored.
void st7789_pixel(uint16_t x, uint16_t y, uint16_t color);

// Backlight PWM duty (0..255). 0 = off, 255 = full brightness.
void st7789_set_backlight(uint8_t value);

#endif // PBC_DISPLAY_ST7789_H
