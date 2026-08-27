/*
 * picoboy_pins.h
 *
 * Hardware-Pin-Mapping fuer den PicoBoy Color (RP2350).
 * Alle Tasten sind aktiv-LOW mit internem Pull-up.
 */

#ifndef PICOBOY_PINS_H
#define PICOBOY_PINS_H

// ---- Display (ST7789, 240x280, SPI0) ----
#define PB_PIN_DISP_SCK   18    // SPI0 SCK
#define PB_PIN_DISP_MOSI  19    // SPI0 TX
#define PB_PIN_DISP_CS    10
#define PB_PIN_DISP_DC     8
#define PB_PIN_DISP_RST    9
#define PB_PIN_DISP_BL    26    // PWM-faehig

#define PB_DISP_W         240
#define PB_DISP_H         280
#define PB_DISP_Y_OFFSET   20   // Panel sitzt mit 20px Offset im 240x320-RAM
#define PB_DISP_X_OFFSET    0
#define PB_DISP_MADCTL    0xC8  // 180 Grad, BGR

// ---- Eingaben (5-Wege-Joystick + 2 Action-Buttons) ----
#define PB_PIN_JOY_UP      4
#define PB_PIN_JOY_DOWN    2
#define PB_PIN_JOY_LEFT    3
#define PB_PIN_JOY_RIGHT   1
#define PB_PIN_JOY_CENTER  0
#define PB_PIN_BTN_A      27
#define PB_PIN_BTN_B      28

// ---- Audio (Piezo via PWM mit RC-Tiefpass) ----
#define PB_PIN_SPEAKER    15

// ---- Status-LEDs (aktiv-HIGH) ----
#define PB_PIN_LED_RED    14
#define PB_PIN_LED_YELLOW 13
#define PB_PIN_LED_GREEN  12

#endif // PICOBOY_PINS_H
