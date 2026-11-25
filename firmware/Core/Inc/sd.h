#ifndef SD_H
#define SD_H

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

bool sd_init(SPI_HandleTypeDef *hspi);

#endif