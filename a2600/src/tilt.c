/* tilt.c -- see tilt.h. Registers after the STK8BA58 data sheet, as the
 * MicroPython port of this device uses them (micropython/PBC_PLUS/modules/
 * pbc_hw/accel.c): chip ID 00h, data 02h..07h (X, Y, Z; 10 bits, left
 * aligned), range 0Fh, bandwidth 10h, power mode 11h. */
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "tilt.h"

#define I2C     i2c0
#define SDA     20
#define SCL     21
#define ADDR    0x18

static bool ok;
static float last;

static bool put(uint8_t reg, uint8_t v)
{
    uint8_t b[2] = { reg, v };
    return i2c_write_timeout_us(I2C, ADDR, b, 2, false, 2000) == 2;
}

bool tilt_init(void)
{
    i2c_init(I2C, 400000);
    gpio_set_function(SDA, GPIO_FUNC_I2C);
    gpio_set_function(SCL, GPIO_FUNC_I2C);
    gpio_pull_up(SDA);
    gpio_pull_up(SCL);
    uint8_t reg = 0x00, id;
    ok = i2c_write_timeout_us(I2C, ADDR, &reg, 1, true, 2000) == 1 &&
         i2c_read_timeout_us(I2C, ADDR, &id, 1, false, 2000) == 1 &&
         put(0x0f, 0x03) &&             /* +-2 g */
         put(0x10, 0x0c) &&             /* 125 Hz bandwidth */
         put(0x11, 0x00);               /* normal power */
    return ok;
}

float tilt_read(void)
{
    if (!ok)
        return 0.0f;
    uint8_t reg = 0x02, b[6];
    if (i2c_write_timeout_us(I2C, ADDR, &reg, 1, true, 2000) != 1 ||
        i2c_read_timeout_us(I2C, ADDR, b, 6, false, 3000) != 6)
        return last;
    /* the chip sits turned on the board: its Y is the device's X (to the
     * right), its X and Z the device's Y and Z with the sign flipped */
    float x = (float)((int16_t)(b[3] << 8 | b[2]) >> 6);
    float y = -(float)((int16_t)(b[1] << 8 | b[0]) >> 6);
    float z = -(float)((int16_t)(b[5] << 8 | b[4]) >> 6);
    float d = sqrtf(y * y + z * z);
    if (d < 40.0f)                      /* lying on its side: keep the last */
        return last;
    /* gravity pulls the right edge down when turned clockwise: -x */
    last = -x / d;
    return last;
}
