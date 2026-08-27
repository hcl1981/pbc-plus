#include "accel.h"
#include "pbc_hw_config.h"

#include "hardware/i2c.h"
#include "hardware/gpio.h"

// STK8BA58 register map (subset).
#define REG_CHIPID      0x00
#define REG_XLSB        0x02   // X LSB; subsequent registers hold X MSB, Y LSB, Y MSB, Z LSB, Z MSB
#define REG_RANGE       0x0F
#define REG_BWIDTH      0x10
#define REG_POWMODE     0x11

#define RANGE_2G        0x03

// Earth gravity, used to convert g→m/s².
#define G_TO_MS2        9.80665f

static bool accel_initialized   = false;
static int  accel_range_g_int   = 2;

static int reg_write_u8(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = { reg, val };
    return i2c_write_blocking(PBC_I2C_PORT, STK8BA58_I2C_ADDR, buf, 2, false);
}

static int reg_read(uint8_t reg, uint8_t *buf, size_t len) {
    int r = i2c_write_blocking(PBC_I2C_PORT, STK8BA58_I2C_ADDR, &reg, 1, true);
    if (r < 0) return r;
    return i2c_read_blocking(PBC_I2C_PORT, STK8BA58_I2C_ADDR, buf, len, false);
}

bool accel_init(void) {
    if (accel_initialized) {
        return true;
    }

    i2c_init(PBC_I2C_PORT, I2C_BAUDRATE);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    // Probe -- chip ID register should answer.
    uint8_t id = 0;
    if (reg_read(REG_CHIPID, &id, 1) < 0) {
        return false;
    }

    // ±2g range, 125 Hz bandwidth, normal power mode.
    reg_write_u8(REG_RANGE,   RANGE_2G);
    reg_write_u8(REG_BWIDTH,  0x0C);
    reg_write_u8(REG_POWMODE, 0x00);

    accel_initialized = true;
    return true;
}

// Read a 16-bit two's-complement axis value, then sign-extend the
// 10-bit reading down from bit 15. STK8BA58 puts data left-justified
// in [15:6] across LSB+MSB registers.
static int16_t read_axis(uint8_t reg_lsb) {
    uint8_t buf[2];
    if (reg_read(reg_lsb, buf, 2) < 0) {
        return 0;
    }
    int16_t raw = ((int16_t)buf[1] << 8) | buf[0];
    return raw >> 6;
}

// Axis convention (device frame, after physical-mounting correction):
//   +X = right, +Y = down/forward (screen-y), +Z = out of screen.
// Chip is mounted such that we swap X/Y reads and negate the new X.
float accel_x_g(void) {
    if (!accel_initialized && !accel_init()) return 0.0f;
    return -(float)read_axis(REG_XLSB + 2) * (float)accel_range_g_int / 512.0f;
}
float accel_y_g(void) {
    if (!accel_initialized && !accel_init()) return 0.0f;
    return  (float)read_axis(REG_XLSB)     * (float)accel_range_g_int / 512.0f;
}
float accel_z_g(void) {
    if (!accel_initialized && !accel_init()) return 0.0f;
    return  (float)read_axis(REG_XLSB + 4) * (float)accel_range_g_int / 512.0f;
}

float accel_x_ms2(void) { return accel_x_g() * G_TO_MS2; }
float accel_y_ms2(void) { return accel_y_g() * G_TO_MS2; }
float accel_z_ms2(void) { return accel_z_g() * G_TO_MS2; }
