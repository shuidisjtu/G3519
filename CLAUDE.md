# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

MSPM0G3519 嵌入式开发项目。主控芯片为 TI `MSPM0G3519SPZR`（100 引脚，Arm Cortex-M0+），使用 IAR Embedded Workbench for Arm + DAPLink (CMSIS-DAP) 作为开发工具链。

## 硬件关键信息

- **调试口**: J1 (2×5)，红边对准 RST/1 脚
- **LED**: D1 接 `PB5`，**低电平点亮**（与逻辑电平相反）
- **供电**: 主板仅通过 USB-C 供电，禁止同时从多路供电
- **DAPLink 接线**: 必须连接 SWDIO(J1-7)、SWCLK(J1-5)、GND(J1-6/8/9)、VTref(J1-10)，建议连接 RST(J1-1)
- DAPLink VTref 接 `MCU_3V3`，不接 DAPLink 的 3.3V 电源输出

## 开发工具链

| 工具 | 安装路径 | 版本 | 用途 |
|---|---|---|---|
| IAR EWARM | `D:\iar\ewarm-9.60.3` | 9.60.3 ✅ | IDE、编译器、CMSIS-DAP 调试 |
| TI MSPM0 SDK | `C:\ti\mspm0_sdk_2_10_00_04` | 2.10.00.04 ✅ | driverlib、启动文件、SysConfig、示例工程 |
| DAPLink | USB `VID_0D28&PID_0204` | CMSIS-DAP v2 ✅ | 调试探头（非 XDS110） |

### 构建命令

无 `IarBuild.exe`（未安装 CLI 构建组件）。可通过 IDE 构建：

- **GUI**: 双击 `.eww` 工作区，在 IAR 中 `Project → Rebuild All`（`F7`）
- **命令行备选**: `D:\iar\ewarm-9.60.3\common\bin\IarIdePm.exe <project.ewp> -build <config>`

### IAR 关键文件验证

- 编译器: `D:\iar\ewarm-9.60.3\arm\bin\iccarm.exe`
- CMSIS-DAP 驱动: `D:\iar\ewarm-9.60.3\arm\bin\swtdarm_cmsisdap.dll`
- MSPM0G3519 DDF: `D:\iar\ewarm-9.60.3\arm\config\debugger\TexasInstruments\MSPM0G3519.ddf`
- G3519 Flash loader: `FlashMSPM0GX519.*`

### 工作工程

实际开发工作在 `empty_mspm0g3519/` 目录，参照老师例程 (`F:\Test\Example\TSP3519\`) 组织：

```
empty_mspm0g3519/                    ← 项目根目录
├── iar/                             ← $PROJ_DIR$（工程根目录）
│   ├── empty_mspm0g3519_nortos_iar.eww  ← IAR 工作区（从此打开）
│   ├── empty_mspm0g3519_nortos_iar.ewp  ← IAR 工程文件
│   ├── empty_mspm0g3519_nortos_iar.ipcf ← IAR Project Connection（SysConfig 构建规则）
│   ├── empty_mspm0g3519.c           ← 主程序（LCD + LED + 蜂鸣器）
│   ├── empty_mspm0g3519.syscfg      ← SysConfig 配置（全外设）
│   ├── mspm0g3519.icf               ← 链接脚本
│   ├── ti_msp_dl_config.c / .h      ← SysConfig 生成（不要手动编辑）
│   ├── Event.dot                    ← SysConfig 生成
│   └── iar/
│       └── startup_mspm0g351x_iar.c ← 启动文件
├── TSP3519/                          ← 板级支持库
│   ├── tsp_common_headfile.h         ← 公共头文件枢纽
│   ├── tsp_gpio.h / tsp_gpio.c       ← GPIO 宏封装
│   └── TSP_TFT18.h / TSP_TFT18.c     ← TFT LCD 驱动（160×128）
└── NUEDC2025/                        ← 中断服务
    └── tsp_isr.h / tsp_isr.c         ← SysTick 延时 + UART ISR 框架
```

> **注意**: 此结构仿照老师参考例程，`$PROJ_DIR$` = `iar/`，库目录与工程目录平级。`.ewp` 中的路径使用 IAR 自定义变量（`$MSPM0_SDK_INSTALL_DIR$`、`$SYSCONFIG_ROOT$`），非硬编码。

### PB5 引脚映射

| 物理引脚 | IOMUX 索引 | GPIO | API |
|---|---|---|---|
| 26 (PB5) | `IOMUX_PINCM18` | `GPIOB`, `DL_GPIO_PIN_5` | `DL_GPIO_initDigitalOutput` + `DL_GPIO_enableOutput` |

### 全板外设引脚（SysConfig 配置）

| 类别 | 引脚 | 外设宏 | 功能 |
|---|---|---|---|
| **LED** | PB5 | `LED_ON/OFF/TOGGLE` | 低电平点亮 |
| **蜂鸣器** | PA13 | `BUZZ_ON/OFF/TOGGLE` | 有源蜂鸣器 |
| **TFT LCD** | SPI1: PB30(PICO), PB31(SCLK), PB14(POCI) | `LCD_INST`=SPI1 | 160×128 彩色屏，10MHz SPI |
| **LCD 控制** | PA8(RST), PA9(BL), PB28(CS), PB29(DC) | `LCD_RST/BL/CS/DC` | 复位/背光/片选/数据命令 |
| **按键** | PA18(S0), PC0(S1), PA16(S2), PA12(PUSH) | `S0/S1/S2/PUSH` | 数字输入 |
| **编码器** | PA14(PHA0), PA15(PHB0) | `PHA0/PHB0` | PHA0 带双边沿中断 |
| **CCD** | PC9(SI1), PB20(CLK1), PC4(SI2), PC5(CLK2) | `CCD_SI1/CLK1/SI2/CLK2` | 线性 CCD 接口 |
| **电源控制** | PB1(SLEEP), PA7(FAULT) | `SLEEP/FAULT` | 睡眠/故障检测 |

### 时钟配置

- **CPUCLK = 80MHz**: HFXT 40MHz → SYSPLL (qDiv=3, pDiv=1, MCLK=CLK0) → 80MHz
- Flash wait state = 2
- **SysTick**: 1ms 中断 (period=80000)，驱动 `delay_1ms()`
- **LFXT**: 32.768kHz 外部晶振已使能

### 外设驱动 API 要点

```c
// 初始化：全部由 SYSCFG_DL_init() 自动完成（GPIO/SPI/时钟/SysTick）
SYSCFG_DL_init();

// LED/蜂鸣器：通过 tsp_gpio.h 宏操作（已封装电平逻辑）
LED_ON();     // = DL_GPIO_clearPins(GPIOB, PIN5) — 低电平点亮
LED_OFF();    // = DL_GPIO_setPins(GPIOB, PIN5)
LED_TOGGLE();
BUZZ_ON();    // = DL_GPIO_setPins(GPIOA, PIN13)
BUZZ_OFF();

// SysTick 延时：tsp_isr.c 提供，1ms 单位（基于 80MHz CPUCLK）
delay_1ms(100);  // 延时 100ms

// TFT LCD：TSP_TFT18.c 驱动 SPI1
tsp_tft18_init();         // 初始化 LCD
tsp_tft18_show_str_color(0, 0, "text", BLUE, YELLOW);  // 显示字符串
```

### 调试器配置

- Driver = `CMSIS-DAP`, Interface = `SWD`, 速度 = `1000 kHz`
- 不稳定时降速到 `100 kHz`
- **Reset**: 选 Hardware Reset（需接 RST 线）

### 已知踩坑

1. **IAR 全局变量持久化失败**: `global.custom_argvars` 在 IAR 关闭时会被覆盖，不能通过文件编辑设置。通过 IAR GUI `Tools → Configure Custom Argument Variables → Global` 设置:
   - `MSPM0_SDK_INSTALL_DIR` = `C:\ti\mspm0_sdk_2_10_00_04`
   - `SYSCONFIG_ROOT` = `C:\ti\sysconfig_1.28.0`
   
   **注意**：`.custom_argvars` 文件会被 IAR 持续改回 `TI` 组，因此变量**必须配在 Global 而非特定组下**
2. **Flash loader 报 Device ID 不匹配**: TI 的 `FlashMSPM0GX51X.mac` 缺 CMSIS-DAP 分支，需手动添加 `|| __driverType("cmsisdap")`。已修复
3. **DL_GPIO_initDigitalOutput 不设方向**: 该函数只配引脚功能为 GPIO，还需调用 `DL_GPIO_enableOutput` 才能输出。SysConfig 生成的代码会自动处理
4. **下载调试时弹出设备锁定警告**: MSPM0G3519 首次下载或异常断开后芯片会进入锁定状态，IAR 弹出 "Device is locked" 警告框。**直接点 Yes/OK 执行 Mass Erase 解锁即可**，这是正常流程，不会损坏芯片。后续正常下载不会再出现

### 环境搭建步骤

1. **安装独立版 SysConfig**: 从 https://www.ti.com/tool/SYSCONFIG 下载安装，默认路径 `C:\ti\sysconfig_<version>\`
2. **配置 IAR 集成 SysConfig**: IAR → `Tools` → `Configure Viewers` → `Import` → 选择 `C:\ti\mspm0_sdk_2_10_00_04\tools\iar\sysconfig_iar_setup.xml` → 确认
3. **配置 IAR 全局变量**: IAR → `Tools` → `Configure Custom Argument Variables` → `Global` → 设置:
   - `MSPM0_SDK_INSTALL_DIR` = `C:\ti\mspm0_sdk_2_10_00_04`
   - `SYSCONFIG_ROOT` = `C:\ti\sysconfig_1.28.0`（独立版） 或 `D:\ti\ccs2100\ccs\utils\sysconfig_1.28.0`（CCS 自带）

## 参考文档

- TI MSPM0 SDK: `C:\ti\mspm0_sdk_2_10_00_04`
- IAR EWARM: `D:\iar\ewarm-9.60.3`
- SysConfig 独立版: `C:\ti\sysconfig_1.28.0`
- IAR SysConfig 集成配置: `C:\ti\mspm0_sdk_2_10_00_04\tools\iar\sysconfig_iar_setup.xml`
