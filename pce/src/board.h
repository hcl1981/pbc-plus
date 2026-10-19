/* board.h -- PicoBoy Color Plus (RP2350A, 16 MB flash, ST7789 240x280).
 *
 * Pin assignment as confirmed in the pbc-plus ports (docs/HARDWARE.md there).
 * All buttons are active low with the internal pull-up.
 */
#ifndef PBC_BOARD_H
#define PBC_BOARD_H

/* ---- Display: ST7789 on SPI0 ---- */
#define PBC_SPI          spi0
#ifndef PBC_SPI_HZ
#define PBC_SPI_HZ       75000000u
#endif
#define PBC_PIN_SCK      18
#define PBC_PIN_MOSI     19
#define PBC_PIN_LCD_CS   10      /* plain GPIO, not SPI CSn */
#define PBC_PIN_LCD_DC    8
#define PBC_PIN_LCD_RST   9
#define PBC_PIN_LCD_BL   26      /* backlight, must be driven (PWM) */

#define PBC_LCD_W       240
#define PBC_LCD_H       280
#define PBC_LCD_YOFF     20      /* visible rows are controller rows 20..299 */

/* ---- Input ---- */
#define PBC_PIN_CENTER    0
#define PBC_PIN_RIGHT     1
#define PBC_PIN_DOWN      2
#define PBC_PIN_LEFT      3
#define PBC_PIN_UP        4
#define PBC_PIN_A        27
#define PBC_PIN_B        28

/* ---- Status LEDs (active high) ---- */
#define PBC_PIN_LED_RED    14
#define PBC_PIN_LED_YELLOW 13
#define PBC_PIN_LED_GREEN  12

#endif /* PBC_BOARD_H */
