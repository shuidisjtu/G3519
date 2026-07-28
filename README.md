# MSPM0G3519 信号题专用平台

从 [G3519](../G3519) 综合平台拆分，专注 NUEDC-2026 电赛**信号题**开发。主控 TI MSPM0G3519SPZR（Arm Cortex-M0+, 80MHz），IAR EWARM + DAPLink (CMSIS-DAP)。

## 功能概览

启动后播放开机动画（色彩测试 → 启动信息 → LED 闪烁 + 蜂鸣器短响），然后进入 TFT LCD 菜单界面：

| 菜单项 | 功能 |
|---|---|
| **AD5933 Test** | AD5933 阻抗测量（I2C1 通信验证、GainFactor 标定、阻抗测量） |
| **DDS Test** | AD9833 DDS 波形发生器（方波/正弦/三角波切换、编码器调频率） |
| **ADC Test** | 通用 ADC 五路采集（J2 VIN1~VIN5 电压显示 + 频率测量 + 通道切换） |
| **Scope** | LCD 波形显示器（三档时基、上升沿触发、自动量程、差分更新无闪烁） |
| **Sweep** | 扫频分析仪（DDS+ADC 联动 Cal/Meas/View 三阶段：校准→测量→增益/相位曲线） |
| **PGA Test** | MCP41010 数字电位器程控 AD620 增益（GPIO bit-bang SPI, 编码器调节） |

按键角色：**S0**=上移、**S1**=下移、**S2**=确认、**PUSH**=返回

同时在后台运行 **UART 文本命令协议**（115200-8N1），支持串口终端直接发送 `VER?`、`ADC,1`、`DDS,1000,SINE`、`FFT,1,MED` 等命令。

### TJC 4.3 寸串口屏（外接）

- TJC4827X543_011C（X5 系列，480×272 触摸屏），通过 UART3 (J4, PC6 TX/PC7 RX) 通信
- USART HMI 工程文件位于 `empty_mspm0g3519/TJC4827X543_011/`
- 核心技能已验证：按钮事件(0x65)、页面跳转、数值/文本更新、addt 波形透传
- 硬件注意：J4 Pin1 (5V) 带载不足，TJC 屏需从其他 5V 接口独立供电
- 详见 [`docs/development_reference/TJC4827X543_011C_Use.md`](empty_mspm0g3519/docs/development_reference/TJC4827X543_011C_Use.md)

> 已从 G3519 移除的模块：K230 视觉模块、CCD 线阵传感器、DRV8874 电机驱动、MPU6050 陀螺仪。
> 这些模块保留在 G3519 综合平台或 G3519_control 控制题工程中。

## 信号题核心模块

### AD5933 阻抗分析仪

- I2C1 接口（PA29-SCL, PA30-SDA），100kHz
- 支持温度读取、扫频测量、GainFactor 标定（4 次平均，300KΩ 标定电阻）
- 激励 2Vpp，RFB=20KΩ，对标参考设计（需换 RFB 后验证）
- 详见 [`docs/development_reference/AD5933_Use.md`](empty_mspm0g3519/docs/development_reference/AD5933_Use.md)

### AD9833 DDS 波形发生器

- GPIO bit-bang（PC2-SCLK, PC3-SDATA, PC24-FSYNC）
- 方波/正弦/三角波，100Hz~50kHz
- 详见 [`docs/development_reference/AD9833_DDS_Use.md`](empty_mspm0g3519/docs/development_reference/AD9833_DDS_Use.md)

### 通用 ADC（J2 五路模拟输入）

- ADC0 + ADC1，ULPCLK 40MHz，12-bit 轮询模式
- VIN1~VIN5 五通道：电压显示（0~3300mV）、频率测量（~10Hz~165kHz）、burst 采样
- 各路带 49.9Ω + 220pF 抗混叠滤波

### Scope 波形显示

- 160×96 像素图形区域，三档时基（FAST/MED/SLOW）
- 上升沿触发、自动量程（±5% 余量，噪底保护）
- 差分更新渲染（仅擦旧画新，无全屏清除闪烁）

### Sweep 扫频分析仪

- DDS + ADC 联动，80 点对数扫频（100Hz → ~50kHz，8.2%/步）
- Cal/Meas/View 三阶段工作流：校准（DDS→ADC 直连）→ 测量（DDS→DUT→ADC）→ 查看
- 增益归一化：Vpp_meas / Vpp_cal，消除 DDS 频响误差，显示 "G:XXX.X% @XXXXXHz"
- 相位测量：单频 DFT 提取相位，相位差 unwrap，±180° 曲线显示
- 相位相干采样：DDS reset 相位归零 + 中断屏蔽确保确定性时序
- 垂直频率标记（200/500/1k/2k/5k/10k/20kHz，1-2-5 序列，1k/10k 带标签）

### FFT 频谱分析

- CMSIS-DSP `arm_cfft_q15`，256 点
- 频率（抛物线插值 sub-bin 精度）、幅值 mV（时域 Vpp/2）、THD（2~8 次谐波）
- 可变采样率：FAST(~204kSPS) / MED(~5kSPS) / SLOW(~1.2kSPS)

### UART 文本命令协议

- **UART0→PC**：MFCLK 4MHz, 115200-8N1, PA10(TX)/PA11(RX), TX 10ms 超时保护（脱机安全）
- **UART3→TJC 屏**：BUSCLK 80MHz, 115200-8N1, PC6(TX)/PC7(RX), J4，TJC X5 串口屏通信已验证
- **UART6→K230**：BUSCLK 80MHz, 115200-8N1, PC11(TX)/PC10(RX), J11（待硬件验证）
- 命令格式：`CMD[,PARAM]\r\n` → `OK[,DATA]\r\n` / `ERR,msg\r\n`
- 支持命令：VER?, ADC, FREQ, DDS, FFT

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
| **ADC0** | J2：PA25(VIN1/CH2), PA24(VIN3/CH3), PB24(VIN4/CH5) |
| **ADC1** | J2：PB23(VIN2/CH11), PA23(VIN5/CH12) |
| **UART0** | PC 调试：MFCLK 4MHz, 115200-8N1，PA10(TX)/PA11(RX)，10ms 超时 |
| **UART3** | TJC 串口屏：BUSCLK 80MHz, 115200-8N1，PC6(TX)/PC7(RX)，J4 |
| **UART6** | K230：BUSCLK 80MHz, 115200-8N1，PC11(TX)/PC10(RX)，J11 |
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
| **AD5933 待验证** | 已对标参考设计（2Vpp + 300KΩ标定 + 4次平均） | 需换 RFB=20KΩ 后验证 |
| PHA0 编码器噪声 | 未接编码器时光标抖动 | 已在 `tsp_encoder_init` 中默认禁用 PHA0 中断 |
| Flash loader Device ID 不匹配 | DAPLink 烧录失败 | 按上文步骤 2 修复 Flash loader |
| 设备锁定警告 | 首次下载弹出 "Device is locked" | 点 Yes/OK 执行 Mass Erase |

> **脱机=仅 USB-C 供电、不接 DAPLink 排线。** UART0 TX 已加 10ms 超时保护，脱机启动正常，printf 静默失败不卡死。

## 参考资源

- [TI MSPM0 SDK 文档](https://www.ti.com/tool/MSPM0-SDK)
- [TI MSPM0G3519 数据手册](https://www.ti.com/product/MSPM0G3519)
- [IAR EWARM 用户指南](https://www.iar.com/support/user-guides/)
