# MSPM0G3519 NUEDC-2026 电赛基础平台

基于 TI **MSPM0G3519SPZR**（Arm Cortex-M0+, 80MHz）的电赛开发平台，集成 LCD 菜单交互系统与多外设驱动，使用 IAR EWARM + DAPLink (CMSIS-DAP) 开发工具链。

## 功能概览

启动后播放**开机动画**（色彩测试 → 启动信息 → LED 闪烁 + 蜂鸣器短响），然后进入 **TFT LCD 菜单界面**（2026-07-22 共 4 项）：

| 菜单项 | 功能 |
|---|---|
| **K230 Test** | K230 视觉模块测试（UART6/J11 双向通信：接收真实 YbProtocol 颜色帧 + S0 发 $SWITCH# 切换颜色阈值，LCD 色块追踪画框） |
| **CCD Test** | 线阵 CCD 测试（128 像素采集 + LCD 实时波形、连续/单拍模式、双通道切换、曝光调节） |
| **AD5933 Test** | AD5933 阻抗测量测试（I2C1 通信验证、温度读取、GainFactor 标定、阻抗测量） |
| **Motor Test** | DRV8874 双电机驱动测试（TIMA0 PWM 20kHz，M1/M2 独立控制，AD2 波形验证） |

> **Motor Test 操作**：S0/S1 调占空比(±5%)、S2 切换方向(FWD↔REV)、编码器左转=M2 右转=M1、PUSH 退出。
> M1/M2 方向和占空比**独立存储**，切换后另一电机保持运行。
> **AD2 示波器**：探头接拓展板 **J10**（M1+ M1-）或 **J11**（M2+ M2-），量程 ±25V。
> **必须打开 SW1 接通电池（VBAT）**才能在输出端看到 PWM 波形。无 VBAT 时用逻辑分析仪接 J14 侧 IN1/IN2 观测 3.3V 逻辑电平。
> 详见 [`docs/DRV8874_Motor_Use.md`](empty_mspm0g3519/docs/DRV8874_Motor_Use.md)。

> ~~UART Test~~ 已移除：脱机（不接 DAPLink）时 UART0 TX 阻塞导致程序卡死，根因是 NRST=2.5V 临界电压影响 MFCLK 时钟稳定性。仅接 DAPLink 调试时可临时启用以太网 printf 输出。

> LED、蜂鸣器、按键等基础 GPIO 功能已在开机动画中充分验证，不再单独列为菜单项。

按键角色：**S0**=上移、**S1**=下移、**S2**=确认、**PUSH**=返回

> **已封存模块**：DDS Test（AD9833，PC2/PC3 与 CCD 共用，已验证通过，代码保留供信号题启用）。
> **暂未启用**：MPU6050（无驱动代码）、通用 ADC（J2 五路，无驱动代码）。编码器驱动已初始化但未加入独立菜单项。

## 硬件连接

| 项目 | 说明 |
|---|---|
| **主控** | TI MSPM0G3519SPZR（100 引脚 LQFP） |
| **LED** | D1 接 **PB5**（物理引脚 26），**低电平点亮** |
| **蜂鸣器** | PA13，有源蜂鸣器，高电平驱动 |
| **TFT LCD** | ST7735 160×128，SPI1（PB30/PB31/PB14），10MHz |
| **按键** | S0(PA18)、S1(PC0)、S2(PA16)、PUSH(PA12) |
| **编码器** | PHA0(PA14, 双边沿中断)、PHB0(PA15) |
| **CCD** | 128 像素线阵 CCD 双通道，SI/CLK GPIO + ADC1 序列采样（CCD1: PB18/CH5, CCD2: PB17/CH4） |
| **MPU6050** | 六轴 IMU（I2C0: PB21-SCL/PB22-SDA, 中断 PC8），盲走航向控制 |
| **UART** | UART0 调试：MFCLK 4MHz, 115200-8N1，PA10(TX)/PA11(RX)；UART6→K230：BUSCLK 80MHz, 115200，PC11(TX)/PC10(RX)，J11 |
| **电机驱动** | DRV8874×2（拓展板），TIMA0 PWM 20kHz：PB3(M1 PWM)/PB4(M1 DIR)/PB0(M2 PWM)/PB2(M2 DIR)，nSLEEP=PB1，nFAULT=PA7，需 VBAT(SW1) |
| **I2C** | I2C1→AD5933：PA29(SCL)/PA30(SDA)，100kHz，J19 桥接 |
| **调试器** | DAPLink (CMSIS-DAP v2, VID_0D28&PID_0204) |
| **供电** | USB-C，禁止多路同时供电 |

## 开发注意事项

> **SysConfig 先行**：在开发任何新功能/模块之前，务必先在 `empty_mspm0g3519.syscfg` 中正确配置对应外设（引脚、时钟源、电源域），重新生成 `ti_msp_dl_config.c/.h`，确认无误后再写应用代码。参考 `empty_mspm0g3519/docs/` 中的硬件文档和 SDK 官方例程。
>
> **教程参考**：[立创·泰山派 TI MSPM0 系列教程](https://wiki.lckfb.com/zh-hans/ti-series/) 是良好的 MSPM0 开发引导和示例参考。**但该教程使用的开发板（梁山派/泰山派等）与本项目 G3519 硬件平台不配套**，其引脚定义、外设配置、时钟树等不可直接套用，必须以本项目 `docs/` 中的硬件文档和 `empty_mspm0g3519.syscfg` 配置为最终依据。
>
> **K230 视觉模块**：亚博 K230 视觉识别模块对接（UART6/J11 接线、YbProtocol 协议、双向通信、LCD 色块追踪画框）见 [`docs/K230_Vision_Module_Use.md`](empty_mspm0g3519/docs/K230_Vision_Module_Use.md)。**双向链路已实测打通**（2026-07-19）。

## 开发环境

| 工具 | 版本 |
|------|------|
| **IAR EWARM** | 9.60.3 |
| **TI MSPM0 SDK** | 2.10.00.04（默认 `C:\ti\mspm0_sdk_2_10_00_04`） |
| **SysConfig** | 1.28.0 |

首次配置仅两个关键点（其余安装均为默认下一步，踩坑见「已知问题」）：

1. **IAR 全局变量**：`Tools → Configure Custom Argument Variables` → **Global** 选项卡（勿放 TI 组）添加
   `MSPM0_SDK_INSTALL_DIR` = SDK 路径、`SYSCONFIG_ROOT` = SysConfig 路径
2. **Flash loader 修复**（仅 DAPLink 需要）：`<IAR>\arm\config\flashloader\TexasInstruments\FlashMSPM0GX51X.mac` 第 63 行附近改为
   `} else if(__driverType("ijet") || __driverType("cmsisdap")) {`

## 编译与烧录

双击 `empty_mspm0g3519_nortos_iar.eww` → 确认 `Project → Options → Debugger → Driver` = **CMSIS-DAP**（SWD、1000kHz、Hardware Reset）→ `F7` 编译 → `Ctrl+D` 烧录调试 → `F5` 运行。

命令行编译：`iarbuild.exe empty_mspm0g3519_nortos_iar.ewp -build Debug`

## 工程结构

```
empty_mspm0g3519/
├── iar/                             ← $PROJ_DIR$（工程根目录）
│   ├── empty_mspm0g3519_nortos_iar.eww  ← IAR 工作区（双击打开）
│   ├── empty_mspm0g3519_nortos_iar.ewp  ← IAR 工程文件
│   ├── empty_mspm0g3519_nortos_iar.ipcf ← SysConfig 构建规则
│   ├── empty_mspm0g3519.c           ← 主程序（开机动画 + 菜单交互）
│   ├── empty_mspm0g3519.syscfg      ← SysConfig 引脚配置
│   ├── mspm0g3519.icf               ← 链接脚本
│   ├── ti_msp_dl_config.c/.h        ← SysConfig 生成（勿手动编辑）
│   └── iar/
│       └── startup_mspm0g351x_iar.c ← 启动文件
├── TSP3519/                          ← 板级支持库
│   ├── tsp_common_headfile.h         ← 公共头文件枢纽
│   ├── tsp_gpio.h / tsp_gpio.c       ← GPIO 宏封装（LED/蜂鸣器/LCD/CCD/按键/编码器）
│   ├── TSP_TFT18.h / TSP_TFT18.c     ← TFT LCD 驱动（ST7735, 160×128, SPI1）
│   ├── tsp_ccd.h / tsp_ccd.c         ← 128 像素线阵 CCD 驱动（双通道，ADC1 序列采样）
│   └── tsp_menu.h / tsp_menu.c       ← LCD 菜单系统（列表+子菜单+增量重绘）
└── NUEDC2025/                        ← 应用层驱动
    ├── tsp_isr.h / tsp_isr.c         ← SysTick 延时 + GROUP1/UART0/UART6 中断分发
    ├── tsp_key.h / tsp_key.c         ← 4键扫描（20ms 消抖，边沿检测）
    ├── tsp_encoder.h / tsp_encoder.c ← 编码器驱动（PHA0 中断正交解码）
    ├── tsp_uart.h / tsp_uart.c       ← UART0 通信（MFCLK 4MHz, 环形缓冲 RX, printf 重定向）
    ├── tsp_uart_k230.h / tsp_uart_k230.c ← UART6 驱动（K230, BUSCLK 80MHz, 环形缓冲 RX）
    ├── tsp_k230.h / tsp_k230.c       ← K230 YbProtocol 解析（主循环状态机断帧）
    ├── tsp_motor.h / tsp_motor.c     ← DRV8874 电机驱动（TIMA0 PWM 20kHz, M1/M2 独立控制）
    └── tsp_dds.h / tsp_dds.c         ← AD9833 DDS 波形发生器（GPIO bit-bang, 方波/正弦/三角波）
```

K230 侧 MicroPython 脚本位于 `empty_mspm0g3519/k230_scripts/`（CanMV IDE 中打开运行）：
- `k230_bidir_test.py` / `k230_link_test.py` = 早期链路测试脚本（模拟帧，**已被固件方案取代**）
- **当前方案**：修改原厂固件 `sdcard/apps/color_det/color_recog/color_recognition.py`，在颜色识别循环中加入 UART TX（发真实 YbProtocol 帧）和 UART RX（收 $SWITCH# 切换颜色阈值），保持 K230 GUI 和屏幕显示完整不变。见 [`docs/K230_Vision_Module_Use.md`](empty_mspm0g3519/docs/K230_Vision_Module_Use.md) §5。

## 已知问题

| 问题 | 现象 | 解决 |
|------|------|------|
| Flash loader Device ID 不匹配 | 使用 DAPLink 烧录失败 | 按上文步骤 5 修复 Flash loader |
| LED 不亮 | 编译烧录成功但灯不闪 | 确认调用了 `DL_GPIO_enableOutput()`，SysConfig 生成的代码会自动处理 |
| 全局变量被覆盖 | 手动编辑 `.custom_argvars` 后无效 | 只能通过 IAR GUI 设置全局变量，且必须放 Global 下 |
| 设备锁定警告 | 首次下载弹出 "Device is locked" | 点 Yes/OK 执行 Mass Erase 即可，不损坏芯片 |
| PHA0 编码器噪声 | 未接编码器时光标抖动 | 已在 `tsp_encoder_init` 中默认禁用 PHA0 中断，接编码器后手动使能 |
| **UART0/printf 脱机阻塞** | 不接 DAPLink 时程序死在 `DL_UART_transmitDataBlocking()`，菜单不出现 | NRST=2.5V 临界电压导致 MFCLK 时钟不稳定；已从 main() 移除 `tsp_uart_init()` 和所有 UART0 TX 调用。仅接 DAPLink 时可恢复。详见 §5 |

> **脱机=仅 USB-C 供电、不接 DAPLink 排线、不接 K230 UART。** 接 DAPLink 时所有功能正常。

## 参考资源

- [TI MSPM0 SDK 文档](https://www.ti.com/tool/MSPM0-SDK)
- [TI MSPM0G3519 数据手册](https://www.ti.com/product/MSPM0G3519)
- [IAR EWARM 用户指南](https://www.iar.com/support/user-guides/)
