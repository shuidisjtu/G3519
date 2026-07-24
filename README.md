# MSPM0G3519 信号题专用平台

从 [G3519](../G3519) 综合平台拆分，专注 NUEDC-2026 电赛**信号题**开发。主控 TI MSPM0G3519SPZR（Arm Cortex-M0+, 80MHz），IAR EWARM + DAPLink (CMSIS-DAP)。

## 功能概览

启动后播放开机动画（色彩测试 → 启动信息 → LED 闪烁 + 蜂鸣器短响），然后进入 TFT LCD 菜单界面：

| 菜单项 | 功能 |
|---|---|
| **AD5933 Test** | AD5933 阻抗测量（I2C1 通信验证、温度读取、GainFactor 标定、阻抗测量） |
| **DDS Test** | AD9833 DDS 波形发生器（方波/正弦/三角波切换、编码器调频率、AD2 验证） |

按键角色：**S0**=上移、**S1**=下移、**S2**=确认、**PUSH**=返回

> 已从 G3519 移除的模块：K230 视觉模块、CCD 线阵传感器、DRV8874 电机驱动、MPU6050 陀螺仪。
> 这些模块保留在 G3519 综合平台或 G3519_control 控制题工程中。

## 信号题核心模块

### AD5933 阻抗分析仪

- I2C1 接口（PA29-SCL, PA30-SDA），100kHz
- 支持温度读取、扫频测量、GainFactor 标定
- 硬件：J15/J19 桥接，需确认 R31(100ohm TIA 反馈电阻) 已焊接
- 详见 [`docs/development_reference/AD5933_Use.md`](empty_mspm0g3519/docs/development_reference/AD5933_Use.md)

### AD9833 DDS 波形发生器

- GPIO bit-bang（PC2-SCLK, PC3-SDATA, PC24-FSYNC）
- 方波/正弦/三角波，100Hz~50kHz
- 详见 [`docs/development_reference/AD9833_DDS_Use.md`](empty_mspm0g3519/docs/development_reference/AD9833_DDS_Use.md)

## 硬件连接

| 项目 | 说明 |
|---|---|
| **主控** | TI MSPM0G3519SPZR（100 引脚 LQFP） |
| **LED** | D1 接 **PB5**（物理引脚 26），**低电平点亮** |
| **蜂鸣器** | PA13，有源蜂鸣器，高电平驱动 |
| **TFT LCD** | ST7735 160x128，SPI1（PB30/PB31/PB14），10MHz |
| **按键** | S0(PA18)、S1(PC0)、S2(PA16)、PUSH(PA12) |
| **编码器** | PHA0(PA14, 双边沿中断)、PHB0(PA15) |
| **I2C1** | AD5933：PA29(SCL)/PA30(SDA)，100kHz |
| **DDS** | AD9833：PC2(SCLK)/PC3(SDATA)/PC24(FSYNC) |
| **UART0** | 调试：MFCLK 4MHz, 115200-8N1，PA10(TX)/PA11(RX) |
| **调试器** | DAPLink (CMSIS-DAP v2) |
| **供电** | USB-C，禁止多路同时供电 |

## 开发环境

| 工具 | 版本 |
|------|------|
| **IAR EWARM** | 9.60.3 |
| **TI MSPM0 SDK** | 2.10.00.04（默认 `C:\ti\mspm0_sdk_2_10_00_04`） |
| **SysConfig** | 1.28.0 |

首次配置仅两个关键点：

1. **IAR 全局变量**：`Tools → Configure Custom Argument Variables` → **Global** 选项卡添加
   `MSPM0_SDK_INSTALL_DIR` = SDK 路径、`SYSCONFIG_ROOT` = SysConfig 路径
2. **Flash loader 修复**（仅 DAPLink 需要）：`<IAR>\arm\config\flashloader\TexasInstruments\FlashMSPM0GX51X.mac` 第 63 行附近改为
   `} else if(__driverType("ijet") || __driverType("cmsisdap")) {`

## 编译与烧录

双击 `empty_mspm0g3519_nortos_iar.eww` → 确认 `Project → Options → Debugger → Driver` = **CMSIS-DAP**（SWD、1000kHz、Hardware Reset）→ `F7` 编译 → `Ctrl+D` 烧录调试 → `F5` 运行。

## 已知问题

| 问题 | 现象 | 解决 |
|------|------|------|
| **UART0/printf 脱机阻塞** | 不接 DAPLink 时程序死在 `DL_UART_transmitDataBlocking()` | NRST=2.5V 导致 MFCLK 不稳定；已从 main() 移除 UART0 初始化 |
| PHA0 编码器噪声 | 未接编码器时光标抖动 | 已在 `tsp_encoder_init` 中默认禁用 PHA0 中断 |
| Flash loader Device ID 不匹配 | DAPLink 烧录失败 | 按上文步骤 2 修复 Flash loader |
| 设备锁定警告 | 首次下载弹出 "Device is locked" | 点 Yes/OK 执行 Mass Erase |

> **脱机=仅 USB-C 供电、不接 DAPLink 排线。** 接 DAPLink 时所有功能正常。

## 参考资源

- [TI MSPM0 SDK 文档](https://www.ti.com/tool/MSPM0-SDK)
- [TI MSPM0G3519 数据手册](https://www.ti.com/product/MSPM0G3519)
- [IAR EWARM 用户指南](https://www.iar.com/support/user-guides/)
