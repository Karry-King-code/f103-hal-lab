/**
  ******************************************************************************
  * @file    main.c
  * @brief   【练习】任务一：点亮 LED —— HAL 版，GPIO 部分全部自己写
  * @note    规则：
  *          1. 只看下面的"目标"，不看任何代码，凭记忆写
  *          2. 卡住先翻《三合一-寄存器手册》→ 再翻三合一"看图说话"
  *          3. 最后才允许看答案：git diff main -- Src/main.c
  *          4. 看完答案必须合上，重新默写
  *          SystemClock_Config 是官方模板脚手架（已给你），GPIO 流程自己写
  ******************************************************************************
  */
#include "main.h"

void SystemClock_Config(void);

int main(void)
{
  HAL_Init();                    /* 必须最先调：它把 SysTick 配成 1ms 时基（HAL_Delay 靠它） */
  SystemClock_Config();          /* 官方模板脚手架：HSI -> 64MHz（已给你） */

  /* ===== 目标（每一步做完打勾）=====
   * [ ] ① 开 GPIOC 时钟
   * [ ] ② PC13 配置成推挽输出（Mode/Pull/Speed 三个字段都要填，Pull 填什么？）
   * [ ] ③ 上电先让灯灭
   * [ ] ④ 主循环：亮 1 秒 -> 灭 1 秒（HAL_Delay 为什么依赖中断？）
   *
   * [ ] 编译 0 错 0 警
   * [ ] 烧录（.axf，记得 -rst）
   * [ ] 绿灯 1 秒闪 = 通关，git add -A && git commit -m "任务一完成"
   */

  while (1)
  {
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
