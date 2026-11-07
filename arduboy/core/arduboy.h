/* arduboy.h -- the Arduboy for the PicoBoy Color Plus
 *
 * Written for the PicoBoy Color Plus (GPL-3.0-or-later). The Arduboy is an
 * ATmega32U4 at 16 MHz with a 128x64 SSD1306 OLED on its SPI, six buttons
 * and a piezo speaker between PC6 and PC7. Emulated here: the AVR CPU (the
 * whole AVRe+ instruction set with the ATmega32U4's cycle counts, the
 * interrupts and SLEEP), timers 0, 1, 3 and 4 (the modes games use, their
 * flags, interrupts and the output-compare pins that drive the speaker),
 * the SPI with the SSD1306 behind it, the EEPROM, the ADC (a random seed),
 * the PLL and USB enough for the Arduino core to start, the ports with the
 * buttons and the RGB LED.
 *
 * A game is the Intel HEX file the Arduino IDE makes (.hex). Arduboy FX
 * games, which need the external flash chip, are not supported.
 */
#ifndef ARDUBOY_H
#define ARDUBOY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define ARD_W 128
#define ARD_H 64
#define ARD_HZ 16000000u                /* the CPU clock */
#define ARD_EEPROM 1024

/* buttons for ard_buttons() (pressed = 1) */
#define ARD_UP    0x01
#define ARD_DOWN  0x02
#define ARD_LEFT  0x04
#define ARD_RIGHT 0x08
#define ARD_A     0x10
#define ARD_B     0x20

/* The program from an Intel HEX text; false if it is none. */
bool ard_load_hex(const char *text, size_t len);
void ard_reset(void);

void ard_buttons(uint8_t pressed);

/* Run `cycles` CPU cycles; the speaker in `samples` signed samples spread
 * evenly over them (audio may be NULL). */
void ard_run(uint32_t cycles, int16_t *audio, int samples);

/* The display: 128x64, one bit a pixel, in the SSD1306's memory layout as
 * shown (8 pages of 128 bytes, bit 0 the top row of a page), already
 * flipped as the game set it; NULL while the display is off. ard_invert:
 * the game set inverse video. */
const uint8_t *ard_screen(void);
bool ard_invert(void);
uint8_t ard_contrast(void);

/* the RGB LED: bits 0..2 red, green, blue on */
uint8_t ard_led(void);

/* the EEPROM (1 KB); dirty since the last call */
uint8_t *ard_eeprom(void);
bool ard_eeprom_dirty(void);

/* for the status line: CPU cycles spent asleep since the last call */
uint32_t ard_idle_cycles(void);

/* ARD_JIT: after ard_load_hex, translate the program to Thumb-2 as it runs,
 * into buf (executable RAM); ard_jit_used: bytes of it taken so far */
bool ard_jit_init(void *buf, size_t size);
size_t ard_jit_used(void);

#endif
