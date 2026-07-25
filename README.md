# MSPM0G3519 Control Topic Platform

基于 TI **MSPM0G3519SPZR**（Arm Cortex-M0+, 80MHz）的**控制题专用**开发平台，从 [G3519 基础平台](https://github.com/shuidisjtu/G3519) 拆分而来，专注小车/控制类赛题（NUEDC-2026 SAIS@SJTU）。

集成 K230 视觉导航、CCD 循迹、DRV8874 双电机驱动，使用 IAR EWARM + DAPLink (CMSIS-DAP) 开发工具链。

## 功能概览

启动后播放**开机动画**（色彩测试 -> 启动信息 -> LED 闪烁 + 蜂鸣器短响），然后进入 **TFT LCD 菜单界面**（4 项）：

| 菜单项 | 功能 |
|---|---|
| **K230 Test** | K230 视觉模块测试（UART6/J11 双向通信：接收 YbProtocol 颜色帧 + S0 发 $SWITCH# 切换颜色阈值，LCD 色块追踪画框） |
| **CCD Test** | 线阵 CCD 测试（128 像素采集 + LCD 实时波形、连续/单拍模式、双通道切换、曝光调节） |
| **Motor Test** | DRV8874 双电机驱动测试（TIMA0 PWM 20kHz，M1/M2 独立控制，正反转+占空比调节） |
| **MPU6050 Test** | 六轴 IMU 测试（I2C0 轮询：加速度/陀螺仪原始数据 + Yaw 航向角积分） |

> **Motor Test 操作**：S0/S1 调占空比(+-5%)、S2 切换方向(FWD/REV)、编码器左转=M2 右转=M1、PUSH 退出。
> 必须打开 SW1 接通电池（VBAT）才能在输出端看到 PWM 波形。

按键角色：**S0**=上移、**S1**=下移、**S2**=确认、**PUSH**=返回

> **暂未启用**：编码器驱动已初始化但未加入独立菜单项。

## 开发环境

| 工具 | 版本 |
|------|------|
| **IAR EWARM** | 9.60.3 |
| **TI MSPM0 SDK** | 2.10.00.04（默认 `C:\ti\mspm0_sdk_2_10_00_04`） |
| **SysConfig** | 1.28.0 |

首次配置仅两个关键点：

1. **IAR 全局变量**：`Tools -> Configure Custom Argument Variables` -> **Global** 选项卡添加
   `MSPM0_SDK_INSTALL_DIR` = SDK 路径、`SYSCONFIG_ROOT` = SysConfig 路径
2. **Flash loader 修复**（仅 DAPLink 需要）：`<IAR>\arm\config\flashloader\TexasInstruments\FlashMSPM0GX51X.mac` 第 63 行附近改为
   `} else if(__driverType("ijet") || __driverType("cmsisdap")) {`

## 编译与烧录

双击 `empty_mspm0g3519_nortos_iar.eww` -> 确认 `Project -> Options -> Debugger -> Driver` = **CMSIS-DAP**（SWD、1000kHz、Hardware Reset）-> `F7` 编译 -> `Ctrl+D` 烧录调试 -> `F5` 运行。

## 硬件连接

| 项目 | 说明 |
|---|---|
| **主控** | TI MSPM0G3519SPZR（100 引脚 LQFP） |
| **LED** | D1 接 **PB5**（物理引脚 26），**低电平点亮** |
| **蜂鸣器** | PA13，有源蜂鸣器，高电平驱动 |
| **TFT LCD** | ST7735 160x128，SPI1（PB30/PB31/PB14），10MHz |
| **按键** | S0(PA18)、S1(PC0)、S2(PA16)、PUSH(PA12) |
| **编码器** | PHA0(PA14, 双边沿中断)、PHB0(PA15) |
| **CCD** | 128 像素线阵 CCD 4 通道 2 组，SI/CLK GPIO + ADC1 序列采样（SysConfig） |
| **电机驱动** | DRV8874x2（拓展板），TIMA0 PWM 20kHz，需 VBAT(SW1) |
| **MPU6050** | 六轴 IMU（I2C0: PB21-SCL/PB22-SDA, 中断 PC8），驱动待开发 |
| **UART** | UART0 调试：PA10/PA11；UART6->K230：PC10/PC11，J11 |
| **调试器** | DAPLink (CMSIS-DAP v2) |
| **供电** | USB-C，禁止多路同时供电 |

## K230 视觉模块

- **接线**：J11 排座，UART6（PC11-TX / PC10-RX），115200-8N1
- **协议**：YbProtocol（`$...#` 帧格式），双向通信已验证
- **K230 固件**：修改原厂 SD 卡固件 `sdcard/apps/color_det/color_recog/color_recognition.py`，在颜色识别循环中加入 UART TX/RX
- **测试脚本**：`empty_mspm0g3519/k230_scripts/` 中有早期链路测试脚本（CanMV IDE 中运行）
- **详细文档**：`docs/development_reference/K230_Vision_Module_Use.md`

### sdcard 目录

`sdcard/` 存放 K230 SD 卡固件，已在 `.gitignore` 中排除，不纳入版本控制。需手动从原 G3519 项目复制或从亚博官方获取。

## 工程结构

```
G3519_control/
├── empty_mspm0g3519/
│   ├── iar/                             <- 工程根目录（$PROJ_DIR$）
│   │   ├── empty_mspm0g3519.c           <- 主程序（开机动画 + 3 项菜单）
│   │   ├── empty_mspm0g3519.syscfg      <- SysConfig 引脚配置
│   │   └── ti_msp_dl_config.c/.h        <- SysConfig 生成（勿手动编辑）
│   ├── TSP3519/                          <- 板级支持库（LCD/GPIO/CCD/菜单）
│   ├── NUEDC2025/                        <- 应用层驱动（电机/K230/编码器/按键/UART/ISR）
│   ├── docs/                             <- 硬件文档与项目进度
│   └── k230_scripts/                     <- K230 MicroPython 测试脚本
└── k230_scripts/                         <- K230 脚本（根目录副本）
```

## 已知问题

| 问题 | 现象 | 解决 |
|------|------|------|
| Flash loader Device ID 不匹配 | 使用 DAPLink 烧录失败 | 修复 FlashMSPM0GX51X.mac（见上文） |
| **UART0/printf 脱机阻塞** | 不接 DAPLink 时程序死在 `DL_UART_transmitDataBlocking()` | NRST=2.5V 导致 MFCLK 不稳定；已从 main() 移除 `tsp_uart_init()` |
| PHA0 编码器噪声 | 未接编码器时光标抖动 | 已在 `tsp_encoder_init` 中默认禁用 PHA0 中断 |
| 电机输出无波形 | AD2 看不到 PWM | 必须打开 SW1 接通 VBAT；无 VBAT 时接 J14 侧 IN1/IN2 看 3.3V 逻辑 |
| 设备锁定警告 | 首次下载弹出 "Device is locked" | 点 Yes/OK 执行 Mass Erase 即可 |

> **脱机 = 仅 USB-C 供电、不接 DAPLink 排线。** 接 DAPLink 时所有功能正常。

## 参考资源

- [TI MSPM0 SDK 文档](https://www.ti.com/tool/MSPM0-SDK)
- [TI MSPM0G3519 数据手册](https://www.ti.com/product/MSPM0G3519)
- [IAR EWARM 用户指南](https://www.iar.com/support/user-guides/)
