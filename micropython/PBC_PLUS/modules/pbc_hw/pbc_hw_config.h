// PicoBoy Color Plus — hardware pin map for pbc_hw module.
// Single source of truth; all subsystem .c files include this.

#ifndef PBC_HW_CONFIG_H
#define PBC_HW_CONFIG_H

#include "hardware/i2c.h"

// Speaker (PWM out, low-pass filtered to a small speaker)
#define SPEAKER_PIN         15

// Battery sense (ADC3 = GP29, behind a 1:2 divider)
#define BATTERY_PIN         29
#define BATTERY_ADC_INPUT   3
#define BATTERY_DIVIDER     2.0f
#define BATTERY_VREF        3.3f
#define BATTERY_ADC_MAX     4095.0f

// Joystick + action buttons (all active-low with internal pull-up)
#define BTN_UP_PIN          4
#define BTN_DOWN_PIN        2
#define BTN_LEFT_PIN        3
#define BTN_RIGHT_PIN       1
#define BTN_CENTER_PIN      0
#define BTN_A_PIN           27
#define BTN_B_PIN           28

// Single SK6805-EC14 RGB LED (WS2812-compatible)
#define RGB_LED_PIN         11

// STK8BA58 G-Sensor on I2C0.
// NOTE: name is PBC_I2C_PORT, not I2C_INSTANCE — pico-sdk defines a
// macro named I2C_INSTANCE() in hardware/i2c.h which would collide.
#define PBC_I2C_PORT        i2c0
#define I2C_SDA_PIN         20
#define I2C_SCL_PIN         21
#define I2C_BAUDRATE        400000
#define ACCEL_INT_PIN       22
#define STK8BA58_I2C_ADDR   0x18

#endif // PBC_HW_CONFIG_H
