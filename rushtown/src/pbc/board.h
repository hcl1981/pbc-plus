/* board.h -- PicoBoy Color Plus (RP2350A, 16 MB flash, ST7789 240x280).
 * Pins as in docs/HARDWARE.md. Buttons are active low with pull-ups. */
#ifndef PBC_BOARD_H
#define PBC_BOARD_H

#define PBC_SPI          spi0
#define PBC_SPI_HZ       62500000u
#define PBC_PIN_SCK      18
#define PBC_PIN_MOSI     19
#define PBC_PIN_LCD_CS   10      /* a plain GPIO, not SPI CSn */
#define PBC_PIN_LCD_DC    8
#define PBC_PIN_LCD_RST   9
#define PBC_PIN_LCD_BL   26      /* must be driven, by PWM */
#define PBC_LCD_W       240
#define PBC_LCD_H       280
#define PBC_LCD_YOFF     20      /* the visible rows are the controller's 20..299 */

#define PBC_PIN_CENTER    0
#define PBC_PIN_RIGHT     1
#define PBC_PIN_DOWN      2
#define PBC_PIN_LEFT      3
#define PBC_PIN_UP        4
#define PBC_PIN_A        27
#define PBC_PIN_B        28

#define PBC_PIN_AUDIO    15      /* channel B of its PWM slice */
#define PBC_PIN_LED_GREEN  12
#define PBC_PIN_LED_YELLOW 13
#define PBC_PIN_LED_RED    14

#endif
