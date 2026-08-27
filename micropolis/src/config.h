#pragma once
// =====================================================================
//  Micropolis-PBC — central configuration
//  Target: PBC+ (RP2350A, ST7789 240x280, joystick + A/B)
//
//  Hardware constants below are taken from the MicroCityPBC Arduino
//  sketch. The ONE value that sketch did not state explicitly is the
//  display SPI bus + SCK/MOSI pins, because the Adafruit library used
//  the arduino-pico default `SPI` (spi0, SCK=GP18, MOSI=GP19). Those
//  defaults are filled in here — if the screen stays black on first
//  flash, this is the first thing to verify against the schematic.
// =====================================================================
// (DISPLAY_SPI below resolves to the Pico SDK `spi0` instance; only
//  st7789.c uses it and that file includes <hardware/spi.h> itself.)

// ---- Display panel ----------------------------------------------------
#define DISPLAY_W        240
#define DISPLAY_H        280
// 1.69" 240x280 ST7789 panels sit in the controller's 240x320 RAM with a
// vertical offset. With rotation 0 the visible window starts at row 20.
#define PANEL_COL_OFFSET 0
#define PANEL_ROW_OFFSET 20
// MADCTL orientation byte. The reference (Adafruit setRotation(0)) maps
// this panel with both axes flipped relative to a raw 0x00, so use 0xC0
// (MY|MX). Bit 3 (0x08 = BGR) stays clear → RGB order. If the image is
// mirrored or upside-down after a panel swap, this is the byte to tweak:
//   0x00 none · 0x40 MX · 0x80 MY · 0xC0 MX+MY(180°) · +0x20 MV(90°)
#define PANEL_MADCTL     0xC0
// Rounded corners: rows reserved top/bottom (matches MicroCityPBC margins)
#define MARGIN_TOP       20
#define MARGIN_BOTTOM    20

// ---- Display SPI (see note above) ------------------------------------
#define DISPLAY_SPI      spi0
#define PIN_SCK          18
#define PIN_MOSI         19
#define PIN_CS           10
#define PIN_DC            8
#define PIN_RST           9
#define PIN_BL           26          // backlight (PWM)
#define DISPLAY_BAUD     62500000u   // 62.5 MHz; pico clamps to clk_peri/2

// ---- Buttons (active LOW, internal pull-ups) -------------------------
#define PIN_BTN_CENTER    0
#define PIN_BTN_RIGHT     1
#define PIN_BTN_DOWN      2
#define PIN_BTN_LEFT      3
#define PIN_BTN_UP        4
#define PIN_BTN_A        27
#define PIN_BTN_B        28
#define PIN_SPEAKER      15

// Input bitmask (matches MicroCityPBC Interface.h ordering)
#define IN_LEFT   (1u << 0)
#define IN_RIGHT  (1u << 1)
#define IN_UP     (1u << 2)
#define IN_DOWN   (1u << 3)
#define IN_A      (1u << 4)
#define IN_B      (1u << 5)

// ---- World / viewport -------------------------------------------------
// Micropolis world is fixed 120 x 100 tiles, tiles are 16x16 px → 1:1
// with the framebuffer (no upscaling). 240/16 = 15 columns visible.
#define WORLD_W   120
#define WORLD_H   100
#define TILE_PX   16

#define VIEW_COLS (DISPLAY_W / TILE_PX)             // 15
#define VIEW_ROWS ((DISPLAY_H + TILE_PX - 1) / TILE_PX) // 18 (covers screen)

#define MAX_SCROLL_X (WORLD_W - VIEW_COLS)          // 105
#define MAX_SCROLL_Y (WORLD_H - VIEW_ROWS)          // 82

// ---- HUD layout (overlays drawn on top of the map) -------------------
#define STATUS_H  22        // top bar: funds + date
#define HUD_H     58        // bottom bar: RCI demand + active tool

// Floating tool palette (opens over the map; building is paused while open)
#define ICON_PX      34     // tool icon size in px
#define PALETTE_COLS 6      // icons per row in the palette grid

// ---- Frame pacing -----------------------------------------------------
#define FRAME_MS  40        // ~25 FPS, like the reference

// ---- Save region (top of flash) --------------------------------------
// Reserve the top 64 KB of flash for the city save.
#define SAVE_REGION_BYTES (32 * 1024)   // city blob is ~27 KB
