/**
  ******************************************************************************
  * @file    Templates/Src/main.c
  * @author  MCD Application Team
  * @brief   Main program body（03 章：按键控制 LED——HAL 版）
  * @note    实测硬件：KEY1=PB7（上拉输入，按下=0）；绿灯=PC13（低电平亮）
  *          逻辑与标准库版完全相同：按一下开始 1s 闪烁，再按一下停止
  *          ——只是把标准库函数全部换成 HAL 函数
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
static uint8_t running = 0;          /* 闪烁开关：0=停 1=闪 */
static uint16_t slice = 0;           /* 10ms 小片计数 */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static uint8_t key_pressed(void);

/**
  * @brief  Main program
  */
int main(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  HAL_Init();                                        /* 底层=SysTick 1ms 中断 + NVIC 分组 */

  /* Configure the system clock to 64 MHz */
  SystemClock_Config();                              /* 底层=写 RCC_CR/CFGR/FLASH_ACR（详见任务二注释） */

  /* ==== 按键 PB7：上拉输入（HAL 把"上拉/下拉"做成显式的 Pull 字段） ==== */
  __HAL_RCC_GPIOB_CLK_ENABLE();                      /* 底层=RCC->APB2ENR bit3(IOPBEN)；RM0008 中文版 PDF 第77页 */
  __HAL_RCC_GPIOC_CLK_ENABLE();                      /* 底层=APB2ENR bit4(IOPCEN) */

  GPIO_InitStruct.Pin  = GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;            /* MODE=00 输入 */
  GPIO_InitStruct.Pull = GPIO_PULLUP;               /* 标准库藏进 ODR 的动作，这里明说。
                                                       HAL 内部：CNF 填 10(上/下拉) 且把 ODR bit7 置 1 —— 上拉真正由 ODR 决定（PDF 第113页编码表） */
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);            /* 底层=读改写 GPIOB->CRL 的 [31:28]（PB7 是第 7 格）+ 写 ODR；第114页位表 */

  /* ==== 绿灯 PC13：推挽输出 ==== */
  GPIO_InitStruct.Pin   = GPIO_PIN_13;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;       /* CNF=00 + MODE=10 = 0b0010 填进 CRH[23:20] */
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);            /* 底层=读改写 GPIOC->CRH */

  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);   /* 初始：灭；底层=GPIOC->BSRR 置位（PDF 第115页） */

  /* Infinite loop */
  while (1)
  {
    /* ① 每个周期开头照看按键 */
    if (key_pressed())
    {
      running = !running;                              /* 开 <-> 停 */
      if (running == 0)
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);   /* 停止：灯灭 */
    }

    /* ② 闪烁：非阻塞分片，每 500ms 翻一次（1s 周期） */
    if (running)
    {
      if (++slice >= 50)
      {
        slice = 0;
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);        /* HAL 自带翻转函数！底层=翻转 GPIOC 的 bit13（ODR 异或，或新版 HAL 用 BSRR 原子翻转） */
      }
    }

    HAL_Delay(10);                                     /* ③ 全局节拍器（靠 SysTick 中断累加 uwTick） */
  }
}

/**
  * @brief  查一次按键：捕捉"松开->按下"的沿（消抖+等释放）
  * @retval 1 = 完整按压一次；0 = 无事发生
  */
static uint8_t key_pressed(void)
{
  static uint8_t last = 1;
  uint8_t now = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7) == GPIO_PIN_RESET) ? 0 : 1;   /* 底层=读 GPIOB->IDR 的 bit7 */
  uint8_t evt = 0;

  if (last == 1 && now == 0)                           /* 1->0 = 按下沿 */
  {
    HAL_Delay(10);                                     /* 按下消抖 */
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7) == GPIO_PIN_RESET)
    {
      evt = 1;
      while (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7) == GPIO_PIN_RESET)
      {
      }                                                /* 等松手 */
      HAL_Delay(10);                                   /* 释放消抖 */
    }
  }
  last = now;
  return evt;
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
  oscinitstruct.HSIState        = RCC_HSI_ON;         /* RCC_CR bit0=1 开内部 8MHz */
  oscinitstruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  oscinitstruct.HSEPredivValue    = RCC_HSE_PREDIV_DIV1;
  oscinitstruct.PLL.PLLState    = RCC_PLL_ON;
  oscinitstruct.PLL.PLLSource   = RCC_PLLSOURCE_HSI_DIV2;
  oscinitstruct.PLL.PLLMUL      = RCC_PLL_MUL16;      /* 8/2*16=64MHz */
  if (HAL_RCC_OscConfig(&oscinitstruct)!= HAL_OK)     /* 寄存器组表 RM0008 PDF 第69页 */
  {
    while(1);
  }

  clkinitstruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  clkinitstruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  clkinitstruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  clkinitstruct.APB2CLKDivider = RCC_HCLK_DIV1;
  clkinitstruct.APB1CLKDivider = RCC_HCLK_DIV2;       /* PCLK1=32MHz */
  if (HAL_RCC_ClockConfig(&clkinitstruct, FLASH_LATENCY_2)!= HAL_OK)
  {
    while(1);
  }
}
