/**
  ******************************************************************************
  * @file    Templates/Src/main.c
  * @author  MCD Application Team
  * @brief   Main program body
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
/* 页码注释约定：RM0008 = 《STM32F103xx参考手册(中文参考)》，页码为 PDF 阅读器页码 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/** @addtogroup STM32F1xx_HAL_Examples
  * @{
  */

/** @addtogroup Templates
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Main program
  * @param  None
  * @retval None
  */
int main(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* STM32F103xB HAL library initialization:
       - Configure the Flash prefetch
       - Systick timer is configured by default as source of time base, but user
         can eventually implement his proper time base source (a general purpose
         timer for example or other time source), keeping in mind that Time base
         duration should be kept 1ms since PPP_TIMEOUT_VALUEs are defined and
         handled in milliseconds basis.
       - Set NVIC Group Priority to 4
       - Low Level Initialization
     */
  HAL_Init();                                            /* 底层=配 SysTick 为 1ms 中断时钟源（内核寄存器 LOAD/VAL/CTRL）+NVIC 优先级分组4；HAL_Delay 全靠它 */

  /* Configure the system clock to 64 MHz */
  SystemClock_Config();                                  /* 底层=写 RCC_CR(HSI 开启 bit0)/RCC_CFGR(PLL 倍频位)/FLASH_ACR(等待周期)，寄存器组表 RM0008 中文版 PDF 第69页 */


  /* Add your application code here
     */

  /* 02 章：点亮 PC13 绿灯（实测：PC13=绿灯，低电平亮） */
  __HAL_RCC_GPIOC_CLK_ENABLE();                          /* 底层=置 RCC->APB2ENR 的 bit4(IOPCEN)；地址=0x40021000+0x18=0x40021018；RM0008 中文版 PDF 第69页(组表)/第77页(位表)。等价标准库的 RCC_APB2PeriphClockCmd */

  GPIO_InitStruct.Pin   = GPIO_PIN_13;                   /* PC13（库内部换算成 CRH 的 [23:20] 格子） */
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;           /* 推挽输出 = CNF=00（等价 GPIO_Mode_Out_PP） */
  GPIO_InitStruct.Pull  = GPIO_NOPULL;                   /* 输出不带上下拉 */
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;           /* 低速够用 = MODE=10（2MHz，等价 GPIO_Speed_2MHz） */
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);                /* 底层=读改写 GPIOC->CRH 的 [23:20] 填 0b0010；RM0008 中文版 PDF 第113页(编码表)/第114页(CRH位表) */

  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);   /* 底层=向 GPIOC->BSRR 写 1<<13 置位（高电平=灭）；PDF 第115页。上电先灭 */

  /* Infinite loop */
  while (1)
  {
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);  /* 低电平 -> 亮：底层=向 GPIOC->BSRR 高 16 位写 1<<13（复位位） */
    HAL_Delay(1000);                                        /* HAL 自带毫秒延时：累加 uwTick，uwTick 由 SysTick 1ms 中断递增——中断被关死它会卡住（与标准库/寄存器的软件循环延时的本质区别） */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);    /* 高电平 -> 灭：底层=向 GPIOC->BSRR 低 16 位写 1<<13（置位位） */
    HAL_Delay(1000);
  }
}

/**
  * @brief  System Clock Configuration
  *         The system Clock is configured as follow :
  *            System Clock source            = PLL (HSI)
  *            SYSCLK(Hz)                     = 64000000
  *            HCLK(Hz)                       = 64000000
  *            AHB Prescaler                  = 1
  *            APB1 Prescaler                 = 2
  *            APB2 Prescaler                 = 1
  *            PLLMUL                         = 16
  *            Flash Latency(WS)              = 2
  * @param  None
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_ClkInitTypeDef clkinitstruct = {0};
  RCC_OscInitTypeDef oscinitstruct = {0};

  /* Configure PLL ------------------------------------------------------*/
  /* PLL configuration: PLLCLK = (HSI / 2) * PLLMUL = (8 / 2) * 16 = 64 MHz */
  /* PREDIV1 configuration: PREDIV1CLK = PLLCLK / HSEPredivValue = 64 / 1 = 64 MHz */
  /* Enable HSI and activate PLL with HSi_DIV2 as source */
  oscinitstruct.OscillatorType  = RCC_OSCILLATORTYPE_HSI;
  oscinitstruct.HSEState        = RCC_HSE_OFF;
  oscinitstruct.LSEState        = RCC_LSE_OFF;
  oscinitstruct.HSIState        = RCC_HSI_ON;              /* RCC_CR bit0 HSION=1：开内部 8MHz 振荡器 */
  oscinitstruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  oscinitstruct.HSEPredivValue    = RCC_HSE_PREDIV_DIV1;
  oscinitstruct.PLL.PLLState    = RCC_PLL_ON;
  oscinitstruct.PLL.PLLSource   = RCC_PLLSOURCE_HSI_DIV2;  /* RCC_CFGR：PLL 时钟源=HSI/2 */
  oscinitstruct.PLL.PLLMUL      = RCC_PLL_MUL16;           /* RCC_CFGR PLLMUL 位段=16 倍：8/2*16=64MHz */
  if (HAL_RCC_OscConfig(&oscinitstruct)!= HAL_OK)          /* 底层=写 RCC_CR/RCC_CFGR 并等锁相标志；寄存器组表 RM0008 中文版 PDF 第69页 */
  {
    /* Initialization Error */
    while(1);
  }

  /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2
     clocks dividers */
  clkinitstruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  clkinitstruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;    /* RCC_CFGR SW 位段：系统时钟切到 PLL */
  clkinitstruct.AHBCLKDivider = RCC_SYSCLK_DIV1;           /* RCC_CFGR HPRE：HCLK=SYSCLK */
  clkinitstruct.APB2CLKDivider = RCC_HCLK_DIV1;            /* RCC_CFGR PPRE2：PCLK2=HCLK */
  clkinitstruct.APB1CLKDivider = RCC_HCLK_DIV2;            /* RCC_CFGR PPRE1：PCLK1=HCLK/2（最高 36MHz） */
  if (HAL_RCC_ClockConfig(&clkinitstruct, FLASH_LATENCY_2)!= HAL_OK)   /* FLASH_LATENCY_2=写 FLASH_ACR 等待周期（64MHz 需 2WS） */
  {
    /* Initialization Error */
    while(1);
  }
}

#ifdef  USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}
#endif

/**
  * @}
  */

/**
  * @}
  */
