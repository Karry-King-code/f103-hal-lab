/**
  ******************************************************************************
  * @file    Templates/Src/main.c
  * @author  MCD Application Team
  * @brief   Main program body（04 章：串口收发——HAL 版）
  * @note    USART1 = PA9(TX)/PA10(RX)，115200-8-N-1，收什么回什么（echo）。
  *          时钟仍是官方模板的 HSI->64MHz（APB2=64MHz），HAL 自己算 BRR。
  *          每收到 1 字节：回显 + 翻转绿灯（PC13）。
  *          页码注释约定：RM0008 = 《STM32F103xx参考手册(中文参考)》，页码为 PDF 阅读器页码
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

  HAL_Init();                                                /* 底层=配 SysTick 1ms 中断 + NVIC 优先级分组 */

  /* Configure the system clock to 64 MHz */
  SystemClock_Config();                                      /* 底层=写 RCC_CR/CFGR + FLASH_ACR；APB2=64MHz、APB1=32MHz（HAL 用 PCLK 算 BRR 就靠这个数） */

  /* ==== 时钟：GPIOA、GPIOC、USART1（在 APB2 上，64MHz） ==== */
  __HAL_RCC_GPIOA_CLK_ENABLE();                              /* 底层=RCC->APB2ENR bit2(IOPAEN)；地址 0x40021018；RM0008 中文版 PDF 第77页 */
  __HAL_RCC_GPIOC_CLK_ENABLE();                              /* 底层=APB2ENR bit4(IOPCEN) */
  __HAL_RCC_USART1_CLK_ENABLE();                             /* 底层=APB2ENR bit14(USART1EN) */

  /* ==== PA9=TX：复用推挽（引脚控制权交给 USART） ==== */
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin      = GPIO_PIN_9;
  GPIO_InitStruct.Mode     = GPIO_MODE_AF_PP;                /* 复用推挽 = CNF=10+MODE=11 = 0b1011 填进 CRH[7:4]；RM0008 中文版 PDF 第113页(编码表)/第114页(CRH位表) */
  GPIO_InitStruct.Speed    = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);                    /* 底层=读改写 GPIOA->CRH */

  /* ==== PA10=RX：浮空输入 ==== */
  GPIO_InitStruct.Pin  = GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;                    /* 输入 = MODE=00；Pull 决定 CNF（NOPULL→浮空 01，PULLUP/DOWN→1x） */
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* ==== PC13 绿灯：回显反馈 ==== */
  GPIO_InitStruct.Pin   = GPIO_PIN_13;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);   /* 初始：灭（底层=GPIOC->BSRR 置位） */

  /* ==== USART1：115200-8-N-1 ==== */
  USART1_UART_Init();

  /* ==== 上电横幅（HAL_UART_Transmit：阻塞发一整块） ==== */
  uint8_t banner[] = "\r\n=== STM32F103 USART1 READY - HAL (115200-8-N-1) ===\r\n"
                     "Type anything, I will echo it back. LED toggles per byte.\r\n";
  HAL_UART_Transmit(&huart1, banner, sizeof(banner) - 1, HAL_MAX_DELAY);   /* 底层=逐字节等 USART1->SR 的 TXE(bit7) 后写 USART1->DR；位表 RM0008 PDF 第521页 */

  /* Infinite loop */
  while (1)
  {
    /* 收 1 字节（阻塞轮询模式）；读完成 HAL 内部已清 RXNE */
    if (HAL_UART_Receive(&huart1, &b, 1, HAL_MAX_DELAY) == HAL_OK)   /* 底层=轮询 SR 的 RXNE(bit5) 后读 DR（读 DR 自动清标志）；PDF 第523页 */
    {
      HAL_UART_Transmit(&huart1, &b, 1, HAL_MAX_DELAY);  /* 原样发回 */
      HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);            /* 底层=翻转 GPIOC 的 bit13（不同 HAL 版本有的走 ODR 异或、有的走 BSRR 原子翻转，效果相同） */
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
  huart1.Init.BaudRate               = 115200;              /* 底层=用 PCLK2(64MHz) 算 BRR：64e6/16/115200≈34.72 → 0x22C 左右。
                                                                ★同一参数"115200"，HAL 算出 0x22C，标准库腿(72MHz)算出 0x271——它们不是同一个值，因为时钟不同！ */
  huart1.Init.WordLength             = UART_WORDLENGTH_8B;   /* 底层=CR1 的 M 位(bit12)=0 */
  huart1.Init.StopBits               = UART_STOPBITS_1;      /* 底层=CR2 的 STOP 位段=00 */
  huart1.Init.Parity                 = UART_PARITY_NONE;     /* 底层=CR1 的 PCE(bit10)=0 */
  huart1.Init.Mode                   = UART_MODE_TX_RX;      /* 底层=CR1 的 TE(bit3)+RE(bit2) */
  huart1.Init.HwFlowCtl              = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling           = UART_OVERSAMPLING_16; /* 16 倍过采样（对应 CR1 的 OVER8=0），BRR 按 /16 算 */
  if (HAL_UART_Init(&huart1) != HAL_OK)                      /* 底层=写 BRR/CR1/CR2 并置 USART1->CR1 的 bit13(UE)；组表 RM0008 PDF 第517页、CR1 位表第518页 */
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
  oscinitstruct.HSIState        = RCC_HSI_ON;              /* RCC_CR bit0=1 开内部 8MHz */
  oscinitstruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  oscinitstruct.HSEPredivValue    = RCC_HSE_PREDIV_DIV1;
  oscinitstruct.PLL.PLLState    = RCC_PLL_ON;
  oscinitstruct.PLL.PLLSource   = RCC_PLLSOURCE_HSI_DIV2;  /* RCC_CFGR：PLL 源=HSI/2 */
  oscinitstruct.PLL.PLLMUL      = RCC_PLL_MUL16;           /* 8/2*16=64MHz */
  if (HAL_RCC_OscConfig(&oscinitstruct)!= HAL_OK)          /* 底层=写 RCC_CR/CFGR 等锁定；寄存器组表 RM0008 PDF 第69页 */
  {
    while(1);
  }

  clkinitstruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  clkinitstruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;    /* RCC_CFGR SW：系统时钟=PLL */
  clkinitstruct.AHBCLKDivider = RCC_SYSCLK_DIV1;           /* HPRE：HCLK=SYSCLK */
  clkinitstruct.APB2CLKDivider = RCC_HCLK_DIV1;            /* PPRE2：PCLK2=64MHz（USART1 用的就是它） */
  clkinitstruct.APB1CLKDivider = RCC_HCLK_DIV2;            /* PPRE1：PCLK1=32MHz（≤36MHz 限制内） */
  if (HAL_RCC_ClockConfig(&clkinitstruct, FLASH_LATENCY_2)!= HAL_OK)   /* FLASH_LATENCY_2 → FLASH_ACR 2 等待周期 */
  {
    while(1);
  }
}
