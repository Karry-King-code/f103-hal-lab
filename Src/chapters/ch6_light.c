/**
  ******************************************************************************
  * @file    Templates/Src/main.c
  * @brief   07 ch: light sensor DO read + UART print (HAL version)
  * @note    DO = PB12 (floating input), prints on change, '?' = query,
  *          HAL_UART_Receive 10ms timeout polling keeps loop non-blocking.
  ******************************************************************************
  */
#include "main.h"

static UART_HandleTypeDef huart1;
static uint16_t uart_len(const char *s)
{
  uint16_t n = 0;
  while (s[n]) n++;
  return n;
}
static void uart_str(const char *s)
{
  HAL_UART_Transmit(&huart1, (uint8_t *)s, uart_len(s), HAL_MAX_DELAY);
}

void SystemClock_Config(void);

int main(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  uint8_t last = 0xFF;
  uint16_t n = 0;

  HAL_Init();
  SystemClock_Config();

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_USART1_CLK_ENABLE();

  /* PB13 = DO: floating input (module comparator is push-pull) */
  GPIO_InitStruct.Pin  = GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* PC13 heartbeat */
  GPIO_InitStruct.Pin   = GPIO_PIN_13;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);

  /* USART1 PA9/PA10 */
  GPIO_InitStruct.Pin      = GPIO_PIN_9;
  GPIO_InitStruct.Mode     = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Speed    = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  GPIO_InitStruct.Pin  = GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  huart1.Instance          = USART1;
  huart1.Init.BaudRate     = 115200;
  huart1.Init.WordLength   = UART_WORDLENGTH_8B;
  huart1.Init.StopBits     = UART_STOPBITS_1;
  huart1.Init.Parity       = UART_PARITY_NONE;
  huart1.Init.Mode         = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
    while (1);

  uart_str("\r\n=== CH07 LIGHT SENSOR (HAL) READY ===\r\n");
  uart_str("Prints on level change. '?'=query. PB13=DO\r\n");

  while (1)
  {
    uint8_t now = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_12) == GPIO_PIN_RESET) ? 0 : 1;
    uint8_t b;

    if (now != last)
    {
      last = now;
      uart_str("SMOKE(CH07 LIGHT) DO=");
      uart_str(now ? "1  (normal/bright)\r\n" : "0  (ALARM/dark!)\r\n");
    }

    if (HAL_UART_Receive(&huart1, &b, 1, 10) == HAL_OK && b == '?')
    {
      uart_str("[QUERY] DO=");
      uart_str((HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_13) != GPIO_PIN_RESET) ? "1\r\n" : "0\r\n");
    }

    HAL_Delay(10);
    if (++n >= 50)
    {
      n = 0;
      HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    }
  }
}

void SystemClock_Config(void)
{
  RCC_ClkInitTypeDef clkinitstruct = {0};
  RCC_OscInitTypeDef oscinitstruct = {0};

  oscinitstruct.OscillatorType  = RCC_OSCILLATORTYPE_HSI;
  oscinitstruct.HSEState        = RCC_HSE_OFF;
  oscinitstruct.LSEState        = RCC_LSE_OFF;
  oscinitstruct.HSIState        = RCC_HSI_ON;
  oscinitstruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  oscinitstruct.HSEPredivValue    = RCC_HSE_PREDIV_DIV1;
  oscinitstruct.PLL.PLLState    = RCC_PLL_ON;
  oscinitstruct.PLL.PLLSource   = RCC_PLLSOURCE_HSI_DIV2;
  oscinitstruct.PLL.PLLMUL      = RCC_PLL_MUL16;
  if (HAL_RCC_OscConfig(&oscinitstruct)!= HAL_OK)
    while(1);

  clkinitstruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  clkinitstruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  clkinitstruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  clkinitstruct.APB2CLKDivider = RCC_HCLK_DIV1;
  clkinitstruct.APB1CLKDivider = RCC_HCLK_DIV2;
  if (HAL_RCC_ClockConfig(&clkinitstruct, FLASH_LATENCY_2)!= HAL_OK)
    while(1);
}
