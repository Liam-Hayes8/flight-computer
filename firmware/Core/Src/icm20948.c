#include "icm20948.h"
#include <string.h>

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

#define ICM_REG_USER_CTRL       0x03   /* bank 0 */
#define ICM_REG_EXT_SLV_DATA    0x3B   /* bank 0 */
#define ICM_REG_I2C_MST_CTRL    0x01   /* bank 3 */
#define ICM_REG_I2C_SLV0_ADDR   0x03   /* bank 3 */
#define ICM_REG_I2C_SLV0_REG    0x04   /* bank 3 */
#define ICM_REG_I2C_SLV0_CTRL   0x05   /* bank 3 */
#define ICM_REG_I2C_SLV0_DO     0x06   /* bank 3 */

#define AK09916_ADDR            0x0C
#define AK09916_REG_WIA2        0x01   /* device ID, expect 0x09 */
#define AK09916_REG_ST1         0x10   /* status 1: bit0 = data ready */
#define AK09916_REG_CNTL2       0x31   /* mode */
#define AK09916_REG_CNTL3       0x32   /* bit0 = soft reset */
#define AK09916_UT_PER_LSB      0.15f

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
static uint8_t icm_read_reg(uint8_t reg)
{
  uint8_t v = 0;
  HAL_I2C_Mem_Read(bus, ICM_ADDR, reg, I2C_MEMADD_SIZE_8BIT, &v, 1, 100);
  return v;
}

/* Write one byte to an AK09916 register through the ICM's I2C master. */
static void mag_write(uint8_t reg, uint8_t val)
{
  icm_bank(3);
  icm_write(ICM_REG_I2C_SLV0_ADDR, AK09916_ADDR);        /* write mode */
  icm_write(ICM_REG_I2C_SLV0_REG,  reg);
  icm_write(ICM_REG_I2C_SLV0_DO,   val);
  icm_write(ICM_REG_I2C_SLV0_CTRL, 0x81);                /* enable, 1 byte */
  icm_bank(0);
  HAL_Delay(10);                                         /* let master run */
}



/* Read one byte from an AK09916 register through the ICM's I2C master. */
static uint8_t mag_read_byte(uint8_t reg)
{
  icm_bank(3);
  icm_write(ICM_REG_I2C_SLV0_ADDR, AK09916_ADDR | 0x80); /* read mode */
  icm_write(ICM_REG_I2C_SLV0_REG,  reg);
  icm_write(ICM_REG_I2C_SLV0_CTRL, 0x81);
  icm_bank(0);
  HAL_Delay(10);
  return icm_read_reg(ICM_REG_EXT_SLV_DATA);
}

bool icm20948_mag_init(void)
{

  /* Reset the I2C master first, in case it's wedged from a prior run. */
  icm_bank(0);
  icm_write(ICM_REG_USER_CTRL, 0x02);          /* I2C_MST_RST */
  HAL_Delay(100);

  /* Turn on the ICM's internal I2C master. */
  icm_bank(0);
  icm_write(ICM_REG_USER_CTRL, 0x20);          /* I2C_MST_EN */
  icm_bank(3);
  icm_write(ICM_REG_I2C_MST_CTRL, 0x07);       /* ~345 kHz master clock */
  icm_bank(0);
  HAL_Delay(50);

  

  /* Ask the magnetometer who it is. The master needs a moment after
   * being enabled, so retry rather than failing on the first attempt. */
   bool found = false;
   for (int i = 0; i < 10 && !found; i++)
   {
     HAL_Delay(20);
     found = (mag_read_byte(AK09916_REG_WIA2) == 0x09);
   }
   if (!found) return false;

  mag_write(AK09916_REG_CNTL3, 0x01);          /* soft reset */
  HAL_Delay(100);
  mag_write(AK09916_REG_CNTL2, 0x08);          /* continuous, 100 Hz */
  HAL_Delay(10);

  /* Now park SLV0 on a continuous 9-byte read starting at ST1:
   * ST1, HXL, HXH, HYL, HYH, HZL, HZH, TMPS, ST2.
   * The ICM refreshes EXT_SLV_SENS_DATA automatically from here on. */
  icm_bank(3);
  icm_write(ICM_REG_I2C_SLV0_ADDR, AK09916_ADDR | 0x80);
  icm_write(ICM_REG_I2C_SLV0_REG,  AK09916_REG_ST1);
  icm_write(ICM_REG_I2C_SLV0_CTRL, 0x89);      /* enable, 9 bytes */
  icm_bank(0);
  return true;
}

static void mag_rearm(void)
{
  icm_bank(0);
  icm_write(ICM_REG_USER_CTRL, 0x02);   /* I2C_MST_RST */
  icm_write(ICM_REG_USER_CTRL, 0x20);   /* re-enable master */
  icm_bank(3);
  icm_write(ICM_REG_I2C_SLV0_ADDR, AK09916_ADDR | 0x80);
  icm_write(ICM_REG_I2C_SLV0_REG,  AK09916_REG_ST1);
  icm_write(ICM_REG_I2C_SLV0_CTRL, 0x89);
  icm_bank(0);
}

bool icm20948_mag_read(icm20948_mag_t *out)
{
  static uint8_t last[6];
  static int same = 0;

  uint8_t d[9];
  if (HAL_I2C_Mem_Read(bus, ICM_ADDR, ICM_REG_EXT_SLV_DATA,
                       I2C_MEMADD_SIZE_8BIT, d, 9, 100) != HAL_OK)
    return false;
  if (d[8] & 0x08) return false;

  if (memcmp(d + 1, last, 6) == 0)
  {
    if (++same >= 20)             /* ~2 s of frozen data = real stall */
    {
      mag_rearm();
      same = 0;
    }
    return false;                 /* skip this stale sample */
  }
  same = 0;
  memcpy(last, d + 1, 6);

  int16_t x = (int16_t)(d[1] | (d[2] << 8));
  int16_t y = (int16_t)(d[3] | (d[4] << 8));
  int16_t z = (int16_t)(d[5] | (d[6] << 8));
  out->mx = x * AK09916_UT_PER_LSB;
  out->my = y * AK09916_UT_PER_LSB;
  out->mz = z * AK09916_UT_PER_LSB;
  return true;
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