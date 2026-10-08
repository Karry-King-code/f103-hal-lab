/**
  ******************************************************************************
  * @file    main.c
  * @brief   任务五+六（HAL版）：DHT11 温湿度(PB14) + 光敏(PB12) 串口打印
  * @note    与标准库版功能相同，全部换成 HAL 函数：
  *          光敏=PULLUP输入 / DHT=读时切输出拉低、读完切上拉输入 / HAL_UART_Transmit
  *          微秒延时用 DWT 周期计数器（三条腿通用）
  *          结果同时写 RAM 0x20004000：[0]光敏 [1]err [2]湿度 [3]温度 [4]成功数
  ******************************************************************************
  */
#include "main.h"

#define RES ((volatile uint8_t *)0x20004000)
static UART_HandleTypeDef huart1;

/* DWT 微秒延时 */
#define DEMCR      (*(volatile uint32_t *)0xE000EDFC)
#define DWT_CTRL   (*(volatile uint32_t *)0xE0001000)
#define DWT_CYCCNT (*(volatile uint32_t *)0xE0001004)
static void dwt_init(void){ DEMCR |= (1u<<24); DWT_CYCCNT=0; DWT_CTRL |= 1u; }
static void delay_us(uint32_t us){ uint32_t s=DWT_CYCCNT; while((DWT_CYCCNT-s) < us*72){} }
static void delay_ms(uint32_t ms){ while(ms--) delay_us(1000); }

static void uart_str(const char *s){ uint16_t n=0; while(s[n]) n++; HAL_UART_Transmit(&huart1,(uint8_t*)s,n,HAL_MAX_DELAY); }
static void uart_num(uint8_t v){ char b[4]; b[0]=(char)('0'+v/100); b[1]=(char)('0'+v/10%10); b[2]=(char)('0'+v%10); HAL_UART_Transmit(&huart1,(uint8_t*)b,3,HAL_MAX_DELAY); }

void SystemClock_Config(void);

/* ---------- DHT11（PB14）：HAL 用 GPIOMode 切换，不用碰 CRH ---------- */
static void dht_out_low(void)
{
  GPIO_InitTypeDef g = {0};
  g.Pin=GPIO_PIN_14; g.Mode=GPIO_MODE_OUTPUT_PP; g.Speed=GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB,&g);
  HAL_GPIO_WritePin(GPIOB,GPIO_PIN_14,GPIO_PIN_RESET);
}
static void dht_rel(void)
{
  GPIO_InitTypeDef g = {0};
  g.Pin=GPIO_PIN_14; g.Mode=GPIO_MODE_INPUT; g.Pull=GPIO_PULLUP;   /* 上拉，DHT 靠它回高 */
  HAL_GPIO_Init(GPIOB,&g);
}
static uint8_t dht_read_pin(void){ return (HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_14)==GPIO_PIN_SET)?1:0; }

static uint8_t wait_line(uint8_t lv)
{
  uint32_t t0 = DWT_CYCCNT;
  while (dht_read_pin() != lv)
    if ((DWT_CYCCNT - t0) > 5000u*72) return 1;
  return 0;
}
/* 0=成功 1=超时 2=校验错 */
static uint8_t dht11_read(uint8_t d[5])
{
  uint8_t i, j;
  for(i=0;i<5;i++) d[i]=0;
  dht_out_low(); delay_ms(20); dht_rel();
  if(wait_line(0)) return 1;
  if(wait_line(1)) return 1;
  if(wait_line(0)) return 1;              /* ★ 等应答高结束，漏掉会错位一位 */
  for(i=0;i<5;i++){
    for(j=0;j<8;j++){
      if(wait_line(1)) return 1;
      delay_us(40);
      d[i] = (uint8_t)(d[i]<<1);
      if(dht_read_pin()){ d[i] |= 1; if(wait_line(0)) return 1; }
    }
  }
  if((uint8_t)(d[0]+d[1]+d[2]+d[3]) != d[4]) return 2;
  return 0;
}

int main(void)
{
  GPIO_InitTypeDef g = {0};
  uint8_t d[5], err, light, ok_cnt = 0;

  HAL_Init();
  SystemClock_Config();
  dwt_init();

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_USART1_CLK_ENABLE();

  /* 光敏 PB12：上拉输入 ★ 本任务的关键一行 */
  g.Pin=GPIO_PIN_12; g.Mode=GPIO_MODE_INPUT; g.Pull=GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB,&g);
  /* DHT PB14：先上拉输入 */
  g.Pin=GPIO_PIN_14; g.Mode=GPIO_MODE_INPUT; g.Pull=GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB,&g);
  /* PC13 心跳灯 */
  g.Pin=GPIO_PIN_13; g.Mode=GPIO_MODE_OUTPUT_PP; g.Pull=GPIO_NOPULL; g.Speed=GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC,&g);
  HAL_GPIO_WritePin(GPIOC,GPIO_PIN_13,GPIO_PIN_SET);

  /* USART1 PA9/PA10 */
  g.Pin=GPIO_PIN_9; g.Mode=GPIO_MODE_AF_PP; g.Speed=GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA,&g);
  g.Pin=GPIO_PIN_10; g.Mode=GPIO_MODE_INPUT; g.Pull=GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA,&g);

  huart1.Instance=USART1;
  huart1.Init.BaudRate=115200;
  huart1.Init.WordLength=UART_WORDLENGTH_8B;
  huart1.Init.StopBits=UART_STOPBITS_1;
  huart1.Init.Parity=UART_PARITY_NONE;
  huart1.Init.Mode=UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl=UART_HWCONTROL_NONE;
  huart1.Init.OverSampling=UART_OVERSAMPLING_16;
  HAL_UART_Init(&huart1);

  uart_str("\r\n=== TASK5+6 HAL: DHT11(PB14) + LIGHT(PB12) ===\r\n");

  while(1)
  {
    light = (HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_12)==GPIO_PIN_SET)?1:0;
    err = dht11_read(d);

    uart_str("LIGHT="); uart_str(light?"1":"0");
    if(err==0){
      uart_str("  HUMI="); uart_num(d[0]); uart_str("."); uart_str((char[]){'0'+d[1],0}); uart_str("%");
      uart_str("  TEMP="); uart_num(d[2]); uart_str("."); uart_str((char[]){'0'+d[3],0}); uart_str("C\r\n");
      ok_cnt++;
    } else {
      uart_str(err==1 ? "  DHT11 TIMEOUT (power-cycle VCC if stuck)\r\n" : "  DHT11 CHECKSUM ERR\r\n");
    }

    RES[0]=light; RES[1]=err; RES[2]=d[0]; RES[3]=d[2]; RES[4]=ok_cnt;

    delay_ms(2000);
    HAL_GPIO_TogglePin(GPIOC,GPIO_PIN_13);
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
  if (HAL_RCC_OscConfig(&oscinitstruct)!= HAL_OK) while(1);
  clkinitstruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  clkinitstruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  clkinitstruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  clkinitstruct.APB2CLKDivider = RCC_HCLK_DIV1;
  clkinitstruct.APB1CLKDivider = RCC_HCLK_DIV2;
  if (HAL_RCC_ClockConfig(&clkinitstruct, FLASH_LATENCY_2)!= HAL_OK) while(1);
}
