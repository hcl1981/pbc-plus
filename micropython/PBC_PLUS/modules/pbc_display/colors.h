// Color constants for the PBC+ display module.
//
// Values are RGB565 in "natural" form (the 16-bit value as decoded
// by framebuf.RGB565 and as accepted by ST7789 over 16-bit SPI).
// The same constant works in both direct draws and framebuffer
// draws -- no byte-swap considerations leak into user code.
//
// Bit layout: RRRRRGGG GGGBBBBB (red=high 5 bits, blue=low 5 bits).

#ifndef PBC_DISPLAY_COLORS_H
#define PBC_DISPLAY_COLORS_H

#define MAKE_RGB565(r, g, b) \
    (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

#define COLOR_BLACK      0x0000
#define COLOR_WHITE      0xFFFF
#define COLOR_RED        0xF800
#define COLOR_GREEN      0x07E0
#define COLOR_BLUE       0x001F
#define COLOR_YELLOW     0xFFE0
#define COLOR_CYAN       0x07FF
#define COLOR_MAGENTA    0xF81F
#define COLOR_ORANGE     0xFD20  // ~ rgb(255, 165, 0)
#define COLOR_PURPLE     0x780F  // ~ rgb(128, 0, 128)
#define COLOR_GRAY       0x8410  // ~ rgb(128, 128, 128)
#define COLOR_DARKGRAY   0x4208  // ~ rgb(64, 64, 64)
#define COLOR_LIGHTGRAY  0xC618  // ~ rgb(192, 192, 192)

#endif // PBC_DISPLAY_COLORS_H
