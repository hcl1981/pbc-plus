/* board.h -- Pinbelegung PicoBoy Color Plus.
 * Werte laut CLAUDE.md Abschnitt 1 (auf echter Hardware bestaetigt). */
#ifndef PBC_BOARD_H
#define PBC_BOARD_H

#define PBC_SPI          spi0
#define PBC_PIN_SCK      18
#define PBC_PIN_MOSI     19
#define PBC_PIN_LCD_CS   10                /* normaler GPIO, nicht SPI-CSn  */
#define PBC_PIN_LCD_DC    8
#define PBC_PIN_LCD_RST   9
#define PBC_PIN_LCD_BL   26                /* PWM                            */
/* 62,5 MHz sind auf diesem Panel unauffaellig.  80 MHz laufen meist und
 * kuerzen die Bildzeit von ~14,8 auf ~11,5 ms -- erst nach der Messung auf
 * Seite 5 der Testfirmware hochsetzen:
 *     cmake -B build ... -DPBC_SPI_HZ=80000000
 */
#ifndef PBC_SPI_HZ
#define PBC_SPI_HZ       62500000u
#endif

#define PBC_PIN_CENTER    0
#define PBC_PIN_RIGHT     1
#define PBC_PIN_DOWN      2
#define PBC_PIN_LEFT      3
#define PBC_PIN_UP        4
#define PBC_PIN_A        27
#define PBC_PIN_B        28

#define PBC_PIN_AUDIO    15                /* PWM-Kanal B seines Slices      */

#define PBC_PIN_LED_GREEN  12              /* alle aktiv HIGH                */
#define PBC_PIN_LED_YELLOW 13
#define PBC_PIN_LED_RED    14

#define PBC_LCD_W        240
#define PBC_LCD_H        280
#define PBC_LCD_YOFF     20                /* sichtbar sind Zeilen 20..299   */

#endif /* PBC_BOARD_H */
