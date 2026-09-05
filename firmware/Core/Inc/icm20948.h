#ifndef ICM20948_H
#define ICM20948_H

#include "main.h"
#include <stdbool.h>

typedef struct {
    float ax, ay, az;    /* g */
    float gx, gy, gz;    /* deg/s */
} icm20948_data_t;

bool icm20948_init(I2C_HandleTypeDef *hi2c);
bool icm20948_read(icm20948_data_t *out);

typedef struct {
    float mx, my, mz;    /* microtesla, raw sensor axes */
} icm20948_mag_t;

bool icm20948_mag_init(void);
bool icm20948_mag_read(icm20948_mag_t *out);

#endif