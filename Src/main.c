/**
  ******************************************************************************
  * @file    Templates/Src/main.c
  * @author  MCD Application Team
  * @brief   Main program body（05 章：按键控制蜂鸣器+继电器——HAL 版）
  * @note    KEY1=PB7 蜂鸣器(PC15 高响)；KEY2=PB6 继电器(PC14 低吸合实测)。
  *          串口命令 B/R/? 双通道验证；逻辑与标准库版完全同构。
  *          时钟：官方模板 HSI->64MHz 原样。
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
static UART_HandleTypeDef huart1;
static uint8_t buzz_state = 0, relay_state = 0;
volatile uint32_t g_rx_count = 0;

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void USART1_UART_Init(void);
static void print_state(void);
static uint8_t key_pressed(uint16_t pin);
static void uart_str(const char *s);

#define BUZZ_ON()   HAL_GPIO_WritePin(GPIOC, GPIO_PIN_15, GPIO_PIN_SET)
#define BUZZ_OFF()  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_15, GPIO_PIN_RESET)
#define RELAY_ON()  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_14, GPIO_PIN_RESET)  /* low-active (measured) */
#define RELAY_OFF() HAL_GPIO_WritePin(GPIOC, GPIO_PIN_14, GPIO_PIN_SET)

/**
  * @brief  Main program
  */
int main(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  HAL_Init();
  SystemClock_Config();

  /* ==== 时钟：GPIOA/B/C + USART1（APB2） ==== */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_USART1_CLK_ENABLE();

  /* ==== 按键 PB7/PB6：上拉输入（Pull 显式字段） ==== */
  GPIO_InitStruct.Pin  = GPIO_PIN_7 | GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* ==== PC13/14/15：推挽输出（一次配三脚） ==== */
  GPIO_InitStruct.Pin   = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);   /* LED off */
  RELAY_OFF();
  BUZZ_OFF();

  /* ==== USART1 115200-8-N-1 ==== */
  GPIO_InitStruct.Pin      = GPIO_PIN_9;
  GPIO_InitStruct.Mode     = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Speed    = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  GPIO_InitStruct.Pin  = GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  USART1_UART_Init();

  uart_str("\r\n=== CH05 BUZZER+RELAY (HAL) READY ===\r\n");
  print_state();

  /* Infinite loop */
  while (1)
  {
    if (key_pressed(GPIO_PIN_7))
    {
      buzz_state ^= 1;
      if (buzz_state) BUZZ_ON(); else BUZZ_OFF();
      uart_str("[KEY1/CMD B] ");
      print_state();
    }
    if (key_pressed(GPIO_PIN_6))
    {
      relay_state ^= 1;
      if (relay_state) RELAY_ON(); else RELAY_OFF();
      uart_str("[KEY2/CMD R] ");
      print_state();
    }

    uint8_t b;
    if (HAL_UART_Receive(&huart1, &b, 1, 10) == HAL_OK)   /* 10ms 超时轮询 */
    {
      g_rx_count++;
      switch (b)
      {
      case 'B':
      case 'b':
        buzz_state ^= 1;
        if (buzz_state) BUZZ_ON(); else BUZZ_OFF();
        uart_str("[KEY1/CMD B] ");
        print_state();
        break;
      case 'R':
      case 'r':
        relay_state ^= 1;
        if (relay_state) RELAY_ON(); else RELAY_OFF();
        uart_str("[KEY2/CMD R] ");
        print_state();
        break;
      case '?':
        print_state();
        break;
      default:
        break;
      }
    }
  }
}

static uint16_t uart_len(const char *s)
{
  uint16_t n = 0;
  while (s[n]) n++;
  return n;
}

static void uart_str(const char *s)
{
  HAL_UART_Transmit(&huart1, (uint8_t *)s, (uint16_t)uart_len(s), HAL_MAX_DELAY);
}

static void print_state(void)
{
  uart_str("[STATE] BUZZER=");
  uart_str(buzz_state ? "ON" : "OFF");
  uart_str("  RELAY=");
  uart_str(relay_state ? "ON(closed)" : "OFF(open)");
  uart_str("\r\n");
}

/**
  * @brief  按键：沿检测+双消抖+等释放（03 章同款，加 pin 参数）
  */
static uint8_t key_pressed(uint16_t pin)
{
  static uint8_t last7 = 1, last6 = 1;
  uint8_t *last = (pin == GPIO_PIN_7) ? &last7 : &last6;
  uint8_t now = (HAL_GPIO_ReadPin(GPIOB, pin) == GPIO_PIN_RESET) ? 0 : 1;
  uint8_t evt = 0;

  if (*last == 1 && now == 0)
  {
    HAL_Delay(10);
    if (HAL_GPIO_ReadPin(GPIOB, pin) == GPIO_PIN_RESET)
    {
      evt = 1;
      while (HAL_GPIO_ReadPin(GPIOB, pin) == GPIO_PIN_RESET)
      {
      }
      HAL_Delay(10);
    }
  }
  *last = now;
  return evt;
}

/**
  * @brief  USART1 初始化（04 章同款）
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
    while (1);
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
