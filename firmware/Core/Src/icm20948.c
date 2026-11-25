#include "icm20948.h"
#include <string.h>
#include <stdio.h>

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
#define ICM_REG_I2C_SLV0_CTRL   0x05   /* bank 3 */


#define AK09916_ADDR            0x0C
#define AK09916_REG_WIA2        0x01   /* device ID, expect 0x09 */
#define AK09916_REG_ST1         0x10   /* status 1: bit0 = data ready */
#define AK09916_REG_CNTL2       0x31   /* mode */
#define AK09916_REG_CNTL3       0x32   /* bit0 = soft reset */
#define AK09916_UT_PER_LSB      0.15f

static I2C_HandleTypeDef *bus;
static bool    mag_ready   = false;

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

/* ------------------------------------------------------------------ */
/* Magnetometer via bypass mode                                        */
/*                                                                     */
/* The AK09916 sits on the ICM's internal auxiliary I2C bus. Instead   */
/* of driving it through the ICM's I2C master (which has proven        */
/* unreliable at startup), BYPASS_EN wires that bus straight through   */
/* to the main SDA/SCL pins, and the STM32 talks to the AK09916        */
/* directly at address 0x0C. Simpler, and every failure is visible.    */
/* ------------------------------------------------------------------ */

#define ICM_REG_INT_PIN_CFG     0x0F   /* bank 0 */
#define INT_PIN_CFG_BYPASS_EN   0x02
#define AK_ADDR                 (AK09916_ADDR << 1)

/* Disable the ICM master and open the bypass switches. */
static void mag_bypass_enable(void)
{
  icm_bank(3);
  icm_write(ICM_REG_I2C_SLV0_CTRL, 0x00);      /* no slave slots active */
  icm_bank(0);
  icm_write(ICM_REG_USER_CTRL, 0x00);          /* master off */
  icm_write(ICM_REG_INT_PIN_CFG, INT_PIN_CFG_BYPASS_EN);
  HAL_Delay(10);
}

static bool ak_write(uint8_t reg, uint8_t val)
{
  return HAL_I2C_Mem_Write(bus, AK_ADDR, reg, I2C_MEMADD_SIZE_8BIT, &val, 1, 100) == HAL_OK;
}
static bool ak_read(uint8_t reg, uint8_t *buf, uint16_t n)
{
  return HAL_I2C_Mem_Read(bus, AK_ADDR, reg, I2C_MEMADD_SIZE_8BIT, buf, n, 100) == HAL_OK;
}

bool icm20948_mag_init(void)
{
  mag_ready = false;
  mag_bypass_enable();

  if (HAL_I2C_IsDeviceReady(bus, AK_ADDR, 3, 50) != HAL_OK)
  {
    printf("AK09916 diag: nothing at 0x0C in bypass mode\r\n");
    return false;
  }

  uint8_t wia = 0;
  if (!ak_read(AK09916_REG_WIA2, &wia, 1) || wia != 0x09)
  {
    printf("AK09916 diag: 0x0C answers but WIA=0x%02X (want 0x09)\r\n", wia);
    return false;
  }

  ak_write(AK09916_REG_CNTL3, 0x01);           /* soft reset */
  HAL_Delay(100);
  ak_write(AK09916_REG_CNTL2, 0x08);           /* continuous, 100 Hz */
  HAL_Delay(10);

  uint8_t mode = 0;
  if (!ak_read(AK09916_REG_CNTL2, &mode, 1) || mode != 0x08)
  {
    printf("AK09916 diag: CNTL2 readback 0x%02X (want 0x08)\r\n", mode);
    return false;
  }

  mag_ready = true;
  return true;
}

bool icm20948_mag_read(icm20948_mag_t *out)
{
  static int fails = 0;

  if (!mag_ready) return false;

  /* ST1, HXL, HXH, HYL, HYH, HZL, HZH, TMPS, ST2. Reading through ST2
   * is required: it releases the data registers for the next sample. */
  uint8_t d[9];
  if (!ak_read(AK09916_REG_ST1, d, 9))
  {
    if (++fails >= 20)                       /* ~2 s of failed reads */
    {
      mag_bypass_enable();                   /* re-open the switches */
      fails = 0;
    }
    return false;
  }
  fails = 0;

  if (!(d[0] & 0x01)) return false;          /* ST1 DRDY clear: no new sample */
  if (d[8] & 0x08)    return false;          /* ST2 HOFL: overflow */

  int16_t x = (int16_t)(d[1] | (d[2] << 8));
  int16_t y = (int16_t)(d[3] | (d[4] << 8));
  int16_t z = (int16_t)(d[5] | (d[6] << 8));
  out->mx = x * AK09916_UT_PER_LSB;
  out->my = y * AK09916_UT_PER_LSB;
  out->mz = z * AK09916_UT_PER_LSB;
  return true;
}

/* ------------------------------------------------------------------ */
/* IMU                                                                 */
/* ------------------------------------------------------------------ */

bool icm20948_init(I2C_HandleTypeDef *hi2c)
{
  bus = hi2c;

  uint8_t who = 0;
  if (HAL_I2C_Mem_Read(bus, ICM_ADDR, ICM_REG_WHOAMI,
                       I2C_MEMADD_SIZE_8BIT, &who, 1, 100) != HAL_OK)
    return false;
  if (who != ICM_WHOAMI_VAL)
    return false;

  /* On a warm reset the ICM is still configured from the previous boot.
   * Switch its I2C master off before the device reset. */
  icm_bank(0);
  icm_write(ICM_REG_USER_CTRL, 0x00);
  HAL_Delay(5);

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
