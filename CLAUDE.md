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

详见 `MSPM0G3519_IAR_DAPLink_开发板验证操作手册.md`。

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

实际开发工作在 `empty_mspm0g3519/` 目录，基于 SDK 示例复制并适配：

```
empty_mspm0g3519/                    ← $PROJ_DIR$（工程根目录）
├── empty_mspm0g3519_nortos_iar.eww  ← IAR 工作区（从此打开）
├── empty_mspm0g3519_nortos_iar.ewp  ← IAR 工程文件
├── empty_mspm0g3519_nortos_iar.ipcf ← IAR Project Connection（SysConfig 构建规则）
├── empty_mspm0g3519.c               ← 主程序（LED 闪烁）
├── empty_mspm0g3519.syscfg          ← SysConfig 配置
├── mspm0g3519.icf                   ← 链接脚本
├── ti_msp_dl_config.c / .h          ← SysConfig 生成（不要手动编辑）
└── iar/
    └── startup_mspm0g351x_iar.c     ← 启动文件
```

> **注意**: SDK 原版示例的 `.ewp` 放在 `iar/` 子目录里导致 `$PROJ_DIR$` 指向 `iar/` 而非工程根目录，所有路径错位。本工程已将 `.ewp`/`.eww`/`.ipcf`/`.icf` 移到根目录。

### PB5 引脚映射

| 物理引脚 | IOMUX 索引 | GPIO | API |
|---|---|---|---|
| 26 (PB5) | `IOMUX_PINCM18` | `GPIOB`, `DL_GPIO_PIN_5` | `DL_GPIO_initDigitalOutput` + `DL_GPIO_enableOutput` |

### LED 验证代码要点

```c
DL_GPIO_initDigitalOutput(IOMUX_PINCM18);  // 1. 设引脚功能为 GPIO
DL_GPIO_enableOutput(GPIOB, DL_GPIO_PIN_5); // 2. 开输出使能（容易漏！）
DL_GPIO_setPins(GPIOB, DL_GPIO_PIN_5);      // 3. 初始高电平（LED 低有效 = 灭）
DL_GPIO_togglePins(GPIOB, DL_GPIO_PIN_5);   // 4. 翻转
```

### 调试器配置

- Driver = `CMSIS-DAP`, Interface = `SWD`, 速度 = `1000 kHz`
- 不稳定时降速到 `100 kHz`
- **Reset**: 选 Hardware Reset（需接 RST 线）

### 已知踩坑

1. **IAR 全局变量持久化失败**: `global.custom_argvars` 在 IAR 关闭时会被覆盖，不能通过文件编辑设置。通过 IAR GUI `Tools → Configure Custom Argument Variables → Global` 设置 `MSPM0_SDK_INSTALL_DIR` 和 `SYSCONFIG_ROOT`
2. **Flash loader 报 Device ID 不匹配**: TI 的 `FlashMSPM0GX51X.mac` 缺 CMSIS-DAP 分支，需手动添加 `|| __driverType("cmsisdap")`。已修复
3. **DL_GPIO_initDigitalOutput 不设方向**: 该函数只配引脚功能为 GPIO，还需调用 `DL_GPIO_enableOutput` 才能输出

## 参考文档

- `MSPM0G3519_IAR_DAPLink_开发板验证操作手册.md` — 完整验证流程与故障排查
- TI MSPM0 SDK: `C:\ti\mspm0_sdk_2_10_00_04`
- IAR EWARM: `D:\iar\ewarm-9.60.3`
