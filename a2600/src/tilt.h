/* tilt.h -- the PicoBoy Color Plus's acceleration sensor (STK8BA58 on I2C0,
 * SDA GP20, SCL GP21) as a steering wheel: how far the device is turned
 * left or right in the plane of the screen. */
#ifndef PBC_TILT_H
#define PBC_TILT_H

#include <stdbool.h>

/* Brings up I2C and the sensor; false if it does not answer. */
bool tilt_init(void);

/* The turn as the tangent of its angle against the vertical, positive
 * to the right (clockwise); about 0 held upright, flat or tilted back. */
float tilt_read(void);

#endif
