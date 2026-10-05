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
| 3 | CubeProgrammer 报 `invalid ELF file`，报错路径被截成 `Desktop//HAL/...` | 命令行参数里的**中文路径**（姚凯锐任务/HAL库）传参时被编码吃掉 | `cd` 进 .axf 所在目录，用**相对路径** `-w f103-hal-lab/f103-hal-lab.axf`【实测 2026-10-05】 |

---
---

# 03 · 按键控制 LED（HAL 版：按一下开始闪，再按一下停）✅ 已验收 2026-10-05

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

# 04 · 串口收发（HAL 版）✅ 已上板 2026-10-05

### 硬件事实（三腿一致）

| 项 | 值 | 来源 |
|---|---|---|
| 串口 | USART1：**PA9=TX、PA10=RX**，115200-8-N-1 | 原理图 P2 + 实测 |
| 接线（外接 USB-TTL 时） | 模块 RX→板 A9、模块 TX→板 A10、GND→GND | **TX/RX 交叉**，【实测】双向通 |
| 本腿时钟 | **APB2 = 64MHz**（HSI/2×16，APB2=HCLK，官方模板原样未动） | 步骤 6 的决定 |

### 步骤 16 · 时钟与引脚：HAL 写法

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

### 步骤 17 · 句柄 + HAL_UART_Init：BRR 它替你算

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

### 步骤 18 · 收发：HAL 的阻塞 API

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

### 步骤 19 · 编译烧录验证（实测）

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

# 05 · 蜂鸣器 + 继电器（HAL 版）✅ 已上板 2026-10-06

### 硬件事实

与标准库章完全一致（PC15 蜂鸣器高响 / PC14 继电器**低吸合实测** / PC13 心跳灯），此处不重复——**硬件事实三腿只有一份**。PC14/15 备份域引脚注意事项同前。

### 步骤 20 · HAL 写法：三脚一次配 + 有效电平宏

```c
GPIO_InitStruct.Pin   = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
GPIO_InitStruct.Pull  = GPIO_NOPULL;
GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);          /* 三格半字节一次写好 */

#define BUZZ_ON()   HAL_GPIO_WritePin(GPIOC, GPIO_PIN_15, GPIO_PIN_SET)
#define RELAY_ON()  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_14, GPIO_PIN_RESET)  /* 低吸合(实测) */
```

和标准库的差异只有函数名：`GPIO_WriteBit` ↔ `HAL_GPIO_WritePin`，`Bit_RESET` ↔ `GPIO_PIN_RESET`——**同一颗芯片、同一个 CRH 格子**。

### 步骤 21 · 新知识点：HAL_UART_Receive 的"超时轮询"

04 章回显用的是 `HAL_MAX_DELAY`（收不到就死等）。本章主循环还要扫按键，死等会卡住按键——改成 **10ms 超时轮询**：

```c
if (HAL_UART_Receive(&huart1, &b, 1, 10) == HAL_OK)   /* 最多等 10ms */
```

内部就是"轮询 RXNE，数着 uwTick 到 10ms 就返回 HAL_TIMEOUT"。这就是**非阻塞化的最朴素手段**：超时短一点，主循环就"既看串口又看按键"。

> ⚠️ 函数式宏（`BUZZ_ON()`）**不能放进三目运算符**取用：`(cond ? BUZZ_ON : BUZZ_OFF)()` 预处理后变成 `(cond ? 函数调用表达式 : 函数调用表达式)()`——对一个 void 表达式再取函数调用，编译必错（本章实测 8 errors）。老老实实 `if/else`。

### 步骤 22 · 编译烧录验证（实测）

```
Program Size: Code=4048 RO-data=312 RW-data=24 ZI-data=1096 → 0 Error(s), 0 Warning(s)
Download verified successfully（cd 后相对路径烧 .axf）
```

| 操作 | 现象 | 结论 |
|---|---|---|
| 复位 | 横幅 `=== CH05 BUZZER+RELAY (HAL) READY ===` | ✅ |
| 串口 `B`+`R` | BUZZER=ON / RELAY=ON(closed) 依次回报 | ✅ |
| SWD 读 GPIOC_ODR | 0x0000A000（PC15=1 蜂鸣器得电、PC14=0 低吸合） | ✅ 与标准库版**逐位一致** |
| 按 KEY1/KEY2 | 与串口命令同路径 | 🔶 待用户按键确认 |

# 07 · 光敏 DO 读取（HAL 版）✅ 已上板 2026-10-06

硬件事实与"烟雾替身"原理见标准库章。HAL 差异只有函数名：`HAL_GPIO_ReadPin`（返回 `GPIO_PIN_SET/RESET`，记得转成 0/1），输入模式 `GPIO_MODE_INPUT`+`GPIO_NOPULL`（显式浮空）。

主循环沿用 05 章的 **10ms 超时轮询**（`HAL_UART_Receive(...,10)`），既扫电平变化又收 `'?'` 查询。

```
Program Size: Code=3844 → 0 Error(s), 0 Warning(s)；烧 .axf verified
实测：横幅+DO=1+查询 DO=1 ✅（与标准库版一致）
