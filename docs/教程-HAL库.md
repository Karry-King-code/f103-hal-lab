#  HAL教程 · STM32F103C8T6 精英板

> 本章目标：搞清这个 HAL 工程从哪来、改了什么、怎么编译烧录；理解 HAL 工程的标准分层。
> 步骤照做可复现；知识点解释为什么。

---

## 1. 开发板与硬件事实

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

## 2. 这个工程从哪来（重要：来源与改动清单）

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

## 3. 软件安装

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

## 4. 编译

打开 `MDK-ARM/Project.uvprojx`，F7。预期：

```
Program Size: Code=1838 RO-data=302 RW-data=16 ZI-data=1024
"f103-hal-lab\f103-hal-lab.axf" - 0 Error(s), 0 Warning(s).
```

**知识点：为什么 HAL 脚手架比 SPL 脚手架"大"却 Code 更大？** 两个脚手架都是空主循环，但 HAL 工程把**全量 HAL .c 都加入工程**（官方模板风格）。链接器只把实际用到的函数链进镜像（所以 Code=1838B 并不大），但编译时间明显变长（要编 40+ 个文件）。SPL 仓库是"按需添加"风格。这是两种官方风格，各有取舍。

---

## 5. 烧录

```
STM32_Programmer_CLI -c port=SWD -w MDK-ARM\f103-hal-lab\f103-hal-lab.hex -v -rst
```

脚手架空主循环无现象，链路打通即可。注意 02 章开始才产生"1 秒闪一次"的可观察现象。

---

## 6. HAL 工程标准分层（每个文件的角色）

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

## 7. 上电到 main()（与 SPL 版对比）

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

## 8. Git 提交规矩

同标准库仓库：每章一 commit，格式 `章节: 内容 (HAL)`。

## 9. 常见问题

| 现象 | 原因 | 解决 |
|---|---|---|
| `stm32f1xx_nucleo.h not found` | 用了未裁剪的原模板 | 用本仓库已裁剪版本 |
| 编译慢 | HAL 全量源码在编译 | 正常现象（~20s），镜像不含未用代码 |
| 其他 | 同标准库仓库 01 章第 9 节 | — |

## 10. 下一步

02-GPIO输出：用 `HAL_GPIO_WritePin` 点亮 PC14，`HAL_Delay` 实现 1s 闪烁，并讲清 GPIO 推挽输出与 HAL_Delay 背后的 SysTick。

---

# 02 · 点亮 LED（HAL 版，1 秒闪烁）✅ 已验收 2026-10-04

> 同一个灯（PC13 绿灯），换 HAL 库实现。时钟 64MHz（模板默认 HSI），delay 用 HAL_Delay。

## 硬件事实（实测，与标准库版一致）

| 目标 | 引脚 | 依据 |
|---|---|---|
| 绿灯 | **PC13**（低电平亮） | 【实测】丝印 PC13 |
| 继电器 | PC14（想听咔哒就加它） | 【实测】 |
| PWR 红灯 | 常亮，不受控 | 【实测】丝印 PWR |

## 步骤 6 · 预检时钟：为什么我"不动"官方模板的时钟配置

**在哪看**：`Src/main.c` 的 `SystemClock_Config()`。
**看什么**：官方模板配的是 **内部 HSI → 64MHz**（不是外部晶振 72M）。
**为什么不改**：HSI 在芯片内部，永远起振、绝对可靠；HAL_Init/HAL_Delay 的 1ms 心跳由 HAL_RCC_ClockConfig 自动校准到实际频率，64M 还是 72M 延时都准。切 HSE 72M 留给时钟章节练手。
**我看到的**：模板 main() 里有一行官方预留注释 `/* Add your application code here */`——**代码就加在这里，这是 ST 规定的位置**。

## 步骤 7 · 写代码（改 `Src/main.c`）

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

## 步骤 8 · 编译

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

## 步骤 9 · 烧录

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

## 步骤 10 · 验收

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

---
---

# 03 · 按键控制 LED（HAL 版：按一下开始闪，再按一下停）🔶 代码已上板待确认

> 与标准库版（f103-spl-lab 03 章）逻辑一字不差，全部函数换成 HAL 写法。新知识点：**HAL 把"上拉"从隐藏动作变成了显式字段**，以及 **HAL 自带翻转函数**。

## 硬件事实（实测，三腿一致）

| 目标 | 引脚 | 依据 |
|---|---|---|
| 按键 KEY1 | PB7（按下接 GND，低有效，无外部上拉） | 【文档】原理图 P3 |
| 绿灯 | PC13（低电平亮） | 【实测】02 章 |

## 步骤 11 · 配置：HAL 的"Pull 字段"比标准库诚实

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

## 步骤 12 · 读按键与翻转：HAL 的三个顺手函数

```c
HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7)      /* 读引脚：返回 GPIO_PIN_SET(1)/GPIO_PIN_RESET(0) */
HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);   /* 写引脚 */
HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);  /* ★ 翻转：HAL 自带，标准库没有！ */
```

- `HAL_GPIO_TogglePin` 内部就是 `ODR ^= pin`（读-反-写）——标准库版我们手写的三行，HAL 一个函数搞定；
- `HAL_GPIO_ReadPin` 返回的是枚举 `GPIO_PinState`，判断按下用 `== GPIO_PIN_RESET`（0=按下）。

## 步骤 13 · 完整逻辑（与标准库版同构，只贴关键）

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

## 步骤 14 · 编译烧录（实测）

```
UV4.exe -b Project.uvprojx -j0 -o build.log
→ Program Size: Code=4106 RO-data=340 RW-data=16 ZI-data=1168
→ 0 Error(s), 0 Warning(s), 15 秒
STM32_Programmer_CLI -c port=SWD -w f103-hal-lab103-hal-lab.axf -v -rst
→ Download verified successfully + MCU Reset
```

体积继续膨胀（标准库版 3124B → HAL 版 4462B）：`HAL_Delay/Toggle/Read/Write` 一套库函数比标准库的更重。

## 步骤 15 · 验收

| 操作 | 预期 |
|---|---|
| 按一下 KEY1 | 绿灯 1 秒周期闪烁 |
| 再按一下 | 灯灭、停止 |
| 再按 | 恢复闪烁 |

## 三腿对照表更新（按键版）

| 动作 | 标准库 | HAL | 寄存器（预告） |
|---|---|---|---|
| PB7 上拉输入 | `GPIO_Mode_IPU`（上拉藏在 ODR） | `Mode=INPUT` + `Pull=GPIO_PULLUP`（显式） | `CRL[31:28]=0b1000` + `ODR\|=1<<7` |
| 读按键 | `GPIO_ReadInputDataBit` | `HAL_GPIO_ReadPin` | `GPIOB->IDR & (1<<7)` |
| 翻转 LED | 读-反-写三行 | **`HAL_GPIO_TogglePin` 一行** | `GPIOC->ODR ^= 1<<13` |
| 消抖延时 | 自己的 delay_ms | `HAL_Delay` | 自己的 delay_ms |
