#  HAL教程 · STM32F103C8T6 精英板

> 本章目标：搞清这个 HAL 工程从哪来、改了什么、怎么编译烧录；理解 HAL 工程的标准分层。
> 步骤照做可复现；知识点解释为什么。


## 📋 任务对照表（对应任务清单，每章代码在 User/chapters/ 可独立烧录）

| 任务 | 内容 | 对应章节 | 验收状态 | 证据 |
|---|---|---|---|---|
| 任务一 | LED 1s 闪烁 | 02 章 | ✅ 已验收 | 板载绿灯实测 + SWD 读 ODR |
| 任务二 | 串口正常收发数据 | 04 章 | ✅ 已验收 | 串口横幅+回显实拍记录 |
| 任务三 | 按键控制 LED 闪烁 | 03 章 | ✅ 已验收 | 用户实测+串口状态打印 |
| 任务四 | 按键控蜂鸣器/继电器 | 05 章 | ✅ 已验收 | 用户听到蜂鸣/继电器"咚"声 |
| 任务五 | 串口打印温湿度 | 06 章 | 🔶 代码完成，DHT11 模块待更换 | 边沿诊断记录 |
| 任务六 | 串口打印烟雾数据 | 07 章（光敏替身） | ✅ 已验收 | 串口 DO 打印+OLED 联动 |
| 扩展 | OLED 显示 | 08 章 | ✅ 已验收 | 用户确认屏幕三行字+实时联动 |
---

## 开发板与硬件事实

开发板：**STM32F103C8T6 精英板（嘉立创 v1.3）**。引脚事实（来自原理图，人工逐区核对）：

| 外设 | 引脚 | 电气事实 |
|---|---|---|
| LED1（绿）/ LED2（红） | PC14 / PC13 | 低电平点亮（3.3V→LED→4.7K→引脚） |
| 蜂鸣器 | PC15 | NPN 驱动，高电平响 |
| 继电器 | PC14 | 与 LED1 共用，高电平吸合 |
| 按键 KEY1/2/3 | PB7 / PB6 / PB5 | 按下接地，需内部上拉 |
| 日志串口 | PA9 / PA10 | USART1 ← CH340N ← USB TypeC |
| OLED | PB8 / PB9 | SSD1306 软件模拟 I2C |
| Flash | PB12~PB15 | W25Q128 @ SPI2 |
| ESP8266 | PA2 / PA3 | USART2 |

> 原理图/手册在本地 `docs/schematics/`、`docs/datasheets/`（版权原因不入仓库）。

---

## 这个工程从哪来（重要：来源与改动清单）

**来源**：ST 官方固件包 **STM32Cube_FW_F1_V1.8.6**（本机 `C:\Users\26476\STM32Cube\Repository\`，由 STM32CubeMX 安装）中的官方模板 `Projects\STM32F103RB-Nucleo\Templates`。

**为什么不用 CubeMX 现场生成？** CubeMX 图形化生成当然也可以（后续章节配外设时我们会按"CubeMX 配置清单"方式操作），但脚手架追求"每一行都有出处、可人工核对"——官方模板是 ST 验证过的最小 HAL 工程，从它裁剪，所有文件都保持官方原样，只做下表 4 处**机械改动**：

| # | 改动 | 原因 |
|---|---|---|
| 1 | Device `STM32F103RB` → `STM32F103C8`，Flash 范围 `0x801FFFF`(128K) → `0x800FFFF`(64K) | 板上芯片是 C8T6（64KB Flash / 20KB RAM） |
| 2 | 宏去掉 `USE_STM32F1xx_NUCLEO`；`Inc/main.h` 删除 `#include "stm32f1xx_nucleo.h"`；工程删除 Nucleo BSP/Adafruit/st7735 文件 | 我们不是 Nucleo 板，BSP 是那块板的板级支持 |
| 3 | HAL/CMSIS 源码从"引用固件包相对路径"改为**复制进工程** `Drivers/` | 工程自包含：push 到 GitHub 后任何机器直接能编译，不依赖本机固件包位置 |
| 4 | 删除 HAL 包内 `.chm` 用户手册、CMSIS 模板目录 | 手册不入仓库（与版权要求一致），体积 57MB→17MB |

**保留的官方原样**：`startup_stm32f103xb.s`、`system_stm32f1xx.c`、`stm32f1xx_hal_conf.h`、`main.c` 的 `SystemClock_Config`（HSE 8MHz→PLL×9→72MHz，与本板 8MHz 晶振一致）。

---

## 软件安装

与标准库仓库完全一致，不重复展开，只列清单：

1. **Keil MDK5 + AC5 编译器**（HAL 工程同样含 ARM 语法的官方启动文件，AC5 必须）；
2. **Keil::STM32F1xx_DFP 2.3.0 芯片包**（Keil 认识 STM32F103C8 的前提，装法见标准库仓库 01 章 3.2）；
3. **ST-Link 驱动 + STM32CubeProgrammer**（烧录）；
4. CH340 驱动（串口章节用）。

**知识点：HAL 和 SPL 的"身份宏"不是一套体系**

| | 标准库仓库 | 本仓库（HAL） |
|---|---|---|
| 芯片宏 | `STM32F10X_MD`（按"密度"分类） | `STM32F103xB`（按"型号族"分类） |
| 总开关 | `USE_STDPERIPH_DRIVER` | `USE_HAL_DRIVER` |
| 总头文件 | `stm32f10x.h` | `stm32f1xx.h` → 内部引 `stm32f103xb.h` |
| 配置头 | `stm32f10x_conf.h`（include 开关） | `stm32f1xx_hal_conf.h`（include 开关 + 各模块参数） |

看到工程 Define 里 `USE_HAL_DRIVER,STM32F103xB` 就知道这是 HAL 工程；`stm32f1xx_hal_conf.h` 是 HAL 的"外设开关板 + 参数表"，结构同 SPL 的 conf.h 思想一致。

---

## 编译

打开 `MDK-ARM/Project.uvprojx`，F7。预期：

```
Program Size: Code=1838 RO-data=302 RW-data=16 ZI-data=1024
"f103-hal-lab\f103-hal-lab.axf" - 0 Error(s), 0 Warning(s).
```

**知识点：为什么 HAL 脚手架比 SPL 脚手架"大"却 Code 更大？** 两个脚手架都是空主循环，但 HAL 工程把**全量 HAL .c 都加入工程**（官方模板风格）。链接器只把实际用到的函数链进镜像（所以 Code=1838B 并不大），但编译时间明显变长（要编 40+ 个文件）。SPL 仓库是"按需添加"风格。这是两种官方风格，各有取舍。

---

## 烧录

```
STM32_Programmer_CLI -c port=SWD -w MDK-ARM\f103-hal-lab\f103-hal-lab.hex -v -rst
```

脚手架空主循环无现象，链路打通即可。注意 02 章开始才产生"1 秒闪一次"的可观察现象。

---

## HAL 工程标准分层（每个文件的角色）

```
f103-hal-lab/
├─ Inc/
│  ├─ main.h                    # 业务对外头
│  ├─ stm32f1xx_hal_conf.h      # HAL 总配置：模块开关 + HSE 值 + SysTick 频率等
│  └─ stm32f1xx_it.h
├─ Src/
│  ├─ main.c                    # HAL_Init() → SystemClock_Config() → 空循环
│  ├─ stm32f1xx_it.c            # 全部中断服务函数
│  ├─ stm32f1xx_hal_msp.c       # ★ MSP 层：外设底层初始化（时钟使能/引脚/中断优先级）
│  └─ system_stm32f1xx.c        # SystemInit()
├─ Drivers/
│  ├─ CMSIS/Include/            # ARM 内核头（core_cm3.h 等）
│  ├─ CMSIS/Device/ST/STM32F1xx/Include/  # stm32f1xx.h / stm32f103xb.h（寄存器定义）
│  └─ STM32F1xx_HAL_Driver/     # HAL 全量驱动
└─ MDK-ARM/startup_stm32f103xb.s  # 启动文件（xb = 128K 中密度，与 C8T6 同族）
```

**知识点：HAL 把"外设配置"拆成两层**
- `MX_xxx_Init()`（main.c 里）＝外设"逻辑参数"（波特率、模式……）；
- `HAL_xxx_MspInit()`（hal_msp.c 里）＝该外设的"物理连接"（开哪个时钟、哪个引脚、中断优先级）。
这种拆分让"换一块板子只改 MSP"成为可能——02 章点灯时你会同时看到这两层。

**知识点：启动文件名为什么是 `startup_stm32f103xb.s`？** HAL 体系按型号族给启动文件（xl/xb/md/…）。C8T6 与 RB 同属 `xb`（128K 及以下中密度），向量表完全一致，所以官方模板的启动文件原封不动可用。

---

## 上电到 main()（与 SPL 版对比）

```
复位 → startup_stm32f103xb.s: Reset_Handler
      → SystemInit()（system_stm32f1xx.c）
      → __main → main()
main() 内:
      HAL_Init()            初始化 HAL 库、SysTick=1ms、NVIC 分组
      SystemClock_Config()  HSE 8M → PLL×9 → SYSCLK 72MHz（模板自带，逐行讲解在 02 章）
      while(1) {}
```

对比 SPL 脚手架：HAL 在 main 里多做了 `HAL_Init + SystemClock_Config` 两步显式初始化；SPL 则把时钟留给用户按需配。这正是两库设计哲学差异的第一个体现。

---

## Git 提交规矩

同标准库仓库：每章一 commit，格式 `章节: 内容 (HAL)`。

## 常见问题

| 现象 | 原因 | 解决 |
|---|---|---|
| `stm32f1xx_nucleo.h not found` | 用了未裁剪的原模板 | 用本仓库已裁剪版本 |
| 编译慢 | HAL 全量源码在编译 | 正常现象（~20s），镜像不含未用代码 |
| 其他 | 同标准库仓库 01 章第 9 节 | — |

## 下一步

02-GPIO输出：用 `HAL_GPIO_WritePin` 点亮 PC14，`HAL_Delay` 实现 1s 闪烁，并讲清 GPIO 推挽输出与 HAL_Delay 背后的 SysTick。

---

# 任务一 · 点亮 LED 灯

> 同一个灯（PC13 绿灯），换 HAL 库实现。时钟 64MHz（模板默认 HSI），delay 用 HAL_Delay。

## 硬件事实（实测，与标准库版一致）

| 目标 | 引脚 | 依据 |
|---|---|---|
| 绿灯 | **PC13**（低电平亮） | 【实测】丝印 PC13 |
| 继电器 | PC14（想听咔哒就加它） | 【实测】 |
| PWR 红灯 | 常亮，不受控 | 【实测】丝印 PWR |

## 预检时钟：为什么我"不动"官方模板的时钟配置

**在哪看**：`Src/main.c` 的 `SystemClock_Config()`。
**看什么**：官方模板配的是 **内部 HSI → 64MHz**（不是外部晶振 72M）。
**为什么不改**：HSI 在芯片内部，永远起振、绝对可靠；HAL_Init/HAL_Delay 的 1ms 心跳由 HAL_RCC_ClockConfig 自动校准到实际频率，64M 还是 72M 延时都准。切 HSE 72M 留给时钟章节练手。
**我看到的**：模板 main() 里有一行官方预留注释 `/* Add your application code here */`——**代码就加在这里，这是 ST 规定的位置**。

## 写代码（改 `Src/main.c`）

main() 最终长这样（新增部分带注释）：

```c
int main(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};   /* 声明放函数开头（C90 规矩） */

  HAL_Init();                /* HAL 库初始化：SysTick 1ms 心跳、NVIC 分组 */
  SystemClock_Config();      /* 时钟 -> 64MHz（模板自带） */

  /* ==== 官方预留插入点：应用代码从这里开始 ==== */
  __HAL_RCC_GPIOC_CLK_ENABLE();                          /* 开 GPIOC 时钟 */

  GPIO_InitStruct.Pin   = GPIO_PIN_13;                   /* PC13（绿灯） */
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;           /* 推挽输出 */
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;           /* 低速够用 */
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);                /* 应用配置 */

  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);   /* 上电先灭 */

  while (1)
  {
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);  /* 低 = 亮 */
    HAL_Delay(1000);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);    /* 高 = 灭 */
    HAL_Delay(1000);
  }
}
```

#### 深挖 · HAL_Delay(5000) 内部：中断派数秒（对照标准库的轮询派）

你写的 `HAL_Delay(5000)` 背后发生了什么：

1. `HAL_Init()`（main 开头就调了）内部调 `HAL_InitTick()`，把 SysTick 配成 **每 1ms 触发一次 SysTick_Handler 中断**（同是那个内核倒计时秒表，配法不同：到点不置标志而是跳中断）；
2. 每次中断里执行 `uwTick++`——一个全局变量，每毫秒加 1，本质是"开机以来的毫秒数"；
3. `HAL_Delay(5000)` 的内部：记下当前的 uwTick，然后死等 `uwTick - start >= 5000` 才返回。

**和标准库/寄存器版（轮询派）的对比**：

| | 轮询派（标准库/寄存器） | 中断派（HAL） |
|---|---|---|
| 原理 | 死盯 SysTick 到点标志 | 到点进中断，uwTick++ |
| 代码 | 自己写 delay_ms | 库自带 HAL_Delay |
| 精度 | 好 | 好（1ms 心跳） |
| 限制 | 死等占着 CPU | **在中断里调用会卡死**（中断自己占着自己，uwTick 不走） |

**HAL_Delay 为什么能精确**：`SystemClock_Config()` 配完时钟后 HAL 会把 SysTick 重载值更新为"实际主频/1000"——64MHz 主频下 = 64000，一样是 1ms 心跳。改频率不影响延时的准确性（库自己会换算）。

## 编译

**在哪做**：`MDK-ARM` 目录（工程文件叫 **Project.uvprojx**，注意不是 f103-hal-lab.uvprojx）。

```
UV4.exe -b Project.uvprojx -j0 -o build.log
```

**实测日志**：

```
Program Size: Code=2476 RO-data=304 RW-data=16 ZI-data=1024
"f103-hal-lab103-hal-lab.axf" - 0 Error(s), 0 Warning(s).
Build Time Elapsed:  00:00:18
```

**两个注意**：①HAL 全量源码参与编译，比标准库版慢（18 秒 vs 1 秒），但没用的函数不会链进镜像；②产物在 `MDK-ARM103-hal-lab103-hal-lab.axf`，**官方模板没开 hex 选项**——不影响，CubeProgrammer 直接支持 .axf（ELF 格式，还带调试符号）。

## 烧录

```
STM32_Programmer_CLI -c port=SWD -w MDK-ARM103-hal-lab103-hal-lab.axf -v -rst
```

**实测日志**：

```
Erasing internal memory sectors [0 2]
Time elapsed during download operation: 00:00:00.253
Download verified successfully
MCU Reset
```

## 验收

**现象**：绿灯 1 秒闪——和标准库版现象完全一样，芯片里跑的却是 HAL 程序。

## 标准库 vs HAL 对照表（本章精华）

| 动作 | 标准库写法 | HAL 写法 |
|---|---|---|
| 开 GPIOC 时钟 | `RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE)` | `__HAL_RCC_GPIOC_CLK_ENABLE()` |
| 引脚 | `GPIO_Pin_13` | `GPIO_PIN_13` |
| 推挽输出 | `GPIO_Mode_Out_PP` | `GPIO_MODE_OUTPUT_PP` |
| 应用配置 | `GPIO_Init(GPIOC, &gpio)` | `HAL_GPIO_Init(GPIOC, &gpio)` |
| 写引脚 | `GPIO_WriteBit(GPIOC, GPIO_Pin_13, Bit_RESET)` | `HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET)` |
| 毫秒延时 | 自己写 SysTick delay_ms | `HAL_Delay(1000)`（HAL_Init 已配 1ms 心跳） |
| 结构体成员 | Pin/Mode/Speed | Pin/Mode/Pull/Speed |
| 时钟 | HSE 8M×9=72MHz（SystemInit 自动配） | HSI 64MHz（模板 SystemClock_Config） |
| 编译产物 | .hex | .axf（ELF） |

**一句话总结**：HAL 把"开时钟"藏进了宏、把"延时"做成了库函数、把配置拆成"逻辑参数(main)+物理连接(MSP)"两层——写起来更省心，代价是离寄存器更远。寄存器版会让你看到所有这些背后是什么。

## 翻车档案

| # | 现象 | 原因 | 解法 |
|---|---|---|---|
| 1 | 编译产物里没有 .hex | 官方模板没勾 Create HEX File | 直接烧 .axf，或 Options→Output 勾 Create HEX File |
| 2 | 工程文件名 | 官方模板叫 Project.uvprojx，别找 f103-hal-lab.uvprojx | 认准 MDK-ARM/Project.uvprojx |
| 3 | CubeProgrammer 报 `invalid ELF file`，报错路径被截成 `Desktop//HAL/...` | 命令行参数里的**中文路径**（姚凯锐任务/HAL库）传参时被编码吃掉 | `cd` 进 .axf 所在目录，用**相对路径** `-w f103-hal-lab/f103-hal-lab.axf`【实测 2026-10-05】 |

---
---

# 任务二 · 串口收发

### 硬件事实（三腿一致）

| 项 | 值 | 来源 |
|---|---|---|
| 串口 | USART1：**PA9=TX、PA10=RX**，115200-8-N-1 | 原理图 P2 + 实测 |
| 接线（外接 USB-TTL 时） | 模块 RX→板 A9、模块 TX→板 A10、GND→GND | **TX/RX 交叉**，【实测】双向通 |
| 本腿时钟 | **APB2 = 64MHz**（HSI/2×16，APB2=HCLK，官方模板原样未动） | 步骤 6 的决定 |

### 时钟与引脚：HAL 写法

```c
__HAL_RCC_GPIOA_CLK_ENABLE();
__HAL_RCC_USART1_CLK_ENABLE();          /* USART1 在 APB2，64MHz */

GPIO_InitStruct.Pin   = GPIO_PIN_9;
GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;      /* 复用推挽：引脚控制权交给 USART */
GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

GPIO_InitStruct.Pin  = GPIO_PIN_10;
GPIO_InitStruct.Mode = GPIO_MODE_INPUT;       /* 方向显式拆出来 */
GPIO_InitStruct.Pull = GPIO_NOPULL;           /* 浮空输入，电平听对方的 */
HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
```

和标准库对比：`GPIO_Mode_AF_PP` ↔ `GPIO_MODE_AF_PP`——**名字几乎一样，背后是同一个 CRH 格子**（PA9 → CRH[7:4]=0b1011）。HAL 换了个大写拼写，本质没变。

### 句柄 + HAL_UART_Init：BRR 它替你算

HAL 的思路：所有参数填进一个**句柄**结构体，一次交给库：

```c
static UART_HandleTypeDef huart1;

huart1.Instance          = USART1;            /* 哪个串口 */
huart1.Init.BaudRate     = 115200;            /* 要多少波特率 */
huart1.Init.WordLength   = UART_WORDLENGTH_8B;
huart1.Init.StopBits     = UART_STOPBITS_1;
huart1.Init.Parity       = UART_PARITY_NONE;
huart1.Init.Mode         = UART_MODE_TX_RX;
huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
huart1.Init.OverSampling = UART_OVERSAMPLING_16;
HAL_UART_Init(&huart1);                       /* 库内部：算 BRR + 配 CR1/CR2 + 使能 */
```

#### 深挖 · HAL 替你算的 BRR：同一参数，另一个答案

步骤 21（标准库章）手算过：72MHz 时 BRR=0x271。本腿 APB2 是 **64MHz**（HSI 路线），同一公式换个输入：

```
所需分频 = 64,000,000 / 16 / 115,200 = 34.7222...
整数部分 34 = 0x22 → BRR[15:4]
小数部分 0.7222 × 16 = 11.55 → HAL 四舍五入取 12 = 0xC → BRR[3:0]
BRR = 0x22C = 0b0010 0010 1100
              └ 34 ┘ └ 12 ┘
```

验算：实际波特率 = 64e6 / (16 × (34 + 12/16)) = **115207.4**，误差 **+0.006%**——UART 容差 ±2~3%，纹丝不动 ✓。

> **本节最大的知识点**：波特率分频值是"相对你的总线时钟"算的。同一个 115200，72MHz 时写 0x271，64MHz 时写 0x22C。HAL 靠 `HAL_RCC_GetPCLK2Value()` 运行时取时钟自动适配；寄存器腿就得自己算。

### 收发：HAL 的阻塞 API

```c
uint8_t banner[] = "=== STM32F103 USART1 READY - HAL (115200-8-N-1) ===
...";
HAL_UART_Transmit(&huart1, banner, sizeof(banner) - 1, HAL_MAX_DELAY);   /* 整块发出 */

while (1)
{
  if (HAL_UART_Receive(&huart1, &b, 1, HAL_MAX_DELAY) == HAL_OK)  /* 收 1 字节 */
  {
    HAL_UART_Transmit(&huart1, &b, 1, HAL_MAX_DELAY);   /* 原样发回 */
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);             /* LED 反馈 */
    g_rx_count++;
  }
}
```

#### 深挖 · 这两个 API 的壳里是什么？

`HAL_UART_Transmit` 内部：逐字节 `等 TXE → 写 DR`——和我们标准库手写的 `uart1_send` 一模一样，只是套了个**超时**参数（传 `HAL_MAX_DELAY`=0xFFFFFFFF 就是不限时）。`HAL_UART_Receive` 内部：`等 RXNE → 读 DR`。**HAL 没有魔法，只是把轮询循环替你写好、加上了超时保险。** 代价：`HAL_UART_Receive` 收不到就一直阻塞——回显程序里这恰好是我们要的行为。

### 编译烧录验证（实测）

```
UV4.exe -b Project.uvprojx -j0 -o build.log
→ Program Size: Code=3528 RO-data=428 RW-data=20 ZI-data=1100
→ 0 Error(s), 0 Warning(s)
STM32_Programmer_CLI -c port=SWD -w f103-hal-lab/f103-hal-lab.axf -v -rst   （模板无 hex，烧 axf）
→ Download verified successfully + MCU Reset
```

实测记录：

| 操作 | 现象 | 结论 |
|---|---|---|
| 复位上电 | 横幅出现（带 `- HAL` 标记） | ✅ 发送链路通 |
| 电脑发 `PING1234` | 原样回显 `PING1234` | ✅ 接收链路通 |
| 发送期间绿灯 | 每字节翻转一次 | ✅ 联动正常 |

> 体积对照：本章标准库 4160B vs HAL 3528B——**HAL 反而更小**。"寄存器<HAL<标准库"不是铁律，取决于谁带了更多自己的代码（本章标准库版多写了 str 工具函数）。

## 三腿对照表更新（串口版）

| 动作 | 标准库 | HAL | 寄存器（预告） |
|---|---|---|---|
| 使能时钟 | `RCC_APB2PeriphClockCmd` | `__HAL_RCC_USART1_CLK_ENABLE()` | `RCC->APB2ENR \|= 1<<14` |
| 配引脚 | `GPIO_Mode_AF_PP` | `GPIO_MODE_AF_PP` | `CRH[3:0]=0xB, CRH[7:4]=0x4` |
| 配串口 | `USART_Init` 结构体 | 填句柄 + `HAL_UART_Init` | 手写 `BRR=0x271` + `CR1` 三位 |
| 发 1 字节 | 写 DR 等 TXE | `HAL_UART_Transmit` | 写 DR 等 TXE（裸） |
| 收 1 字节 | 看 RXNE 读 DR | `HAL_UART_Receive` | 看 RXNE 读 DR（裸） |

# 任务三 · 按键控制 LED

> 与标准库版（f103-spl-lab 03 章）逻辑一字不差，全部函数换成 HAL 写法。新知识点：**HAL 把"上拉"从隐藏动作变成了显式字段**，以及 **HAL 自带翻转函数**。

## 硬件事实（实测，三腿一致）

| 目标 | 引脚 | 依据 |
|---|---|---|
| 按键 KEY1 | PB7（按下接 GND，低有效，无外部上拉） | 【文档】原理图 P3 |
| 绿灯 | PC13（低电平亮） | 【实测】02 章 |

## 配置：HAL 的"Pull 字段"比标准库诚实

标准库版（知识卡 B）里踩过一个隐形坑：`GPIO_Mode_IPU` 配置时，**上拉还是下拉由 ODR 位决定**，标准库在 `GPIO_Init` 内部偷偷帮你把 ODR 置 1——不看源码根本不知道。

HAL 把这个动作摆到了台面上，配置结构体里**多了一个标准库没有的字段**：

```c
GPIO_InitStruct.Pin  = GPIO_PIN_7;
GPIO_InitStruct.Mode = GPIO_MODE_INPUT;    /* 方向：输入（HAL 拆出来了！） */
GPIO_InitStruct.Pull = GPIO_PULLUP;        /* 上拉/下拉/浮空，显式三选一 */
HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
```

对照记忆：

| | 标准库 | HAL |
|---|---|---|
| 方向 | 折叠在 `GPIO_Mode`（IPU 里含"输入"） | 独立字段 `Mode = GPIO_MODE_INPUT` |
| 上拉/下拉 | 隐藏（IPU 内部置 ODR） | **显式字段 `Pull = GPIO_PULLUP`** |
| 推挽输出 | `GPIO_Mode_Out_PP` | `Mode = GPIO_MODE_OUTPUT_PP` + `Pull = GPIO_NOPULL` |

HAL 的 `GPIO_MODE_INPUT` / `GPIO_MODE_OUTPUT_PP` 把"方向"从 CNF/MODE 的折叠编码里解放出来——这是 HAL 比 SPL 好读的实锤之一。

## 读按键与翻转：HAL 的三个顺手函数

```c
HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7)      /* 读引脚：返回 GPIO_PIN_SET(1)/GPIO_PIN_RESET(0) */
HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);   /* 写引脚 */
HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);  /* ★ 翻转：HAL 自带，标准库没有！ */
```

- `HAL_GPIO_TogglePin` 内部就是 `ODR ^= pin`（读-反-写）——标准库版我们手写的三行，HAL 一个函数搞定；
- `HAL_GPIO_ReadPin` 返回的是枚举 `GPIO_PinState`，判断按下用 `== GPIO_PIN_RESET`（0=按下）。

## 完整逻辑（与标准库版同构，只贴关键）

```c
static uint8_t running = 0;     /* 开关 */
static uint16_t slice = 0;      /* 10ms 片计数 */

while (1)
{
  if (key_pressed())            /* ① 每片照看按键（static last 抓沿+消抖+等释放） */
  {
    running = !running;
    if (running == 0)
      HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);  /* 停止：灭 */
  }
  if (running)                  /* ② 非阻塞分片 */
  {
    if (++slice >= 50)          /* 50 片 x 10ms = 500ms */
    {
      slice = 0;
      HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);   /* 每 500ms 翻转 = 1s 周期 */
    }
  }
  HAL_Delay(10);                /* ③ 节拍器 */
}
```

`key_pressed()` 的消抖/等释放与标准库版逐行对应，只是 `delay_ms(10)`→`HAL_Delay(10)`、读引脚函数替换。**非阻塞分片的思想不变**——变的是工具。

## 编译烧录（实测）

```
UV4.exe -b Project.uvprojx -j0 -o build.log
→ Program Size: Code=4106 RO-data=340 RW-data=16 ZI-data=1168
→ 0 Error(s), 0 Warning(s), 15 秒
STM32_Programmer_CLI -c port=SWD -w f103-hal-lab103-hal-lab.axf -v -rst
→ Download verified successfully + MCU Reset
```

体积继续膨胀（标准库版 3124B → HAL 版 4462B）：`HAL_Delay/Toggle/Read/Write` 一套库函数比标准库的更重。

## 验收

| 操作 | 预期 | 实测 |
|---|---|---|
| 按一下 KEY1 | 绿灯 1 秒周期闪烁 | ✅ 用户确认（2026-10-05） |
| 再按一下 | 灯灭、停止 | ✅ 用户确认（2026-10-05） |
| 再按 | 恢复闪烁 | ✅ 用户确认（2026-10-05） |

## 三腿对照表更新（按键版）

| 动作 | 标准库 | HAL | 寄存器（预告） |
|---|---|---|---|
| PB7 上拉输入 | `GPIO_Mode_IPU`（上拉藏在 ODR） | `Mode=INPUT` + `Pull=GPIO_PULLUP`（显式） | `CRL[31:28]=0b1000` + `ODR\|=1<<7` |
| 读按键 | `GPIO_ReadInputDataBit` | `HAL_GPIO_ReadPin` | `GPIOB->IDR & (1<<7)` |
| 翻转 LED | 读-反-写三行 | **`HAL_GPIO_TogglePin` 一行** | `GPIOC->ODR ^= 1<<13` |
| 消抖延时 | 自己的 delay_ms | `HAL_Delay` | 自己的 delay_ms |

# 任务四 · 蜂鸣器和继电器

## 第一步 · 引脚从哪来

打开原理图（`STM32F103C8T6精英板原理图.pdf`）找到 "Buzzer Driver" 和 "Relay Driver" 两块电路：

| 外设 | 原理图引脚 | 电路怎么接的 | 有效电平 |
|---|---|---|---|
| 蜂鸣器 | **PC15** | 引脚 → 电阻 → NPN 三极管基极，三极管拉动蜂鸣器 | 引脚**高电平**→三极管导通→蜂鸣器响 |
| 继电器 | **PC14** | 引脚 → R14(10Ω) → NPN 基极；线圈并联 1N4148 续流二极管 | 原理图画的是高电平吸合，**但实测是低电平吸合**（见下） |

> ⚠️ **实测与原理图不一致**：按 NPN 拓扑推断"高电平吸合"，实物却是**低电平吸合**（能听到"咚"的一声）。**规则：实物优先**。代码里用宏封装，换板子只改一行：
> ```c
> #define RELAY_ON()  GPIO_WriteBit(GPIOC, GPIO_Pin_14, Bit_RESET)  /* 低电平吸合（实测） */
> ```

## 第二步 · 初始化：开时钟 + 配引脚

```c
/* ① 开时钟 */
__HAL_RCC_GPIOB_CLK_ENABLE();
__HAL_RCC_GPIOC_CLK_ENABLE();

/* ② 按键 PB7/PB6：上拉输入 */
GPIO_InitTypeDef g = {0};
g.Pin  = GPIO_PIN_7 | GPIO_PIN_6;
g.Mode = GPIO_MODE_INPUT;
g.Pull = GPIO_PULLUP;                            /* HAL 把上拉写成显式字段 */
HAL_GPIO_Init(GPIOB, &g);

/* ③ 蜂鸣器 PC15 + 继电器 PC14 + 心跳灯 PC13：推挽输出 */
g.Pin   = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
g.Mode  = GPIO_MODE_OUTPUT_PP;
g.Pull  = GPIO_NOPULL;
g.Speed = GPIO_SPEED_FREQ_LOW;
HAL_GPIO_Init(GPIOC, &g);

/* ④ 上电初态 */
HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
RELAY_OFF(); BUZZ_OFF();
```

**为什么三只脚能一次配**：`GPIO_Init` 的 `Pin` 是"掩码"，写 `Pin_13|Pin_14|Pin_15` 就等于一次配三个脚，它们的模式相同（都是推挽输出）。

## 第三步 · 用什么方法控制，为什么

**直接用 GPIO 高低电平**（不需要 I2C/SPI/串口）：

- 蜂鸣器/继电器都是**开关型器件**——只有"开"和"关"两种状态，给一个电平就够；
- 为什么中间要三极管：引脚只能输出几毫安，继电器线圈要 70mA 级电流，**三极管当开关放大器**；
- 为什么继电器要并联二极管（1N4148）：线圈断电瞬间会产生高压尖峰，二极管给它一条泄放回路，保护三极管。

## 第四步 · "数据"是什么

没有数据，只有两个状态：

| 动作 | 蜂鸣器 PC15 | 继电器 PC14 |
|---|---|---|
| 开 | 写 1（响） | 写 0（吸合，听到"咚"） |
| 关 | 写 0（停） | 写 1（释放） |

## 第五步 · 怎么控制起来（流程）

1. 上电初始化（上面第二步）；
2. 主循环里每 10ms 查一次按键 KEY1(PB7)、KEY2(PB6)（消抖见任务三）；
3. 按下 KEY1 → 蜂鸣器状态取反；按下 KEY2 → 继电器状态取反；
4. 同时支持串口命令 `B`/`R`/`?`——**按键和串口走同一个翻转函数**，所以串口验证通过就等于按键逻辑正确（这是远程验证的技巧）。

## 第六步 · 完整代码（`User/main.c`）

```c
/* 见文件 User/chapters/ch4_buzzer_relay.c，内容如下： */
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
```

## 第七步 · 上板验证

| 操作 | 现象 | 结果 |
|---|---|---|
| 按 KEY1 | 蜂鸣器响；再按停 | ✅ 用户实测通过 |
| 按 KEY2 | 继电器"咚"一声吸合，旁小灯亮；再按释放 | ✅ 用户实测通过 |
| 串口发 B / R / ? | 蜂鸣器/继电器翻转、回报状态 | ✅ 实测通过 |
| 编译 | 0 错 0 警 Code=4584B | ✅ |

---

# 任务五 · 温湿度（DHT11）

## 第一步 · 引脚从哪来

原理图 "温湿度传感器" 接口 + **实测校正**：

| DHT11 引脚 | 接到 | 为什么 |
|---|---|---|
| VCC | 3V3 | 供电 |
| DAT | **J1 的 B14**（= 芯片 PB14） | 数据线，单总线双向 |
| GND | GND | 共地 |

> ⚠️ **为什么不用板上专用座/B15**：本板专用座走线不可靠、B15 孔在多次插拔后接触不良，实测改用**飞线直插 B14** 后稳定工作。**规则：以实测为准。**

## 第二步 · 初始化

```c
/* 数据线 PB14：平时上拉输入 */
GPIO_InitTypeDef g = {0};
g.Pin  = GPIO_PIN_14;
g.Mode = GPIO_MODE_INPUT;
g.Pull = GPIO_PULLUP;
HAL_GPIO_Init(GPIOB, &g);
```

微秒级延时用 **DWT 周期计数器**（SysTick 只能做到毫秒，DHT 时序要微秒）：

```c
#define DWT_CYCCNT (*(volatile uint32_t *)0xE0001004)
static void delay_us(uint32_t us){ uint32_t s=DWT_CYCCNT; while((DWT_CYCCNT-s) < us*72){} }
```

## 第三步 · 用什么方法通信，为什么

**单总线（1-Wire）**，只用一根数据线——因为 DHT11 就设计成一根线既收又发：

- 发，靠 MCU 主动拉低/释放线；
- 收，靠 MCU 数对方拉低的**时间宽度**（宽=1，窄=0）。

**这是最省钱也最考验时序的方式**：没有时钟线，全靠"约定好的时间"对齐，所以必须用微秒延时。

## 第四步 · 数据长什么样（40 位 = 5 字节）

| 字节 | 含义 | 例 |
|---|---|---|
| d[0] | 湿度整数部分 | 37 |
| d[1] | 湿度小数部分（DHT11 恒为 0） | 0 |
| d[2] | 温度整数部分 | 31 |
| d[3] | 温度小数部分（DHT11 恒为 0） | 0 |
| d[4] | **校验和** = 前 4 字节之和的低 8 位 | 37+0+31+0=68 |

读到的 40 位里，`1` 表示高电平持续 **70µs**、`0` 表示高电平持续 **27µs**（每位都以 50µs 低电平开头）。

## 第五步 · 怎么读到正常值（完整流程）

```
① MCU 拉低数据线 20ms → 释放（告诉 DHT：我要读了）
② DHT 应答：先拉低 80µs，再拉高 80µs
③ ★等应答的高电平结束（漏掉这步 → 整个数据错位一位 → 校验"失败"）
④ 连续读 40 位：每位先等 50µs 低电平结束 → 延时 40µs 采样：
      还高 = 1（70µs 宽）；已低 = 0（27µs 宽）
⑤ 校验：d[0]+d[1]+d[2]+d[3] 的低 8 位 == d[4] → 数据可信
⑥ 湿度 = d[0].d[1]%，温度 = d[2].d[3]°C
```

> ⚠️ **两个必知的坑**：
> 1. 第③步漏掉会"错位一位"，表现为校验失败——**不是传感器坏**；
> 2. 读取中途失败后 DHT11 会**卡死**（拉着数据线不放，后续全部超时）——**唯一解法：把它的 VCC 拔插一次断电重启**。

## 第六步 · 完整代码（`User/main.c`，本固件同时实现任务五+六）

```c
/* 见文件 User/chapters/ch5_6_dht11_light_spl.c，内容如下： */
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
```

## 第七步 · 上板验证

| 操作 | 现象 | 结果 |
|---|---|---|
| 上电 | 串口每 2 秒打印 `HUMI=37.0%  TEMP=31.0C` | ✅ 实测（校验通过） |
| ST-Link 读 RAM | 湿度/温度/成功次数持续增长 | ✅ 实测 |

---

# 任务六 · 烟雾数据（光敏模块顶替）

## 第一步 · 引脚从哪来 + 为什么能顶替

| 光敏模块引脚 | 接到 |
|---|---|
| VCC | 3V3 |
| **DO** | **J1 的 B12**（= 芯片 PB12） |
| GND | GND |

**为什么光敏能顶替烟雾传感器**：两者对单片机来说**一模一样**——都是"比较器输出的数字信号（DO）"，芯片只读一个引脚的高低电平。买回真 MQ-2 插同一根线，**代码一行不用改**。

## 第二步 · 初始化（★本章最关键的配置）

```c
/* PB12：必须配「上拉输入」！ */
GPIO_InitTypeDef g = {0};
g.Pin  = GPIO_PIN_12;
g.Mode = GPIO_MODE_INPUT;
g.Pull = GPIO_PULLUP;               /* ★ 这一行就是"上拉" */
HAL_GPIO_Init(GPIOB, &g);
```

**为什么必须上拉**：光敏模块的 DO 是**开漏输出**（LM393 比较器）——它**只会拉低**，高电平必须靠上拉电阻拉起来。如果配成"浮空输入"（没有上拉），模块"释放"时线是悬空的、读数乱飘偏 0，模块"拉低"时是 0 → **两种状态都读成 0，表现为"怎么挡都不变"**。这是本任务最大的坑（和任务三按键"上拉要 ODR=1"是同一类陷阱）。

## 第三步 · 用什么方法读，为什么

**直接读 GPIO 输入电平**——模块已经把"亮/暗"比较成了 0/1，单片机不需要任何协议，读一位就行。

## 第四步 · 数据是什么

| DO 电平 | 含义 |
|---|---|
| 1（高） | 环境亮（正常） |
| 0（低） | 被遮挡/变暗（报警） |

## 第五步 · 怎么读到（流程）

```
① 读 PB12 电平（GPIO_ReadInputDataBit）
② 和上次的值比较，变了才打印/更新屏幕（事件驱动，不刷屏）
③ 主循环 10ms 一次，同时翻转心跳灯
```

## 第六步 · 代码（光敏相关，完整固件见任务五）

```c
/* 光敏 DO = PB12（上拉输入） */
static uint8_t light = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_12);

/* 串口打印 + 变化才提示 */
uart1_str("LIGHT="); uart1_send((uint8_t)('0'+light));

/* OLED 屏幕同步（寄存器/HAL 版有屏幕，标准库版仅串口） */
o_print(2, 48, light ? "OK  " : "DARK");
```

## 第七步 · 上板验证

| 操作 | 现象 | 结果 |
|---|---|---|
| 挡住光敏 | 串口 `LIGHT=1→0`；屏幕 `OK→DARK` | ✅ 用户实测通过 |
| 松开 | 恢复 `LIGHT=1 / OK` | ✅ 用户实测通过 |
| ST-Link 读 PB12 | 挡住=0、松开=1，两者都读到 | ✅ 实测（硬件层铁证） |

## 三腿函数名对照（同一件事，三种写法）

| 动作 | 标准库 | HAL | 寄存器 |
|---|---|---|---|
| 开时钟 | `RCC_APB2PeriphClockCmd` | `__HAL_RCC_GPIOx_CLK_ENABLE` | `RCC->APB2ENR \|= 位` |
| 配引脚 | `GPIO_Init` + Mode 枚举 | `HAL_GPIO_Init` + Mode/Pull 字段 | `CRL/CRH` 半字节 |
| 写电平 | `GPIO_WriteBit` | `HAL_GPIO_WritePin` | `BSRR` 置位/复位 |
| 读电平 | `GPIO_ReadInputDataBit` | `HAL_GPIO_ReadPin` | `IDR & 位` |
| 上拉输入 | `GPIO_Mode_IPU` | `Pull=GPIO_PULLUP` | `CRH=0x8 且 ODR=1` |

# 任务七 · OLED 显示

## 接线

| OLED 引脚 | 接到 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SCL | **B8** |
| SDA | **B9** |

## 代码要点（HAL 写法，完整代码同 `ch5_6_dht11_light_hal.c`）

HAL 没有"软件 I2C"外设函数，用 HAL_GPIO_Init/WritePin 手动翻转电平：

```c
static void sda_lo(void){ GPIO_InitTypeDef g={0};              /* 拉低 = 推挽输出0 */
  g.Pin=GPIO_PIN_9; g.Mode=GPIO_MODE_OUTPUT_PP; HAL_GPIO_Init(GPIOB,&g);
  HAL_GPIO_WritePin(GPIOB,GPIO_PIN_9,GPIO_PIN_RESET); }
static void sda_rel(void){ GPIO_InitTypeDef g={0};             /* 释放 = 上拉输入 */
  g.Pin=GPIO_PIN_9; g.Mode=GPIO_MODE_INPUT; g.Pull=GPIO_PULLUP; HAL_GPIO_Init(GPIOB,&g); }
```

屏幕显示四行：`F103 SMART ENV / LIGHT: OK|DARK / HUMI: xx.x% / TEMP: xx.xC`，与串口打印一致。

## 你会看到

上电约 10 秒清屏画字，之后挡光敏 `LIGHT` 行实时变 `DARK`，温湿度每 2 秒刷新。


# 任务八 · SPI 读取 Flash（W25Q 系列）

## 硬件事实（实测定案，以实物为准）

| 项 | 实测值 | 说明 |
|---|---|---|
| 板载 Flash | **W25Q16（2MB）** | JEDEC ID 实测 `EF 40 15`——不是任务书的 W25Q64、也不是原理图的 W25Q128！W25Q 全家族命令兼容，驱动通用 |
| 接线 | **CS=PB12, SCK=PB13, MISO=PB14, MOSI=PB15** | 板载焊死，挂 SPI2（无需接线） |
| SPI2 时钟 | APB1 = 36MHz，分频 8 → **4.5MHz** | 模式 0（CPOL=0, CPHA=0） |
| ⚠️ 冲突 | PB12/PB14 与光敏(DO)/温湿度(DAT) 共用 | **做本章前拔掉这两根线** |

## 命令表（W25Q 通用）

| 命令 | 作用 | 用法 |
|---|---|---|
| 0x9F | 读 JEDEC ID | CS低→发0x9F→读3字节→CS高 |
| 0x05 | 读状态寄存器 | bit0=1 表示芯片忙 |
| 0x03 | 读数据 | 发0x03+3字节地址，然后连续读 |
| 0x06 / 0x02 | 写使能 / 页编程 | 写入前必须先 0x06 |
| 0x20 | 扇区擦除(4KB) | 写入前先擦除（Flash 只能 1→0） |

## 三腿代码差异

| 动作 | 标准库 | HAL | 寄存器 |
|---|---|---|---|
| SPI 初始化 | `SPI_Init(SPI2,&spi)` 结构体 | `HAL_SPI_Init(&hspi2)` 句柄 | `SPI2->CR1 = MSTR\|SSM\|SSI\|SPE\|BR=3` |
| 发一字节 | `SPI_I2S_SendData` + 等 TXE/RXNE | `HAL_SPI_TransmitReceive` | 等 `SR.TXE` 写 `DR`，等 `SR.RXNE` 读 `DR` |
| 片选 | `GPIO_WriteBit(PB12)` | `HAL_GPIO_WritePin` | `ODR` 位操作 |
| CRH 配置 | `GPIO_Init` 枚举 | `HAL_GPIO_Init` | `CRH = 0xB4B2xxxx`（PB13/15=AF推挽, PB14=浮空, PB12=输出）|

## 上板验证（2026-10-06 深夜，三腿全部实测）

```
STATUS=0x00 (idle)
JEDEC ID: EF 40 15  -> Winbond OK（W25Q16）
DATA[0..3]: FF FF FF FF（空白芯片）
```
三腿验证方法相同：结果写 RAM `0x20004000`，用
`STM32_Programmer_CLI -c port=SWD mode=HotPlug -r8 0x20004000 8` 直接读。

## 想继续深入（明天可选）

写测试（需拔传感器线）：发 `0x06`(写使能) → `0x20`(擦除某扇区) → 等忙标志清零 → `0x02`+地址+数据写入 → `0x03` 读回比对。注意地址不要超过 **2MB**（W25Q16 实际容量）。
