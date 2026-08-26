#ifndef BMP581_H
#define BMP581_H

#include "main.h"
#include <stdbool.h>

typedef struct {
    float temperature_c;
    float pressure_pa;
    float altitude_m;
} bmp581_data_t;

bool bmp581_init(I2C_HandleTypeDef *hi2c);
bool bmp581_read(bmp581_data_t *out);

#endif