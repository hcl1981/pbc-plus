//
// PicoBoy Color hardware configuration for rp2040-doom-rp2.
//
// Pinout per official PicoBoy board documentation:
//   SPI0 is connected to the LCD over GP19 (MOSI) and GP18 (SCK).
//   Display chip-select is driven as plain GPIO by GP10.
//
// GPLv2 (same license as rp2040-doom).
//

#ifndef _PICOBOY_CONFIG_H
#define _PICOBOY_CONFIG_H

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------
#define PICOBOY_TFT_W       240
#define PICOBOY_TFT_H       280

#define PICOBOY_VIEW_W      240
#define PICOBOY_VIEW_H      180
#define PICOBOY_VIEW_Y0     50    // (280 - 180) / 2

#define DOOM_SRC_W          320
#define DOOM_SRC_H          200

// PicoBoy Color: SPI0 with SCK=GP18, MOSI=GP19.
// (Earlier attempts on SPI1/GP14/GP11 left the panel completely dark --
//  backlight was on but no SPI bytes ever reached the controller.)
#define PICOBOY_SPI_PORT    spi0
#define PICOBOY_PIN_SCK     18
#define PICOBOY_PIN_MOSI    19
#define PICOBOY_PIN_CS      10    // plain GPIO, not SPI CSn
#define PICOBOY_PIN_DC      8
#define PICOBOY_PIN_RST     9
#define PICOBOY_PIN_BL      26
#define PICOBOY_SPI_HZ      62500000

// ---------------------------------------------------------------------------
// Buttons (active LOW, INPUT_PULLUP)
// ---------------------------------------------------------------------------
#define PICOBOY_PIN_JOY_UP      4
#define PICOBOY_PIN_JOY_DOWN    2
#define PICOBOY_PIN_JOY_LEFT    3
#define PICOBOY_PIN_JOY_RIGHT   1
#define PICOBOY_PIN_JOY_CENTER  0
#define PICOBOY_PIN_BTN_A       27
#define PICOBOY_PIN_BTN_B       28

// ---------------------------------------------------------------------------
// Audio
// ---------------------------------------------------------------------------
#define PICOBOY_PIN_SPEAKER     15

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------
// 0 = normal mode: paint the Doom frame buffer to the panel
// 1 = diagnostic: paint a fixed RGB stripe pattern instead. Use this if
//     you want to verify the display path independently of the renderer.
//
// Display path is now confirmed working with the test pattern. Switch
// back to 0 to render the real Doom frame buffer.
#ifndef PICOBOY_TEST_PATTERN
#define PICOBOY_TEST_PATTERN 0
#endif

#endif // _PICOBOY_CONFIG_H
