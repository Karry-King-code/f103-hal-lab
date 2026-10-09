/**
  ******************************************************************************
  * @file    main.c
  * @brief   任务八（HAL版）：SPI W25Q 完整测试——擦除/写入/读回/断电不丢
  * @note    流程：①读ID ②读地址0的5字节 → 若等于 "STM32" 说明数据还在（断电不丢！）
  *                否则：擦除扇区 → 写 "STM32" → 读回比对
  *          结果写 RAM 0x20004000：[0]=0xAA(ID OK) [1]=0xBB(写成功) [2]=0xCC(持久)
  *          ⚠️ 做写入测试必须拔掉光敏(PB12)和温湿度(PB14)的线
  ******************************************************************************
  */
#include "main.h"

#define RES   ((volatile uint8_t *)0x20004000)
#define MAGIC "STM32"
#define ADDR  0x000000u

static SPI_HandleTypeDef  hspi2;
static UART_HandleTypeDef huart1;

void SystemClock_Config(void);

static void uart1_str(const char *s){ uint16_t n=0; while(s[n]) n++; HAL_UART_Transmit(&huart1,(uint8_t*)s,n,HAL_MAX_DELAY); }
static void uart1_hex(uint8_t v){ char h[2]; const char *t="0123456789ABCDEF"; h[0]=t[v>>4]; h[1]=t[v&15]; HAL_UART_Transmit(&huart1,(uint8_t*)h,2,HAL_MAX_DELAY); }

static void cs_low(void){ HAL_GPIO_WritePin(GPIOB,GPIO_PIN_12,GPIO_PIN_RESET); }
static void cs_high(void){ HAL_GPIO_WritePin(GPIOB,GPIO_PIN_12,GPIO_PIN_SET); }

static uint8_t spi_xfer(uint8_t b)
{
    uint8_t r;
    HAL_SPI_TransmitReceive(&hspi2, &b, &r, 1, HAL_MAX_DELAY);
    return r;
}

static void wait_ready(void)
{
    uint8_t sr;
    do { cs_low(); spi_xfer(0x05); sr = spi_xfer(0xFF); cs_high(); } while (sr & 0x01);
}

static void flash_read(uint32_t addr, uint8_t *buf, uint8_t n)
{
    uint8_t i;
    cs_low();
    spi_xfer(0x03);
    spi_xfer((uint8_t)(addr>>16)); spi_xfer((uint8_t)(addr>>8)); spi_xfer((uint8_t)addr);
    for(i=0;i<n;i++) buf[i] = spi_xfer(0xFF);
    cs_high();
}

int main(void)
{
    GPIO_InitTypeDef g = {0};
    uint8_t id[3], buf[5], i, same;
    const char *magic = MAGIC;

    HAL_Init();
    SystemClock_Config();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_SPI2_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();

    g.Pin=GPIO_PIN_12; g.Mode=GPIO_MODE_OUTPUT_PP; g.Pull=GPIO_NOPULL; g.Speed=GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB,&g); cs_high();
    g.Pin=GPIO_PIN_13|GPIO_PIN_15; g.Mode=GPIO_MODE_AF_PP; g.Speed=GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB,&g);
    g.Pin=GPIO_PIN_14; g.Mode=GPIO_MODE_INPUT; g.Pull=GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB,&g);
    g.Pin=GPIO_PIN_13; g.Mode=GPIO_MODE_OUTPUT_PP; g.Speed=GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC,&g);

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

    g.Pin=GPIO_PIN_9; g.Mode=GPIO_MODE_AF_PP; g.Speed=GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA,&g);
    g.Pin=GPIO_PIN_10; g.Mode=GPIO_MODE_INPUT; g.Pull=GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA,&g);
    huart1.Instance=USART1;
    huart1.Init.BaudRate=115200; huart1.Init.WordLength=UART_WORDLENGTH_8B;
    huart1.Init.StopBits=UART_STOPBITS_1; huart1.Init.Parity=UART_PARITY_NONE;
    huart1.Init.Mode=UART_MODE_TX_RX; huart1.Init.HwFlowCtl=UART_HWCONTROL_NONE;
    huart1.Init.OverSampling=UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart1);

    uart1_str("\r\n=== TASK8 HAL: FLASH WRITE/PERSIST TEST ===\r\n");

    /* ① 读 ID */
    cs_low(); spi_xfer(0x9F);
    id[0]=spi_xfer(0xFF); id[1]=spi_xfer(0xFF); id[2]=spi_xfer(0xFF);
    cs_high();
    uart1_str("JEDEC ID: "); uart1_hex(id[0]); uart1_str(" "); uart1_hex(id[1]); uart1_str(" "); uart1_hex(id[2]);
    uart1_str(" (W25Q 系列)\r\n");
    RES[0] = (id[0]==0xEF) ? 0xAA : 0x00;

    /* ② 先读地址0的5字节 */
    flash_read(ADDR, buf, 5);
    uart1_str("READ before: ");
    for(i=0;i<5;i++){ uart1_hex(buf[i]); uart1_str(" "); }
    uart1_str("\r\n");

    same = 1;
    for(i=0;i<5;i++) if(buf[i] != (uint8_t)magic[i]) same = 0;

    if (same) {
        uart1_str("PERSIST OK: \"STM32\" survived reset! (flash keeps data)\r\n");
        RES[2] = 0xCC;
    } else {
        uart1_str("Erasing sector...\r\n");
        cs_low(); spi_xfer(0x06); cs_high();
        cs_low();
        spi_xfer(0x20);
        spi_xfer((uint8_t)(ADDR>>16)); spi_xfer((uint8_t)(ADDR>>8)); spi_xfer((uint8_t)ADDR);
        cs_high();
        wait_ready();
        uart1_str("Erase done\r\n");

        cs_low(); spi_xfer(0x06); cs_high();
        cs_low();
        spi_xfer(0x02);
        spi_xfer((uint8_t)(ADDR>>16)); spi_xfer((uint8_t)(ADDR>>8)); spi_xfer((uint8_t)ADDR);
        for(i=0;i<5;i++) spi_xfer((uint8_t)magic[i]);
        cs_high();
        wait_ready();
        uart1_str("Write done\r\n");

        flash_read(ADDR, buf, 5);
        uart1_str("READ after : ");
        for(i=0;i<5;i++){ uart1_hex(buf[i]); uart1_str(" "); }
        same = 1;
        for(i=0;i<5;i++) if(buf[i] != (uint8_t)magic[i]) same = 0;
        uart1_str(same ? " -> VERIFY PASS\r\n" : " -> VERIFY FAIL\r\n");
        RES[1] = same ? 0xBB : 0x00;
        if (same) uart1_str("Now press RESET: data should stay\r\n");
    }

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
