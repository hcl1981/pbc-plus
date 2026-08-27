#ifndef PBC_HW_ACCEL_H
#define PBC_HW_ACCEL_H

#include <stdbool.h>
#include "py/obj.h"

// Lazy-initialised; first call to any read function brings up I2C0
// and the chip. Returns false if the chip didn't ack on its address.
bool accel_init(void);

// Per-axis acceleration in g (range ±2g, 10-bit resolution).
// Convention: +X = right, +Y = down/forward, +Z = out of screen.
float accel_x_g(void);
float accel_y_g(void);
float accel_z_g(void);

// Same but in m/s². Convenience wrappers.
float accel_x_ms2(void);
float accel_y_ms2(void);
float accel_z_ms2(void);

#endif // PBC_HW_ACCEL_H
