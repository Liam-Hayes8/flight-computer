#include "icm20948.h"

#define ICM_ADDR            (0x69 << 1)
#define ICM_REG_WHOAMI      0x00
#define ICM_WHOAMI_VAL      0xEA
#define ICM_REG_BANK_SEL    0x7F
#define ICM_REG_PWR_MGMT_1  0x06
#define ICM_REG_PWR_MGMT_2  0x07
#define ICM_REG_GYRO_CFG1   0x01   /* bank 2 */
#define ICM_REG_ACCEL_CFG   0x14   /* bank 2 */
#define ICM_REG_ACCEL_XOUT  0x2D   /* bank 0 */

#define ACCEL_LSB_PER_G     16384.0f   /* +/-2 g     */
#define GYRO_LSB_PER_DPS    131.0f     /* +/-250 dps */

static I2C_HandleTypeDef *bus;

static void icm_bank(uint8_t bank)
{
  uint8_t v = bank << 4;
  HAL_I2C_Mem_Write(bus, ICM_ADDR, ICM_REG_BANK_SEL,
                    I2C_MEMADD_SIZE_8BIT, &v, 1, 100);
}

static void icm_write(uint8_t reg, uint8_t val)
{
  HAL_I2C_Mem_Write(bus, ICM_ADDR, reg,
                    I2C_MEMADD_SIZE_8BIT, &val, 1, 100);
}

bool icm20948_init(I2C_HandleTypeDef *hi2c)
{
  bus = hi2c;

  uint8_t who = 0;
  if (HAL_I2C_Mem_Read(bus, ICM_ADDR, ICM_REG_WHOAMI,
                       I2C_MEMADD_SIZE_8BIT, &who, 1, 100) != HAL_OK)
    return false;
  if (who != ICM_WHOAMI_VAL)
    return false;

  icm_bank(0);
  icm_write(ICM_REG_PWR_MGMT_1, 0x80);   /* device reset */
  HAL_Delay(100);
  icm_bank(0);
  icm_write(ICM_REG_PWR_MGMT_1, 0x01);   /* wake, auto clock */
  icm_write(ICM_REG_PWR_MGMT_2, 0x00);   /* all six axes on */
  HAL_Delay(50);

  icm_bank(2);
  icm_write(ICM_REG_GYRO_CFG1, 0x01);    /* +/-250 dps, DLPF on */
  icm_write(ICM_REG_ACCEL_CFG, 0x01);    /* +/-2 g,     DLPF on */
  icm_bank(0);
  HAL_Delay(50);
  return true;
}

bool icm20948_read(icm20948_data_t *out)
{
  uint8_t d[12];
  if (HAL_I2C_Mem_Read(bus, ICM_ADDR, ICM_REG_ACCEL_XOUT,
                       I2C_MEMADD_SIZE_8BIT, d, 12, 100) != HAL_OK)
    return false;

  out->ax = (int16_t)((d[0]  << 8) | d[1])  / ACCEL_LSB_PER_G;
  out->ay = (int16_t)((d[2]  << 8) | d[3])  / ACCEL_LSB_PER_G;
  out->az = (int16_t)((d[4]  << 8) | d[5])  / ACCEL_LSB_PER_G;
  out->gx = (int16_t)((d[6]  << 8) | d[7])  / GYRO_LSB_PER_DPS;
  out->gy = (int16_t)((d[8]  << 8) | d[9])  / GYRO_LSB_PER_DPS;
  out->gz = (int16_t)((d[10] << 8) | d[11]) / GYRO_LSB_PER_DPS;
  return true;
}