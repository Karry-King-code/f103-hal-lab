/**
  ******************************************************************************
  * @file    Templates/Src/main.c
  * @author  MCD Application Team
  * @brief   Main program body（04 章：串口收发——HAL 版）
  * @note    USART1 = PA9(TX)/PA10(RX)，115200-8-N-1，收什么回什么（echo）。
  *          时钟仍是官方模板的 HSI->64MHz（APB2=64MHz），HAL 自己算 BRR。
  *          每收到 1 字节：回显 + 翻转绿灯（PC13）。
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2016 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private variables ---------------------------------------------------------*/
static UART_HandleTypeDef huart1;    /* UART 句柄：HAL 靠它认人（实例+参数+状态全在里面） */
volatile uint32_t g_rx_count = 0;    /* 已收字节数（调试器可读） */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void USART1_UART_Init(void);

/**
  * @brief  Main program
  */
int main(void)
{
  uint8_t b;

  HAL_Init();

  /* Configure the system clock to 64 MHz */
  SystemClock_Config();

  /* ==== 时钟：GPIOA、GPIOC、USART1（在 APB2 上，64MHz） ==== */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_USART1_CLK_ENABLE();

  /* ==== PA9=TX：复用推挽（引脚控制权交给 USART） ==== */
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin      = GPIO_PIN_9;
  GPIO_InitStruct.Mode     = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Speed    = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* ==== PA10=RX：浮空输入 ==== */
  GPIO_InitStruct.Pin  = GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* ==== PC13 绿灯：回显反馈 ==== */
  GPIO_InitStruct.Pin   = GPIO_PIN_13;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);   /* 初始：灭 */

  /* ==== USART1：115200-8-N-1 ==== */
  USART1_UART_Init();

  /* ==== 上电横幅（HAL_UART_Transmit：阻塞发一整块） ==== */
  uint8_t banner[] = "\r\n=== STM32F103 USART1 READY - HAL (115200-8-N-1) ===\r\n"
                     "Type anything, I will echo it back. LED toggles per byte.\r\n";
  HAL_UART_Transmit(&huart1, banner, sizeof(banner) - 1, HAL_MAX_DELAY);

  /* Infinite loop */
  while (1)
  {
    /* 收 1 字节（阻塞轮询模式）；读完成 HAL 内部已清 RXNE */
    if (HAL_UART_Receive(&huart1, &b, 1, HAL_MAX_DELAY) == HAL_OK)
    {
      HAL_UART_Transmit(&huart1, &b, 1, HAL_MAX_DELAY);  /* 原样发回 */
      HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
      g_rx_count++;
    }
  }
}

/**
  * @brief  USART1 初始化：填句柄 -> HAL_UART_Init（内部配 BRR/CR1 并使能）
  */
static void USART1_UART_Init(void)
{
  huart1.Instance                    = USART1;
  huart1.Init.BaudRate               = 115200;
  huart1.Init.WordLength             = UART_WORDLENGTH_8B;
  huart1.Init.StopBits               = UART_STOPBITS_1;
  huart1.Init.Parity                 = UART_PARITY_NONE;
  huart1.Init.Mode                   = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl              = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling           = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    while (1);                         /* 初始化失败：停在这里便于排查 */
  }
}

/**
  * @brief  System Clock Configuration（官方模板原样：HSI -> 64MHz）
  */
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
  {
    while(1);
  }

  clkinitstruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  clkinitstruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  clkinitstruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  clkinitstruct.APB2CLKDivider = RCC_HCLK_DIV1;
  clkinitstruct.APB1CLKDivider = RCC_HCLK_DIV2;
  if (HAL_RCC_ClockConfig(&clkinitstruct, FLASH_LATENCY_2)!= HAL_OK)
  {
    while(1);
  }
}
