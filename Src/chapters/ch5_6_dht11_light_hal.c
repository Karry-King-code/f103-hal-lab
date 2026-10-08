/**
  ******************************************************************************
  * @file    main.c
  * @brief   任务五+六（HAL版）：OLED显示 + DHT11温湿度(PB14) + 光敏(PB12)
  * @note    屏幕四行：F103 SMART ENV / LIGHT:OK|DARK / HUMI:xx.x% / TEMP:xx.xC
  *          全部用 HAL 函数：I2C 软件模拟用 HAL_GPIO_Init/WritePin/ReadPin，
  *          光敏=PULLUP输入，DHT=读取时切换 OUTPUT_PP(拉低)/INPUT+PULLUP(释放)。
  *          结果同时写 RAM 0x20004000：[0]光敏 [1]err [2]湿度 [3]温度 [4]成功数
  ******************************************************************************
  */
#include "main.h"

#define RES ((volatile uint8_t *)0x20004000)
static UART_HandleTypeDef huart1;

/* ---- DWT 微秒延时 ---- */
#define DEMCR      (*(volatile uint32_t *)0xE000EDFC)
#define DWT_CTRL   (*(volatile uint32_t *)0xE0001000)
#define DWT_CYCCNT (*(volatile uint32_t *)0xE0001004)
static void dwt_init(void){ DEMCR |= (1u<<24); DWT_CYCCNT=0; DWT_CTRL |= 1u; }
static void delay_us(uint32_t us){ uint32_t s=DWT_CYCCNT; while((DWT_CYCCNT-s) < us*72){} }
static void delay_ms(uint32_t ms){ while(ms--) delay_us(1000); }

static void uart_str(const char *s){ uint16_t n=0; while(s[n]) n++; HAL_UART_Transmit(&huart1,(uint8_t*)s,n,HAL_MAX_DELAY); }
static void uart_dec2(uint8_t v){ char b[2]; b[0]=(char)('0'+v/10%10); b[1]=(char)('0'+v%10); HAL_UART_Transmit(&huart1,(uint8_t*)b,2,HAL_MAX_DELAY); }

void SystemClock_Config(void);

/* ================= 软件 I2C（HAL 版）PB8=SCL PB9=SDA ================= */
static void i2c_dly(void){ delay_us(3); }
static void scl_hi(void){ HAL_GPIO_WritePin(GPIOB,GPIO_PIN_8,GPIO_PIN_SET); }
static void scl_lo(void){ HAL_GPIO_WritePin(GPIOB,GPIO_PIN_8,GPIO_PIN_RESET); }
static void sda_lo(void)      /* 推挽输出拉低 */
{
  GPIO_InitTypeDef g = {0};
  g.Pin=GPIO_PIN_9; g.Mode=GPIO_MODE_OUTPUT_PP; g.Speed=GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB,&g);
  HAL_GPIO_WritePin(GPIOB,GPIO_PIN_9,GPIO_PIN_RESET);
}
static void sda_rel(void)     /* 释放：上拉输入（漏极开路必须上拉） */
{
  GPIO_InitTypeDef g = {0};
  g.Pin=GPIO_PIN_9; g.Mode=GPIO_MODE_INPUT; g.Pull=GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB,&g);
}
static uint8_t sda_rd(void){ return (HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_9)==GPIO_PIN_SET)?1:0; }

static void i2c_start(void){ sda_rel(); i2c_dly(); scl_hi(); i2c_dly(); sda_lo(); i2c_dly(); scl_lo(); i2c_dly(); }
static void i2c_stop(void){ sda_lo(); i2c_dly(); scl_hi(); i2c_dly(); sda_rel(); i2c_dly(); }
static uint8_t i2c_wr(uint8_t b)
{
  uint8_t i, ack;
  for(i=0;i<8;i++){
    if(b&0x80) sda_rel(); else sda_lo();
    b<<=1; i2c_dly(); scl_hi(); i2c_dly(); scl_lo();
  }
  sda_rel(); i2c_dly(); scl_hi(); i2c_dly();
  ack = sda_rd(); scl_lo();
  return ack;
}

static const uint8_t F57[] = {
0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x5F,0x00,0x00, 0x00,0x07,0x00,0x07,0x00, 0x14,0x7F,0x14,0x7F,0x14,
0x24,0x2A,0x7F,0x2A,0x12, 0x23,0x13,0x08,0x64,0x62, 0x36,0x49,0x55,0x22,0x50, 0x00,0x05,0x03,0x00,0x00,
0x00,0x1C,0x22,0x41,0x00, 0x00,0x41,0x22,0x1C,0x00, 0x14,0x08,0x3E,0x08,0x14, 0x08,0x08,0x3E,0x08,0x08,
0x00,0x50,0x30,0x00,0x00, 0x08,0x08,0x08,0x08,0x08, 0x00,0x60,0x60,0x00,0x00, 0x20,0x10,0x08,0x04,0x02,
0x3E,0x51,0x49,0x45,0x3E, 0x00,0x42,0x7F,0x40,0x00, 0x42,0x61,0x51,0x49,0x46, 0x21,0x41,0x45,0x4B,0x31,
0x18,0x14,0x12,0x7F,0x10, 0x27,0x45,0x45,0x45,0x39, 0x3C,0x4A,0x49,0x49,0x30, 0x01,0x71,0x09,0x05,0x03,
0x36,0x49,0x49,0x49,0x36, 0x06,0x49,0x49,0x29,0x1E, 0x00,0x36,0x36,0x00,0x00, 0x00,0x56,0x36,0x00,0x00,
0x08,0x14,0x22,0x41,0x00, 0x14,0x14,0x14,0x14,0x14, 0x00,0x41,0x22,0x14,0x08, 0x02,0x01,0x51,0x09,0x06,
0x32,0x49,0x79,0x41,0x3E, 0x7E,0x11,0x11,0x11,0x7E, 0x7F,0x49,0x49,0x49,0x36, 0x3E,0x41,0x41,0x41,0x22,
0x7F,0x41,0x41,0x22,0x1C, 0x7F,0x49,0x49,0x49,0x41, 0x7F,0x09,0x09,0x09,0x01, 0x3E,0x41,0x49,0x49,0x7A,
0x7F,0x08,0x08,0x08,0x7F, 0x00,0x41,0x7F,0x41,0x00, 0x20,0x40,0x41,0x3F,0x01, 0x7F,0x08,0x14,0x22,0x41,
0x7F,0x40,0x40,0x40,0x40, 0x7F,0x02,0x0C,0x02,0x7F, 0x7F,0x04,0x08,0x10,0x7F, 0x3E,0x41,0x41,0x41,0x3E,
0x7F,0x09,0x09,0x09,0x06, 0x3E,0x41,0x51,0x21,0x5E, 0x7F,0x09,0x19,0x29,0x46, 0x46,0x49,0x49,0x49,0x31,
0x01,0x01,0x7F,0x01,0x01, 0x3F,0x40,0x40,0x40,0x3F, 0x1F,0x20,0x40,0x20,0x1F, 0x3F,0x40,0x38,0x40,0x3F,
0x63,0x14,0x08,0x14,0x63, 0x07,0x08,0x70,0x08,0x07, 0x61,0x51,0x49,0x45,0x43 };

static void o_cmd(uint8_t c){ i2c_start(); i2c_wr(0x78); i2c_wr(0x00); i2c_wr(c); i2c_stop(); }
static void o_data(uint8_t d){ i2c_start(); i2c_wr(0x78); i2c_wr(0x40); i2c_wr(d); i2c_stop(); }
static void o_pos(uint8_t pg, uint8_t col){ o_cmd((uint8_t)(0xB0|pg)); o_cmd((uint8_t)(0x00|(col&0x0F))); o_cmd((uint8_t)(0x10|(col>>4))); }
static void o_print(uint8_t pg, uint8_t col, const char *s)
{
  o_pos(pg,col);
  while(*s){
    uint8_t c=(uint8_t)*s++; uint8_t i;
    if(c<0x20||c>0x5F) c=' ';
    for(i=0;i<5;i++) o_data(F57[(c-0x20)*5+i]);
    o_data(0x00);
  }
}
static void o_clear(uint8_t pg){ uint8_t i; o_pos(pg,0); for(i=0;i<128;i++) o_data(0x00); }
static void o_init(void)
{
  uint8_t i; for(i=0;i<100;i++) i2c_dly();
  i2c_start(); i2c_wr(0x78); i2c_wr(0x00);
  i2c_wr(0xAE); i2c_wr(0xD5); i2c_wr(0x80); i2c_wr(0xA8); i2c_wr(0x3F);
  i2c_wr(0xD3); i2c_wr(0x00); i2c_wr(0x40); i2c_wr(0x8D); i2c_wr(0x14);
  i2c_wr(0x20); i2c_wr(0x02); i2c_wr(0xA1); i2c_wr(0xC8); i2c_wr(0xDA);
  i2c_wr(0x12); i2c_wr(0x81); i2c_wr(0xCF); i2c_wr(0xD9); i2c_wr(0xF1);
  i2c_wr(0xDB); i2c_wr(0x40); i2c_wr(0xA4); i2c_wr(0xA6); i2c_wr(0xAF);
  i2c_stop();
}

/* ================= DHT11（PB14）HAL 版 ================= */
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
  g.Pin=GPIO_PIN_14; g.Mode=GPIO_MODE_INPUT; g.Pull=GPIO_PULLUP;
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
  if(wait_line(0)) return 1;      /* ★ 等应答高结束，漏掉会错位一位 */
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

/* ================= 主程序 ================= */
int main(void)
{
  GPIO_InitTypeDef g = {0};
  uint8_t d[5], err, lastv=0xFF, humi_i=0xFF, temp_i=0xFF, ok_cnt=0;
  char buf[8];

  HAL_Init();
  SystemClock_Config();
  dwt_init();

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_USART1_CLK_ENABLE();

  /* 光敏 PB12：上拉输入 ★（开漏输出必须上拉） */
  g.Pin=GPIO_PIN_12; g.Mode=GPIO_MODE_INPUT; g.Pull=GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB,&g);
  /* DHT PB14：上拉输入（读取时切换） */
  g.Pin=GPIO_PIN_14; g.Mode=GPIO_MODE_INPUT; g.Pull=GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB,&g);
  /* PB8 = SCL 推挽输出 */
  g.Pin=GPIO_PIN_8; g.Mode=GPIO_MODE_OUTPUT_PP; g.Pull=GPIO_NOPULL; g.Speed=GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB,&g);
  scl_hi();
  sda_rel();
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

  /* 屏幕初始化 + 固定四行 */
  delay_ms(100);
  o_init();
  { uint8_t p; for(p=0;p<8;p++) o_clear(p); }
  o_print(0, 0, "F103 SMART ENV");
  o_print(2, 0, "LIGHT:");
  o_print(4, 0, "HUMI:");
  o_print(6, 0, "TEMP:");
  uart_str("\r\n=== TASK5+6 HAL: OLED + DHT11(PB14) + LIGHT(PB12) ===\r\n");

  while(1)
  {
    uint8_t now = (HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_12)==GPIO_PIN_SET)?1:0;
    if(now != lastv){ lastv = now; o_print(2, 48, now ? "OK  " : "DARK"); }

    err = dht11_read(d);
    RES[0]=now; RES[1]=err; RES[2]=d[0]; RES[3]=d[2]; RES[4]=ok_cnt;

    uart_str("LIGHT="); uart_str(now?"1":"0");
    if(err==0){
      uart_str("  HUMI="); uart_dec2(d[0]); uart_str("."); uart_str((char[]){'0'+d[1],0}); uart_str("%");
      uart_str("  TEMP="); uart_dec2(d[2]); uart_str("."); uart_str((char[]){'0'+d[3],0}); uart_str("C\r\n");
      ok_cnt++;
      if(d[0] != humi_i){ humi_i = d[0];
        o_print(4, 40, "      ");
        buf[0]=(char)('0'+d[0]/100); buf[1]=(char)('0'+d[0]/10%10); buf[2]=(char)('0'+d[0]%10); buf[3]='.'; buf[4]=(char)('0'+d[1]); buf[5]='%'; buf[6]=0;
        o_print(4, 40, buf); }
      if(d[2] != temp_i){ temp_i = d[2];
        o_print(6, 40, "      ");
        buf[0]=(char)('0'+d[2]/100); buf[1]=(char)('0'+d[2]/10%10); buf[2]=(char)('0'+d[2]%10); buf[3]='.'; buf[4]=(char)('0'+d[3]); buf[5]='C'; buf[6]=0;
        o_print(6, 40, buf); }
    } else {
      uart_str(err==1 ? "  DHT11 TIMEOUT\r\n" : "  DHT11 CHECKSUM ERR\r\n");
      o_print(4, 40, " -- ");
      o_print(6, 40, " -- ");
      humi_i=0xFF; temp_i=0xFF;
    }

    delay_ms(1500);
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
