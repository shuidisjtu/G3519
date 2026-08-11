# MSPM0G3519 NUEDC-2026 电赛平台

基于 TI **MSPM0G3519SPZR**（Arm Cortex-M0+, 80MHz）的电赛开发平台。NUEDC-2026（SAIS@SJTU）比赛结束后重构为**共享平台库 + 双题目应用**结构，比赛成果与硬件资料全部归档于本仓库，共享代码供后续题目复用。

## 目录结构

```
G3519/
├── common/          ← 共享平台库（物理一份）
│   ├── TSP3519/     ← 板级支持库：gpio/lcd/ccd/menu
│   ├── NUEDC2025/   ← 基础驱动：isr/key/encoder/uart/uart_k230
│   ├── drivers/     ← 通用外设驱动：motor/mpu6050/pid/ad5933/dds/adc/uart3
│   └── docs/        ← 共享文档（3519_hardware 硬件手册、主板资料、外设用法、datasheets/ 手册库）
├── control/         ← 控制题应用：IAR 工程 + app（循迹/里程计/K230/轮编码器）+ 特有文档
└── signal/          ← 信号题应用：IAR 工程 + app（FFT/示波器/TJC 屏/命令协议）+ 特有文档
```

架构约束（依赖方向、稳定 API 契约、防腐原则）见 [`ARCHITECTURE.md`](ARCHITECTURE.md)。

## 两套 IAR 工程

| 题目 | 工程文件 | 内容 |
|---|---|---|
| 控制题 | `control/iar/empty_mspm0g3519_nortos_iar.ewp` | 小车/CCD 循迹/电机闭环/里程计/MPU6050/K230 |
| 信号题 | `signal/iar/empty_mspm0g3519_nortos_iar.ewp` | AD5933/DDS/ADC/FFT/示波器/TJC 屏/命令协议 |

编译（命令行）：
```
"D:/iar/ewarm-9.60.3/common/bin/iarbuild.exe" control/iar/empty_mspm0g3519_nortos_iar.ewp -build Debug
"D:/iar/ewarm-9.60.3/common/bin/iarbuild.exe" signal/iar/empty_mspm0g3519_nortos_iar.ewp -build Debug
```

## 硬件平台

| 项目 | 说明 |
|---|---|
| **主控** | TI MSPM0G3519SPZR（100 引脚 LQFP），80MHz，IAR EWARM 9.60.3 + DAPLink |
| **LCD** | ST7735 160×128，SPI1（PB30/PB31/PB14），10MHz |
| **按键** | S0(PA18)、S1(PC0)、S2(PA16)、PUSH(PA12) |
| **旋钮编码器** | PHA0(PA14)/PHB0(PA15)，菜单参数微调 |
| **电机** | DRV8874×2（拓展板），TIMA0 双 PWM 20kHz（PB3/PB4/PB0/PB2），nSLEEP=PB1，nFAULT=PA7，需 VBAT(SW1) |
| **轮编码器** | TIMG8/TIMG9 硬件 QEI（PB15/PB16、PB7/PB9），J12 右轮/J13 左轮 |
| **CCD** | 128 像素线阵 CCD×4，ADC1 序列采样 |
| **MPU6050** | I2C0（PB21-SCL/PB22-SDA），Yaw 积分 |
| **AD5933** | I2C1（PA29/PA30），阻抗分析 |
| **AD9833 DDS** | PC2/PC3 bit-bang [封存：与 CCD 共用引脚] |
| **通用 ADC** | J2 五通道（PA25/PA24/PB24/PB23/PA23），ADC0+ADC1 |
| **TJC 屏** | UART3/J4（PC6/PC7），信号题 HMI |
| **K230 视觉** | UART6/J11（PC11/PC10），YbProtocol |
| **调试串口** | UART0（PA10/PA11），MFCLK 4MHz，115200 |

## 已知问题（历史记录）

| 问题 | 现象 | 解决 |
|---|---|---|
| UART0/printf 脱机阻塞 | 不接 DAPLink 时程序死在 `DL_UART_transmitDataBlocking()` | 已修复：`tsp_uart_*` 全部改为 10ms 超时 TX（2026-07-26 起） |
| K230 固件 | `control/sdcard/` 为 K230 固件（已入库），修改固件需按 `common/docs/K230_Vision_Module_Use.md` | — |

## 参考资料

- 硬件手册/电路图/管脚图：`common/docs/3519_hardware/`
- 各外设用法：`common/docs/*.md`
- 芯片官方手册库（mcu/peripherals/instruments，约 40 MB）：`common/docs/datasheets/`，索引见 `common/docs/README.md`
- K230 固件参考（YbProtocol）：`common/docs/k230_firmware_ref/`
- 控制题/信号题比赛记录：`control/docs/`、`signal/docs/`
- 2026 G 题官方赛题原文与模拟前端设计存档：`signal/docs/2026_G_Official_Topic.md`、`signal/docs/2026_Analog_Frontend_Design.md`
- [TI MSPM0 SDK 文档](https://www.ti.com/tool/MSPM0-SDK)
