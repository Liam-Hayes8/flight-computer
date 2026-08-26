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

#endif