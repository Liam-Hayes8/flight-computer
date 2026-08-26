#include "bmp581.h"
#include <math.h>

#define BMP581_ADDR           (0x47 << 1)
#define BMP581_REG_CHIPID     0x01
#define BMP581_CHIPID_VAL     0x50
#define BMP581_REG_OSR_CONFIG 0x36
#define BMP581_REG_ODR_CONFIG 0x37
#define BMP581_REG_TEMP_DATA  0x1D
#define SEA_LEVEL_PA          101325.0f

static I2C_HandleTypeDef *bus;

bool bmp581_init(I2C_HandleTypeDef *hi2c)
{
  bus = hi2c;

  uint8_t chip_id = 0;
  if (HAL_I2C_Mem_Read(bus, BMP581_ADDR, BMP581_REG_CHIPID,
                       I2C_MEMADD_SIZE_8BIT, &chip_id, 1, 100) != HAL_OK)
    return false;
  if (chip_id != BMP581_CHIPID_VAL)
    return false;

  uint8_t cfg = 0x60;   /* press_en = 1, osr_p = 16x, osr_t = 1x */
  HAL_I2C_Mem_Write(bus, BMP581_ADDR, BMP581_REG_OSR_CONFIG,
                    I2C_MEMADD_SIZE_8BIT, &cfg, 1, 100);
  cfg = 0x5D;           /* normal mode, 10 Hz */
  HAL_I2C_Mem_Write(bus, BMP581_ADDR, BMP581_REG_ODR_CONFIG,
                    I2C_MEMADD_SIZE_8BIT, &cfg, 1, 100);
  HAL_Delay(100);
  return true;
}

bool bmp581_read(bmp581_data_t *out)
{
  uint8_t buf[6];
  if (HAL_I2C_Mem_Read(bus, BMP581_ADDR, BMP581_REG_TEMP_DATA,
                       I2C_MEMADD_SIZE_8BIT, buf, 6, 100) != HAL_OK)
    return false;

  int32_t raw_t = (int32_t)((uint32_t)buf[0]
                          | ((uint32_t)buf[1] << 8)
                          | ((uint32_t)buf[2] << 16));
  if (raw_t & 0x800000) raw_t -= 0x1000000;   /* sign-extend 24-bit */

  uint32_t raw_p = (uint32_t)buf[3]
                 | ((uint32_t)buf[4] << 8)
                 | ((uint32_t)buf[5] << 16);

  out->temperature_c = raw_t / 65536.0f;
  out->pressure_pa   = raw_p / 64.0f;
  out->altitude_m    = 44330.0f * (1.0f - powf(out->pressure_pa / SEA_LEVEL_PA,
                                               1.0f / 5.255f));
  return true;
}