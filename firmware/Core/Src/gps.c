#include "gps.h"
#include <string.h>

static UART_HandleTypeDef *gps_uart;
static uint8_t  rx_byte;
static char     line[128];
static volatile int idx = 0;
static char     sentence[128];
static volatile uint8_t ready = 0;

void gps_init(UART_HandleTypeDef *huart)
{
  gps_uart = huart;
  idx   = 0;
  ready = 0;
  HAL_UART_Receive_IT(gps_uart, &rx_byte, 1);
}

bool gps_get_sentence(char *dest, int len)
{
  if (!ready) return false;

  __disable_irq();
  strncpy(dest, sentence, len - 1);
  dest[len - 1] = '\0';
  ready = 0;
  __enable_irq();

  return true;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance != USART1) return;

  char c = (char)rx_byte;
  if (c == '\n')
  {
    line[idx] = '\0';
    if (idx > 0 && !ready)
    {
      strcpy(sentence, line);
      ready = 1;
    }
    idx = 0;
  }
  else if (c != '\r')
  {
    if (idx < (int)sizeof(line) - 1) line[idx++] = c;
    else idx = 0;                    /* overflow — resync */
  }
  HAL_UART_Receive_IT(gps_uart, &rx_byte, 1);
}
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance != USART1) return;

  __HAL_UART_CLEAR_OREFLAG(gps_uart);
  __HAL_UART_CLEAR_NEFLAG(gps_uart);
  __HAL_UART_CLEAR_FEFLAG(gps_uart);
  idx = 0;                                  /* drop the partial line */
  HAL_UART_Receive_IT(gps_uart, &rx_byte, 1);  /* re-arm */
}