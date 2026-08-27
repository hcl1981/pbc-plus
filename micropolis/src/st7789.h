#pragma once
#include <stdint.h>

// Native ST7789 driver for the PBC+ panel.
//  - 8-bit SPI for commands/init, switched to 16-bit for the pixel stream
//  - full-frame blit via DMA (framebuffer is plain little-endian RGB565;
//    the 16-bit SPI frame transmits MSB-first, which is the order the
//    ST7789 expects, so no per-pixel byte swapping is needed)

void st7789_init(void);

// Push a full DISPLAY_W*DISPLAY_H RGB565 framebuffer to the panel.
// Blocks until the DMA transfer has completed.
void st7789_blit(const uint16_t *framebuffer);

// Backlight duty 0..255 (PWM on PIN_BL).
void st7789_backlight(uint8_t level);
