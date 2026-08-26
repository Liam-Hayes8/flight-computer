#ifndef GPS_H
#define GPS_H

#include "main.h"
#include <stdbool.h>

void gps_init(UART_HandleTypeDef *huart);
bool gps_get_sentence(char *dest, int len);

#endif