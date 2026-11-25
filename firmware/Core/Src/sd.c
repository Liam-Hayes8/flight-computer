#include "sd.h"
#include <stdio.h>

static SPI_HandleTypeDef *spi;

static void cs_low(void)  { HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_RESET); }
static void cs_high(void) { HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET); }

/* Clock one byte out and return the byte that came back. */
static uint8_t xfer(uint8_t tx)
{
  uint8_t rx = 0xFF;
  HAL_SPI_TransmitReceive(spi, &tx, &rx, 1, 100);
  return rx;
}

/* Send a command frame and return the R1 response byte.
 * R1 always has bit 7 clear; 0xFF means the card never answered. */
static uint8_t sd_cmd(uint8_t cmd, uint32_t arg, uint8_t crc)
{
  xfer(0x40 | cmd);
  xfer((uint8_t)(arg >> 24));
  xfer((uint8_t)(arg >> 16));
  xfer((uint8_t)(arg >> 8));
  xfer((uint8_t)(arg));
  xfer(crc);

  for (int i = 0; i < 8; i++)
  {
    uint8_t r = xfer(0xFF);
    if (!(r & 0x80)) return r;
  }
  return 0xFF;
}

bool sd_init(SPI_HandleTypeDef *hspi)
{
  spi = hspi;
  cs_low();  HAL_Delay(500);   /* ACT LED should light for half a second */
  cs_high(); HAL_Delay(50);

  cs_high();
  for (int i = 0; i < 10; i++) xfer(0xFF);

  cs_low();
  xfer(0x40); xfer(0); xfer(0); xfer(0); xfer(0); xfer(0x95);
  printf("SD CMD0 raw:");
  uint8_t r1 = 0xFF;
  for (int i = 0; i < 8; i++)
  {
    uint8_t r = xfer(0xFF);
    printf(" %02X", r);
    if (r1 == 0xFF && !(r & 0x80)) r1 = r;
  }
  printf("\r\n");
  cs_high();
  xfer(0xFF);

  printf("SD CMD0: 0x%02X\r\n", r1);
  return (r1 == 0x01);
}