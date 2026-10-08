# f103-hal-lab —— STM32F103C8T6 精英板 · HAL 库学习仓库

STM32F103C8T6 精英板（嘉立创 v1.3）外设驱动学习项目，**HAL 库**版本。

> 本仓库是三条学习线之一：同一套外设功能（LED → 串口 → 按键 → 蜂鸣器/继电器 → 温湿度 → 烟雾 → OLED → W25Q Flash → ESP8266 连 OneNET）分别用 **[标准库] / [HAL 库（本仓库）] / [寄存器]** 三种方式各实现一遍。

## 教程目录

| 章节 | 内容 | 状态 |
|---|---|---|
| [01-环境搭建与脚手架](docs/01-环境搭建与脚手架.md) | 官方模板裁剪、HAL 工程结构、编译烧录 | ✅ 脚手架编译通过 |
| 02-GPIO输出-LED 1s闪烁 | GPIO/HAL_Delay 中断派/SysTick | ✅ 上板验收 |
| 03-GPIO输入-按键控LED | Pull 显式字段/TogglePin/非阻塞分片 | ✅ 上板验收 |
| 04-USART-串口收发 | UART 句柄/HAL_UART_Init 自动算 BRR/阻塞收发 | ✅ 上板验收 |
| （后续按任务清单推进） | | |

## 工程一览

- **主控**：STM32F103C8T6（Cortex-M3，72MHz，64KB Flash / 20KB RAM）
- **库**：ST 官方 **STM32CubeF1 V1.8.6** 的 HAL 驱动（来自 Nucleo-F103RB 官方模板裁剪，改动清单见 01 章）
- **IDE**：Keil MDK5 + AC5 + Keil::STM32F1xx_DFP 2.3.0
- **烧录**：ST-Link（SWD）+ STM32CubeProgrammer

```
f103-hal-lab/
├─ Inc/  Src/                    # 用户代码（main/中断/msp/system）
├─ Drivers/CMSIS/                # ARM 内核层 + ST 设备头（stm32f1xx.h / stm32f103xb.h）
├─ Drivers/STM32F1xx_HAL_Driver/ # HAL 官方驱动全量
├─ MDK-ARM/Project.uvprojx       # Keil 工程
└─ docs/                         # 教程文档（原理图/手册不入库）
```

## 硬件资料说明

原理图与芯片手册因版权原因**保存在本地不入仓库**。板卡引脚分配速查见 `docs/01-环境搭建与脚手架.md`。

## 章节代码（每章独立，想跑哪章复制哪章）

> 一个工程同一时刻只编译一个 main.c。**方法：把 `Src/chapters/`（HAL为 `Src/chapters/`）里对应章节的 .c 内容整体复制进 main.c → 编译 → 烧录**。各章完整讲解见 docs/教程文档。

| 任务编号 | 章节 | 代码文件 | 现象（验收证据） |
|---|---|---|---|
| 任务一 | LED 1s 闪烁 | ch1_led_1s.c | 绿灯亮1秒灭1秒循环 |
| 任务二 | 串口收发 | ch2_uart_echo.c | 串口发什么回什么（回显） |
| 任务三 | 按键控制 LED | ch3_key_led.c | 按一下 KEY1 开始 1s 闪烁，再按停止 |
| 任务四 | 按键控蜂鸣器/继电器 | ch4_buzzer_relay.c | KEY1 蜂鸣器响/停，KEY2 继电器吸合/释放 |
| 任务五 | 串口打印温湿度 | ch5_dht11.c | 串口每 2 秒打印温湿度 |
| 任务六 | 串口打印烟雾(光敏替) | ch6_light.c | 挡光打印报警，松开恢复 |
| 扩展 | OLED 显示 | ch8_oled_display.c | 屏幕显示环境信息+实时光敏 |
