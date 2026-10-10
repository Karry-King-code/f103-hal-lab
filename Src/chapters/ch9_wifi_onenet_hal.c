/**
  ******************************************************************************
  * @file    main.c
  * @brief   任务九（HAL版）终版：DHT11(PB14)+光敏(PB12)+OLED(PB8/PB9)+MQTT上OneNET
  * @note    与 SPL 腿(标准库仓库 09 章)功能完全一致，三处显示一致：
  *            OLED 四行 = 串口1打印 = OneNET 云端（code:200）
  *          移植说明（本腿惯例）：
  *            - 时钟 = HSI 64MHz（不用外部晶振，ch5_6 同款 SystemClock_Config）
  *              DWT 延时仍按 *72 系数（沿用 ch5_6 验证过的写法，DHT/OLED 都容忍 12% 偏慢）
  *            - 串口收发全走 HAL_UART_Transmit / HAL_UART_Receive_IT + RxCpltCallback
  *            - ESP 接收 = 每字节中断 + 环形缓冲（head/tail 总计数+取模，永不收满变聋）
  *          实测（2026-10-10）：SPL 腿同款流程，平台回执 code:200，真值 25.8C/34%。
  *          串口打印一律 ASCII（中文只写注释，否则 AC5 #870-D 警告+乱码）。
  ******************************************************************************
  */
#include "main.h"

#define RES   ((volatile uint32_t *)0x20004000)

/* ---------------- 账号参数(改这里就能换设备/换WiFi) ---------------- */
#define WIFI_SSID   "1"
#define WIFI_PWD    "987654321"

#define MQTT_HOST   "mqtts.heclouds.com"
#define MQTT_PORT   "1883"
#define MQTT_CLIENT "dev1"
#define MQTT_USER   "T67q8WAChA"
#define MQTT_PASS   "version=2018-10-31&res=products/T67q8WAChA/devices/dev1&et=2106900860&method=md5&sign=fKgrFdzCemZ72jpNBVxrHA=="
#define KEEPALIVE   60

#define TOPIC_POST  "$sys/T67q8WAChA/dev1/thing/property/post"
#define TOPIC_SUB   "$sys/T67q8WAChA/dev1/thing/property/post/reply"

void SystemClock_Config(void);   /* 本文件底部定义(HSI 64MHz, ch5_6 同款) */

/* ================= DWT 微秒延时(DHT11 时序必须微秒级) ================= */
#define DEMCR      (*(volatile uint32_t *)0xE000EDFC)
#define DWT_CTRL   (*(volatile uint32_t *)0xE0001000)
#define DWT_CYCCNT (*(volatile uint32_t *)0xE0001004)
static void dwt_init(void){ DEMCR |= (1u<<24); DWT_CYCCNT=0; DWT_CTRL |= 1u; }
static void delay_us(uint32_t us){ uint32_t s=DWT_CYCCNT; while((DWT_CYCCNT-s) < us*72){} }
static void delay_ms(uint32_t ms){ while(ms--) delay_us(1000); }

/* ---------------- 串口环形缓冲区 + 每字节中断接收 ----------------
   环形: head/tail 是"总计数", 下标取模。普通数组收满就聋(实测踩过) */
#define RXCAP 2000
volatile uint32_t rx_head = 0, rx_tail = 0, ore_n = 0;
volatile uint8_t  rx_buf[RXCAP];
static uint8_t  saw_prompt = 0;
static volatile uint8_t rx_byte;                 /* HAL 每字节接收的落点 */
static UART_HandleTypeDef huart1;
static UART_HandleTypeDef huart2;

void USART2_IRQHandler(void) { HAL_UART_IRQHandler(&huart2); }

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        rx_buf[rx_head % RXCAP] = rx_byte;
        rx_head++;
        HAL_UART_Receive_IT(huart, (uint8_t *)&rx_byte, 1);   /* 重新挂上 */
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        ore_n++;                                   /* ORE 等错误: 计数并恢复接收 */
        HAL_UART_Receive_IT(huart, (uint8_t *)&rx_byte, 1);
    }
}

/* ---------------- 串口1: 打印 ---------------- */
static void uart1_init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();

    g.Pin = GPIO_PIN_9; g.Mode = GPIO_MODE_AF_PP; g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &g);
    g.Pin = GPIO_PIN_10; g.Mode = GPIO_MODE_INPUT; g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &g);

    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart1);
}
static void u1ch(char c) { HAL_UART_Transmit(&huart1, (uint8_t *)&c, 1, HAL_MAX_DELAY); }
static void u1str(const char *s) { while (*s) u1ch(*s++); }
static void u1num(uint32_t v)
{
    char b[11]; int i = 0;
    if (v == 0) { u1ch('0'); return; }
    while (v) { b[i++] = (char)('0' + v % 10); v /= 10; }
    while (i--) u1ch(b[i]);
}

/* ---------------- 串口2: 发字节给 ESP ---------------- */
static void u2put(char c) { HAL_UART_Transmit(&huart2, (uint8_t *)&c, 1, 100); }
static void u2str(const char *s) { while (*s) u2put(*s++); }

static void flush_rx(void)
{
    uint32_t h = rx_head;
    while (rx_tail < h)
    {
        uint8_t c = rx_buf[rx_tail % RXCAP];
        rx_tail++;
        if (c == '>') saw_prompt = 1;
        if (c == '\n') { u1ch('\r'); u1ch('\n'); }
        else if (c == '\r') { }
        else if (c >= 32 && c < 127) u1ch((char)c);
        else { u1ch('['); u1num(c); u1ch(']'); }
    }
}

static void pump(uint32_t ms)
{
    uint32_t k;
    for (k = 0; k < ms; k++) { flush_rx(); delay_ms(1); }
    flush_rx();
}

static void at_cmd(const char *cmd, uint32_t ms)
{
    u1str("\r\n===== SEND: "); u1str(cmd); u1str(" =====\r\n");
    flush_rx();
    rx_tail = rx_head;                     /* 丢弃上一条的尾巴, 本段只看本条回复 */
    u2str(cmd); u2put('\r'); u2put('\n');
    pump(ms);
}

/* 在环形缓冲区里找特征字节串(按总计数下标, 取模访问) */
static long find_pat(uint32_t from, const uint8_t *pat, uint32_t plen)
{
    uint32_t i, j, h = rx_head;
    if (h < plen || from > h - plen) return -1;
    for (i = from; i + plen <= h; i++)
    {
        for (j = 0; j < plen; j++)
            if (rx_buf[(i + j) % RXCAP] != pat[j]) break;
        if (j == plen) return (long)i;
    }
    return -1;
}

static const uint8_t PAT_OK[2] = { 'O', 'K' };

static void wait_ready(void)
{
    uint32_t i, base;
    for (i = 0; i < 10; i++)
    {
        base = rx_head;
        at_cmd("AT", 1200);
        if (find_pat(base, PAT_OK, 2) >= 0)
        {
            u1str("[+] module ready (got OK)\r\n");
            return;
        }
        u1str("[..] no OK yet, retry\r\n");
    }
    u1str("[!] module never answered OK - check power / TX-RX wiring\r\n");
}

/* ================= OLED 软件 I2C(PB8=SCL, PB9=SDA) HAL 版 =================
   与 ch5_6_dht11_light_hal.c 同款(已烧录验证) */
static void i2c_dly(void){ delay_us(3); }
static void scl_hi(void){ HAL_GPIO_WritePin(GPIOB,GPIO_PIN_8,GPIO_PIN_SET); }
static void scl_lo(void){ HAL_GPIO_WritePin(GPIOB,GPIO_PIN_8,GPIO_PIN_RESET); }
static void sda_lo(void)
{
  GPIO_InitTypeDef g = {0};
  g.Pin=GPIO_PIN_9; g.Mode=GPIO_MODE_OUTPUT_PP; g.Speed=GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB,&g);
  HAL_GPIO_WritePin(GPIOB,GPIO_PIN_9,GPIO_PIN_RESET);
}
static void sda_rel(void)
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

/* ================= DHT11 单总线(PB14, 带5ms超时, 永不卡死) HAL 版 ================= */
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
/* 返回 0=成功 1=超时 2=校验错 */
static uint8_t dht11_read(uint8_t d[5])
{
  uint8_t i, j;
  for(i=0;i<5;i++) d[i]=0;
  dht_out_low(); delay_ms(20); dht_rel();
  if(wait_line(0)) return 1;
  if(wait_line(1)) return 1;
  if(wait_line(0)) return 1;
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

/* ================= MQTT 报文拼装 ================= */
static uint8_t body[420];
static uint8_t mq[560];

static uint32_t put_str(uint8_t *p, const char *s)
{
    uint32_t n = 0, i;
    while (s[n]) n++;
    p[0] = (uint8_t)(n >> 8);
    p[1] = (uint8_t)(n & 0xFF);
    for (i = 0; i < n; i++) p[2 + i] = (uint8_t)s[i];
    return 2 + n;
}

static uint32_t pack(uint8_t type, uint32_t blen)
{
    uint32_t rl = blen, i = 1, k;
    mq[0] = type;
    do {
        uint8_t d = (uint8_t)(rl & 0x7F);
        rl >>= 7;
        if (rl) d |= 0x80;
        mq[i++] = d;
    } while (rl);
    for (k = 0; k < blen; k++) mq[i + k] = body[k];
    return i + blen;
}

static uint32_t build_connect(void)
{
    uint32_t b = 0;
    b += put_str(body + b, "MQTT");
    body[b++] = 0x04;
    body[b++] = 0xC2;
    body[b++] = (uint8_t)(KEEPALIVE >> 8);
    body[b++] = (uint8_t)(KEEPALIVE & 0xFF);
    b += put_str(body + b, MQTT_CLIENT);
    b += put_str(body + b, MQTT_USER);
    b += put_str(body + b, MQTT_PASS);
    return pack(0x10, b);
}

static uint32_t build_subscribe(void)
{
    uint32_t b = 0;
    body[b++] = 0x00; body[b++] = 0x01;
    b += put_str(body + b, TOPIC_SUB);
    body[b++] = 0x00;
    return pack(0x82, b);
}

/* ---- 动态 payload: 用真实传感器值拼 OneJSON ---- */
static char    payload[200];
static uint32_t plen = 0;
static uint32_t msg_id = 0;

static void p_add(const char *s){ while (*s) payload[plen++] = *s++; }
static void p_u8(uint8_t v)
{
    if (v >= 100) payload[plen++] = (char)('0' + v / 100);
    if (v >= 10)  payload[plen++] = (char)('0' + v / 10 % 10);
    payload[plen++] = (char)('0' + v % 10);
}

static uint32_t build_publish(uint8_t t_i, uint8_t t_f, uint8_t h_i, uint8_t h_f, uint8_t light)
{
    uint32_t b = 0, i;
    plen = 0;
    msg_id++;
    p_add("{\"id\":\"");      p_u8((uint8_t)msg_id);
    p_add("\",\"version\":\"1.0\",\"params\":{");
    p_add("\"temperature\":{\"value\":");  p_u8(t_i); payload[plen++]='.'; p_u8(t_f); p_add("},");
    p_add("\"humidity\":{\"value\":");     p_u8(h_i); payload[plen++]='.'; p_u8(h_f); p_add("},");
    p_add("\"light\":{\"value\":");        p_u8(light); p_add("}}}");
    payload[plen] = 0;
    /* PUBLISH 报文体 = 2字节topic长度+topic+payload, 必须重新装进 body[]! */
    b += put_str(body + b, TOPIC_POST);
    for (i = 0; i < plen; i++) body[b++] = (uint8_t)payload[i];
    return pack(0x30, b);
}

/* 发一包 MQTT 报文: 等 '>' 5s -> 发字节 -> 等 SEND OK 4s 才算成功
   返回 1=发出去了 0=失败(调用方应标记网络掉线并重连) */
static uint8_t send_packet(const char *name, uint32_t n, uint32_t wait_ms)
{
    char cmd[32];
    uint32_t i, k;

    u1str("\r\n----- "); u1str(name); u1str(" : "); u1num(n);
    u1str(" bytes -----\r\n");

    cmd[0] = 'A'; cmd[1] = 'T'; cmd[2] = '+'; cmd[3] = 'C'; cmd[4] = 'I';
    cmd[5] = 'P'; cmd[6] = 'S'; cmd[7] = 'E'; cmd[8] = 'N'; cmd[9] = 'D';
    cmd[10] = '='; i = 11;
    if (n >= 100) cmd[i++] = (char)('0' + n / 100);
    if (n >= 10)  cmd[i++] = (char)('0' + (n / 10) % 10);
    cmd[i++] = (char)('0' + n % 10);
    cmd[i] = 0;

    saw_prompt = 0;
    u2str(cmd); u2put('\r'); u2put('\n');

    for (k = 0; k < 5000; k++)
    {
        flush_rx();
        if (saw_prompt) break;
        delay_ms(1);
    }
    if (!saw_prompt)
    {
        u1str("[!] no '>' prompt, skip this packet\r\n");
        pump(1500);
        return 0;
    }
    for (i = 0; i < n; i++) u2put((char)mq[i]);

    {   /* 必须等到 SEND OK 才算模块真发出去了 */
        static const uint8_t PAT_SENDOK[7] = { 'S','E','N','D',' ','O','K' };
        uint32_t base = rx_head;
        uint8_t ok = 0, kk;
        for (kk = 0; kk < 40; kk++)
        {
            flush_rx();
            if (find_pat(base, PAT_SENDOK, 7) >= 0) { ok = 1; break; }
            delay_ms(100);
        }
        u1str(ok ? "[ok] SEND OK confirmed\r\n" : "[!] no SEND OK!\r\n");
    }
    pump(wait_ms);
    return 1;
}

/* ---- TCP + MQTT CONNECT + SUBSCRIBE(带重试, 治"复位太快被服务器拒之门外") ----
   实测: 复位后立刻重连, OneNET 上旧会话还没超时(keepalive 1.5x), 新 CONNECT
   会被回 rc=5 拒掉。所以失败就等 30 秒再试, 最多 4 次。 */
static uint8_t net_ok = 0;

static uint8_t net_connect(void)
{
    long pos;
    uint32_t n;
    static const uint8_t PAT_CONNECTED[7] = { 'C','O','N','N','E','C','T' };
    static const uint8_t PAT_CONNACK[2] = { 0x20, 0x02 };
    static const uint8_t PAT_SUBACK[2]  = { 0x90, 0x03 };

    at_cmd("AT+CIPSTART=\"TCP\",\"" MQTT_HOST "\"," MQTT_PORT, 6000);
    pos = find_pat(0, PAT_CONNECTED, 7);
    if (pos < 0) { u1str("[!] TCP fail\r\n"); return 0; }
    u1str("\r\n[+] TCP CONNECTED\r\n");

    n = build_connect();
    pos = (long)rx_head;
    send_packet("MQTT CONNECT", n, 5000);
    pos = find_pat((uint32_t)pos, PAT_CONNACK, 2);
    if (pos < 0) { u1str("[!] no CONNACK\r\n"); return 0; }
    u1str("[+] CONNACK rc=");
    u1num(rx_buf[(uint32_t)(pos + 3) % RXCAP]);
    u1str("\r\n");
    if (rx_buf[(uint32_t)(pos + 3) % RXCAP] != 0) return 0;
    o_print(0, 108, "NET");            /* 屏幕右上角亮 NET = 云已连上 */

    n = build_subscribe();
    pos = (long)rx_head;
    send_packet("MQTT SUBSCRIBE", n, 3000);
    pos = find_pat((uint32_t)pos, PAT_SUBACK, 2);
    u1str(pos >= 0 ? "\r\n[+] SUBACK ok\r\n" : "\r\n[!] no SUBACK\r\n");
    return 1;
}

/* ================= 全局传感器值(云/OLED/串口三处共用同一份) ================= */
static uint8_t g_light   = 0;
static uint8_t g_t_i = 0, g_t_f = 0, g_h_i = 0, g_h_f = 0;
static uint8_t g_dht_ok = 0;          /* 拿到过一次成功读数才允许上云 */
static uint32_t g_ok_cnt = 0;

int main(void)
{
    GPIO_InitTypeDef g = {0};
    uint32_t n;
    uint8_t  d[5], err, lastv = 0xFF;
    uint8_t  disp_h = 0xFF, disp_t = 0xFF;
    uint32_t cycle = 0;

    RES[0] = 0x22222801;
    HAL_Init();
    SystemClock_Config();
    dwt_init();
    uart1_init();

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_USART2_CLK_ENABLE();

    /* USART2 PA2/PA3 -> ESP8266 */
    g.Pin = GPIO_PIN_2; g.Mode = GPIO_MODE_AF_PP; g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &g);
    g.Pin = GPIO_PIN_3; g.Mode = GPIO_MODE_INPUT; g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &g);
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart2);
    HAL_NVIC_SetPriority(USART2_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
    HAL_UART_Receive_IT(&huart2, (uint8_t *)&rx_byte, 1);   /* 挂上第一字节接收 */

    /* PB12 光敏 DO: 上拉输入(开漏输出必须上拉!) */
    g.Pin = GPIO_PIN_12; g.Mode = GPIO_MODE_INPUT; g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &g);
    /* PC13 心跳灯 */
    g.Pin = GPIO_PIN_13; g.Mode = GPIO_MODE_OUTPUT_PP; g.Pull = GPIO_NOPULL; g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &g);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    /* PB8/PB9 OLED: SCL 推挽输出 + SDA 释放(上拉输入) */
    g.Pin = GPIO_PIN_8; g.Mode = GPIO_MODE_OUTPUT_PP; g.Pull = GPIO_NOPULL; g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &g);
    scl_hi();
    sda_rel();

    /* OLED 先亮起来(不等网络) */
    delay_ms(100);
    o_init();
    { uint8_t p; for(p=0;p<8;p++) o_clear(p); }
    o_print(0, 0, "F103 SMART ENV");
    o_print(2, 0, "LIGHT:");
    o_print(4, 0, "HUMI:");
    o_print(6, 0, "TEMP:");

    u1str("\r\n\r\n########## TASK9 HAL FINAL: sensors+OLED+MQTT ##########\r\n");
    delay_ms(300);
    flush_rx();
    rx_tail = rx_head;                     /* 丢掉开机垃圾 */

    /* ---- 第一步: 连 WiFi ---- */
    at_cmd("AT+RST",               5000);
    wait_ready();
    at_cmd("AT+CWAUTOCONN=0",      1500);
    at_cmd("AT+CWMODE=1",          1500);
    at_cmd("AT+CIPMUX=0",          1500);
    at_cmd("AT+CIPMODE=0",         1500);
    at_cmd("AT+CWJAP=\"" WIFI_SSID "\",\"" WIFI_PWD "\"", 13000);
    at_cmd("AT+CIFSR",             2500);

    /* ---- 第二步: TCP + MQTT 连云(带重试) ---- */
    {
        uint8_t tries;
        for (tries = 0; tries < 4 && !net_ok; tries++)
        {
            net_ok = net_connect();
            if (!net_ok) { u1str("[..] retry in 30s (old session may be alive)\r\n"); pump(30000); }
        }
    }

    RES[0] = 0x22222802;

    /* ================= 主循环: 传感 -> OLED -> 串口 -> 云端 ================= */
    while (1)
    {
        /* 1) 读光敏 */
        g_light = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_12) == GPIO_PIN_SET) ? 1 : 0;
        if (g_light != lastv)
        {
            lastv = g_light;
            o_print(2, 48, g_light ? "OK  " : "DARK");
        }

        /* 2) 读 DHT11 */
        err = dht11_read(d);
        if (err == 0)
        {
            g_t_i = d[2]; g_t_f = d[3]; g_h_i = d[0]; g_h_f = d[1];
            g_dht_ok = 1; g_ok_cnt++;
            if (d[0] != disp_h)
            {
                disp_h = d[0];
                o_print(4, 40, "      ");
                { char b[8];
                  b[0]=(char)('0'+d[0]/100); b[1]=(char)('0'+d[0]/10%10); b[2]=(char)('0'+d[0]%10);
                  b[3]='.'; b[4]=(char)('0'+d[1]); b[5]='%'; b[6]=0;
                  o_print(4, 40, b); }
            }
            if (d[2] != disp_t)
            {
                disp_t = d[2];
                o_print(6, 40, "      ");
                { char b[8];
                  b[0]=(char)('0'+d[2]/100); b[1]=(char)('0'+d[2]/10%10); b[2]=(char)('0'+d[2]%10);
                  b[3]='.'; b[4]=(char)('0'+d[3]); b[5]='C'; b[6]=0;
                  o_print(6, 40, b); }
            }
        }
        else
        {
            o_print(4, 40, " --  ");
            o_print(6, 40, " --  ");
            disp_h = 0xFF; disp_t = 0xFF;
            u1str("[..] DHT11 ");
            u1str(err == 1 ? "TIMEOUT\r\n" : "CHECKSUM ERR\r\n");
        }

        /* 3) 串口打印(和屏幕同一份值) */
        u1str("LIGHT="); u1num(g_light);
        if (err == 0)
        {
            u1str("  HUMI="); u1num(g_h_i); u1ch('.'); u1num(g_h_f);
            u1str("%  TEMP="); u1num(g_t_i); u1ch('.'); u1num(g_t_f); u1str("C\r\n");
        }
        else u1str("\r\n");

        /* 4) 上云(每3轮一次; 掉线自动重连; 必须拿到过真实读数) */
        cycle++;
        if (!net_ok)
        {
            u1str("[..] net down, reconnect...\r\n");
            net_ok = net_connect();      /* 失败下一轮再试, 不卡死 */
        }
        else if (g_dht_ok && (cycle % 3u) == 0u)
        {
            n = build_publish(g_t_i, g_t_f, g_h_i, g_h_f, g_light);
            u1str("publish: "); u1str(payload); u1str("\r\n");
            if (!send_packet("PUBLISH", n, 4000)) net_ok = 0;
        }
        else pump(800);

        /* 5) 心跳 */
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);

        RES[1] = rx_head; RES[2] = ore_n; RES[3] = g_ok_cnt;
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
