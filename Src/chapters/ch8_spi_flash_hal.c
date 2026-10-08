/**
  ******************************************************************************
  * @file    main.c
  * @brief   任务八（HAL版）：SPI W25Q 读 JEDEC ID + 读数据
  * @note    板载 Flash 挂 SPI2：CS=PB12(手动) SCK=PB13 MISO=PB14 MOSI=PB15
  *          HAL_SPI 主机 8位 模式0，分频 8（APB1 36MHz → 4.5MHz）
  *          读 ID 命令 0x9F：厂商 + 类型 + 容量（实测本板 EF 40 15 = W25Q16 2MB）
  *          结果写 RAM 0x20004000：[0]=0xAA 读到厂商ID [1..3]=ID [4..7]=首4字节
  *          ⚠️ 烧录前拔掉光敏(DO)和温湿度(DAT)线——与 Flash 共用 PB12/PB14
  ******************************************************************************
  */
#include "main.h"

#define RES ((volatile uint8_t *)0x20004000)
static SPI_HandleTypeDef hspi2;
static UART_HandleTypeDef huart1;

static void uart_str(const char *s){ uint16_t n=0; while(s[n]) n++; HAL_UART_Transmit(&huart1,(uint8_t*)s,n,HAL_MAX_DELAY); }
static void uart_hex(uint8_t v){ char h[2]; const char *t="0123456789ABCDEF"; h[0]=t[v>>4]; h[1]=t[v&15]; HAL_UART_Transmit(&huart1,(uint8_t*)h,2,HAL_MAX_DELAY); }

static void cs_low(void){ HAL_GPIO_WritePin(GPIOB,GPIO_PIN_12,GPIO_PIN_RESET); }
static void cs_high(void){ HAL_GPIO_WritePin(GPIOB,GPIO_PIN_12,GPIO_PIN_SET); }

static uint8_t spi_xfer(uint8_t b)
{
  uint8_t r;
  HAL_SPI_TransmitReceive(&hspi2, &b, &r, 1, HAL_MAX_DELAY);
  return r;
}

void SystemClock_Config(void);

int main(void)
{
  GPIO_InitTypeDef g = {0};
  uint8_t id[3], data0[4], sr, i;

  HAL_Init();
  SystemClock_Config();

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_USART1_CLK_ENABLE();
  __HAL_RCC_SPI2_CLK_ENABLE();

  /* PB12 = 片选（推挽输出，空闲高） */
  g.Pin=GPIO_PIN_12; g.Mode=GPIO_MODE_OUTPUT_PP; g.Pull=GPIO_NOPULL; g.Speed=GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB,&g);
  cs_high();
  /* PB13/PB15 = SPI 复用推挽 */
  g.Pin=GPIO_PIN_13|GPIO_PIN_15; g.Mode=GPIO_MODE_AF_PP; g.Speed=GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB,&g);
  /* PB14 = MISO 输入 */
  g.Pin=GPIO_PIN_14; g.Mode=GPIO_MODE_INPUT; g.Pull=GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB,&g);

  /* SPI2 = 主机 8位 模式0 分频8 */
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 7;
  if (HAL_SPI_Init(&hspi2) != HAL_OK) while(1);

  /* USART1 */
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

  uart_str("\r\n=== TASK8 HAL: W25Q SPI READ ID ===\r\n");

  /* ① 状态寄存器 */
  cs_low(); spi_xfer(0x05); sr = spi_xfer(0xFF); cs_high();
  uart_str("STATUS=0x"); uart_hex(sr); uart_str(sr==0 ? " (idle)\r\n" : " (BUSY!)\r\n");

  /* ② JEDEC ID */
  cs_low();
  spi_xfer(0x9F);
  id[0]=spi_xfer(0xFF); id[1]=spi_xfer(0xFF); id[2]=spi_xfer(0xFF);
  cs_high();
  uart_str("JEDEC ID: "); uart_hex(id[0]); uart_str(" "); uart_hex(id[1]); uart_str(" "); uart_hex(id[2]);
  if(id[0]==0xEF && id[1]==0x40) uart_str("  -> Winbond OK\r\n");
  else uart_str("  -> 读取失败\r\n");

  /* ③ 读地址0起4字节 */
  cs_low();
  spi_xfer(0x03); spi_xfer(0x00); spi_xfer(0x00); spi_xfer(0x00);
  for(i=0;i<4;i++) data0[i]=spi_xfer(0xFF);
  cs_high();
  uart_str("DATA[0..3]: ");
  for(i=0;i<4;i++){ uart_hex(data0[i]); uart_str(" "); }
  uart_str("\r\n");

  RES[0]=(id[0]==0xEF)?0xAA:0x00;
  RES[1]=id[0]; RES[2]=id[1]; RES[3]=id[2];
  RES[4]=data0[0]; RES[5]=data0[1]; RES[6]=data0[2]; RES[7]=data0[3];

  while(1){ }
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
