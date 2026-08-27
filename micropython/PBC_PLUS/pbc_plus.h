// PicoBoy Color Plus -- pico-sdk board header.
//
// Strategy: inherit everything from the stock Pico 2 board header
// (which correctly sets PICO_RP2350A=1, the right boot2 for the
// W25Q-class flash, crystal config, etc.) and only override the
// flash size, since the PBC+ has 16 MB instead of the Pico 2's 4 MB.

#ifndef _BOARDS_PBC_PLUS_H
#define _BOARDS_PBC_PLUS_H

// Marker for board-detection code in our own modules.
#define PBC_PLUS

// Pull in all defaults from the stock Pico 2 board (RP2350A, 12 MHz
// XOSC, default boot2 for W25Q-class flash, etc.).
#include "boards/pico2.h"

// Override: PBC+ ships with 16 MB external flash.
#undef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (16 * 1024 * 1024)

// We inherit PICO_DEFAULT_LED_PIN=25 from pico2.h, but the PBC+ has
// no LED on GP25. Undef it so nothing accidentally treats GP25 as
// the "default LED" -- our three status LEDs are on GP12/13/14.
#undef PICO_DEFAULT_LED_PIN

#endif // _BOARDS_PBC_PLUS_H
