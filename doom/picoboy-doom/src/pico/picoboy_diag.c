//
// PicoBoy diagnostic markers via Red/Yellow/Green LEDs.
//
//   GP14 = red LED   (bit 2)
//   GP13 = yellow LED (bit 1)
//   GP12 = green LED  (bit 0)
//
// picoboy_mark(N) writes N (1..7) as a binary pattern on the three LEDs.
// The LATEST mark stays lit until overwritten -- so wherever the engine
// hangs, the LEDs show the highest reached marker number.
//
//   1 = green                    (binary 001)
//   2 = yellow                   (binary 010)
//   3 = yellow + green           (binary 011)
//   4 = red                      (binary 100)
//   5 = red + green              (binary 101)
//   6 = red + yellow             (binary 110)
//   7 = red + yellow + green     (binary 111)
//
// GPLv2.
//

#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"

#define PB_LED_RED     14
#define PB_LED_YELLOW  13
#define PB_LED_GREEN   12

void picoboy_diag_init(void)
{
    gpio_init(PB_LED_RED);    gpio_set_dir(PB_LED_RED,    GPIO_OUT); gpio_put(PB_LED_RED,    0);
    gpio_init(PB_LED_YELLOW); gpio_set_dir(PB_LED_YELLOW, GPIO_OUT); gpio_put(PB_LED_YELLOW, 0);
    gpio_init(PB_LED_GREEN);  gpio_set_dir(PB_LED_GREEN,  GPIO_OUT); gpio_put(PB_LED_GREEN,  0);
}

void picoboy_mark(int n)
{
    gpio_put(PB_LED_GREEN,  (n >> 0) & 1);
    gpio_put(PB_LED_YELLOW, (n >> 1) & 1);
    gpio_put(PB_LED_RED,    (n >> 2) & 1);
}

// picoboy_submark(N): emits N quick beeps on the piezo (GP15) without
// changing any LED. Used to bracket fine-grained sub-sections inside a
// main section (where the latched LED pattern stays at the main marker).
//
// Faster cadence than DIAG_BEEP so it's clearly distinguishable: ~50ms
// beep + 50ms gap, so 4 sub-beeps take ~400ms total.
//
void picoboy_submark(int n)
{
    for (int i = 0; i < n; i++) {
        // ~50ms of 1 kHz square wave
        for (int c = 0; c < 50; c++) {
            gpio_put(15, 1); busy_wait_us(500);
            gpio_put(15, 0); busy_wait_us(500);
        }
        busy_wait_ms(50);
    }
    // small trailer pause
    busy_wait_ms(150);
}
