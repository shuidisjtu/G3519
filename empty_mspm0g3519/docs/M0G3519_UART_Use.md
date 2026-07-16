# M0G3519 UART 使用说明

## 1. 适用范围

本文基于以下三份资料整理：

1. `M0G3519 电路图.pdf`：用于确认开发板上的 UART 网络、连接器和调试接口。
2. `MSPM0G351x、MSPM0G151x、MSPM0G351x-Q1、MSPM0G3529-Q1 微控制器.pdf`：用于确认 MSPM0G3519 的器件资源和 UART 复用信息。
3. `MSPM0Gx51x 具有 CAN-FD 接口的混合信号微控制器.pdf`：用于确认 UART 外设特性、电气限制和引脚属性。

页码以 PDF 中印刷页码为准；若阅读器页码与印刷页码不一致，请以章节标题定位。

## 2. 器件 UART 资源

MSPM0G3519 共提供 7 路 UART：

| UART 实例 | 资料中的定位 | 主要扩展能力 |
|---|---|---|
| UART0 | 扩展低功耗 UART | LIN、DALI、IrDA、ISO7816 Smart Card、Manchester、低功耗运行 |
| UART1 | 主 UART，低功耗 | 低功耗运行 |
| UART3-UART6 | 主 UART | 标准异步 UART、FIFO、硬件流控、9 位配置 |
| UART7 | 扩展低功耗 UART | LIN、DALI、IrDA、ISO7816 Smart Card、Manchester、低功耗运行 |

注意：器件型号没有 UART2；编号从 UART1 直接跳到 UART3。

## 3. 板级 UART 引出情况

原理图中实际出现了以下 UART TX/RX 网络：

| 板级网络 | 原理图中的典型用途/位置 | 备注 |
|---|---|---|
| `UART0-TX`、`UART0-RX` | DAPLink 调试接口区域（J1） | 可作为板载调试器提供的虚拟串口使用，具体是否连通取决于 DAPLink 固件和跳线/连接状态 |
| `UART3-TX`、`UART3-RX` | 扩展排针区域，和 UART7 共用一组候选引脚 | 与 UART7 不能在同一对引脚上同时启用 |
| `UART7-TX`、`UART7-RX` | 扩展排针区域，和 UART3 共用一组候选引脚 | 适合需要 LIN 或低功耗唤醒的场景 |
| `UART6-TX`、`UART6-RX` | 扩展排针区域（J19/J20 信号组） | 与其他外设复用，使用前应确认没有启用冲突功能 |
| `UART1-TX`、`UART1-RX` | MCU 引脚复用表中可选 | 原理图中未见独立的 UART1 收发器 |
| `UART4-TX`、`UART4-RX` | MCU 引脚复用表中可选 | 原理图中未见独立的 UART4 收发器 |
| `UART5-TX`、`UART5-RX` | MCU 引脚复用表中可选 | 原理图中未见独立的 UART5 收发器 |

从原理图看，UART 信号为 MCU GPIO 直接引出，板上未见 MAX3232、RS-232 收发器或 RS-485 收发器。因此这些信号应按 3.3 V 单端逻辑 UART 使用：

- 外部设备 TX 接 MCU RX；外部设备 RX 接 MCU TX；两端 GND 必须相连。
- 不要把 RS-232 电压直接接入 MCU 引脚。
- 不要把 CANH/CANL 或 RS-485 A/B 差分线直接接入 UART 引脚。
- 外部设备电平超过 MCU 供电电压时，必须增加电平转换。

## 4. 推荐引脚复用

以下是数据手册中可直接使用、且便于检查的 UART TX/RX 组合。一个 UART 实例可以有多个候选位置，但同一 GPIO 不能同时承担两个外设功能。

| UART | TX 引脚候选 | RX 引脚候选 | 典型 IOMUX 选择 |
|---|---|---|---|
| UART0 | PA0 | PA1 | `PA0 -> UART0_TX`，`PA1 -> UART0_RX` |
| UART1 | PA3、PB21 | PA4、PB22 | 选择同一组 TX/RX，避免与其他已用功能冲突 |
| UART3 | PA14、PB12 | PA13、PB13 | `PA14 -> UART3_TX`，`PA13 -> UART3_RX` |
| UART4 | PB10、PB21 | PB11、PB22 | `PB10/PB11` 是一组常用选择 |
| UART5 | PA1、PB29 | PA0、PB28 | `PA1 -> UART5_TX`，`PA0 -> UART5_RX` |
| UART6 | PB22、PB29 | PB21、PB28 | `PB22 -> UART6_TX`，`PB21 -> UART6_RX` |
| UART7 | PA13、PA21、PA23、PB15、PB17 | PA14、PA22、PA24、PB16、PB18 | 选择与 TX 对应的同组 RX；以器件封装对应的引脚属性表和 SDK IOMUX 定义为准 |

### UART7 引脚说明

UART7 的可选复用位置较多，且会与 UART3、UART1、SPI 等功能重叠。原理图中 UART3/UART7 信号集中在 PA12-PA15 扩展区域；常见的互补关系是：

- UART3：PA14 为 TX、PA13 为 RX；
- UART7：PA13 为 TX、PA14 为 RX。

最终配置必须以所使用封装的 `PINCM`/IOMUX 表和 SDK 生成的引脚配置为准。不要仅凭 GPIO 名称配置 UART7。

## 5. UART 外设能力

UART 支持以下通用功能：

- 异步串行通信：起始位、数据位、校验位和停止位。
- 数据位：5、6、7 或 8 位。
- 校验：偶校验、奇校验、固定校验或无校验。
- 停止位：1 或 2 位。
- 可编程波特率，过采样率支持 16、8 和 3。
- 独立发送 FIFO 和接收 FIFO，深度为 4。
- 硬件流控 CTS/RTS。
- 9 位通信配置。
- 接收输入抗干扰滤波器。
- 发送/接收环回模式。
- DMA 数据传输。
- UART0/UART7 支持 LIN；扩展低功耗 UART 还支持 DALI、IrDA、ISO7816 Smart Card 和 Manchester 编码。

## 6. 关键速率限制

数据手册给出的 UART 电气限制为：

| 项目 | 电源域 1 UART | 电源域 0 UART |
|---|---:|---:|
| UART 输入时钟最大值 | 80 MHz | 40 MHz |
| BITCLK/波特率最大值 | 10 MBaud | 5 MBaud |

实际可用波特率还受以下因素限制：

1. UART 输入时钟是否已经正确配置。
2. 过采样率选择。
3. RX 输入抗干扰滤波时间；滤波时间越长，对高波特率越不利。
4. 线长、负载、边沿质量和外部收发器延迟。

普通板级调试建议先使用 115200、8 数据位、无校验、1 停止位（115200 8N1）。高波特率使用前应结合时钟树和示波器实测确认。

## 7. 初始化流程

建议按下面顺序初始化：

1. 确认 UART 实例、TX/RX 引脚和封装对应的 IOMUX 复用值。
2. 使能对应 UART 外设时钟和电源域。
3. 配置 UART 输入时钟频率。
4. 设置波特率、过采样率、数据位、校验位和停止位。
5. 配置 TX/RX 引脚的数字输入/输出属性和驱动能力。
6. 如使用 CTS/RTS，额外配置四根信号线；否则关闭硬件流控。
7. 清空 FIFO，并清除挂起的错误/中断标志。
8. 使能 UART、发送器和接收器。
9. 发送固定测试字符串，使用逻辑分析仪或串口工具确认 TX/RX 电平和帧格式。

## 8. DMA、中断和 FIFO 使用建议

- 短报文、低速控制命令：使用轮询或接收中断即可。
- 连续数据流：使用 RX FIFO + DMA，避免 CPU 按字节处理中断。
- 发送数据前检查 TX FIFO 空间，或使用发送 FIFO 中断/DMA。
- 接收处理应检查帧错误、奇偶校验错误、溢出和线路中断状态。
- 环形缓冲区应由应用层维护；UART FIFO 深度只有 4，不能替代较大的软件缓冲区。

## 9. 常见复用冲突

原理图和引脚表显示 UART 引脚通常还复用了 SPI、I2C、定时器、ADC 或 CAN 相关功能。例如：

- PA0/PA1 同时可用于 UART0 和 UART5。
- PA13/PA14 同时可用于 UART3、UART7 及其他外设。
- PB10/PB11 同时可用于 UART4 和定时器/SPI。
- PB21/PB22 同时可用于 UART4、UART1、UART6 或 CAN1。
- PB28/PB29 同时可用于 UART5、UART6 及 I2C/SPI/定时器。

因此，配置 UART 前应先列出整板已占用的外设，选择不冲突的 TX/RX 组合；仅配置 TX 或 RX 的一半通常会导致发送正常但接收无效，或反之。

## 10. 调试排查清单

### 无输出

- 检查 UART 外设时钟、电源域和模块使能。
- 检查 TX 引脚是否被正确切换到 UART 功能，而不是 GPIO 或其他外设。
- 检查串口工具的波特率、数据位、校验和停止位。
- 检查 GND、供电电压和连接器针脚方向。

### 能发送但不能接收

- 确认外部 TX 接到了 MCU RX。
- 确认 RX 引脚的 IOMUX 和数字输入已开启。
- 检查是否误启用了输入滤波、硬件流控或错误的 CTS/RTS。
- 检查接收 FIFO 溢出和帧/奇偶校验错误状态。

### 数据乱码

- 优先检查 UART 输入时钟和波特率分频值。
- 两端必须使用完全一致的帧格式。
- 降低波特率，关闭过长的 RX 抗干扰滤波，检查信号完整性。
- 若通过外部 RS-232/RS-485 收发器，确认收发器方向控制和电平转换逻辑正确。

## 11. G3519 平台已验证配置

以下配置在 NUEDC-2026 G3519 平台上通过 SSCOM 串口调试助手验证（2026-07-16）：

| 项目 | 配置 |
|------|------|
| UART 实例 | UART0 (PD0, 低功耗域) |
| 时钟源 | MFCLK = 4 MHz（SysConfig 管理） |
| 波特率 | 115200, 8N1, 无流控 |
| TX 引脚 | PA10 (IOMUX_PINCM21) |
| RX 引脚 | PA11 (IOMUX_PINCM22) |
| PC 端 | DAPLink 虚拟串口 (COM11) |

### 关键注意事项

1. **UART0 在 PD0 域**，最大输入时钟 40 MHz。不可直接用 BUSCLK (80 MHz) 作为时钟源。
2. **SysConfig 管理时钟和引脚**：手动 `DL_UART_setClockConfig(BUSCLK)` 会失败，应在 `.syscfg` 中添加 UART 模块并选用 MFCLK。
3. **RX 中断按需开启**：`tsp_uart_init()` 不启用 RX 中断，需调用 `tsp_uart_rx_enable()` 后才接收。防止浮空引脚产生中断风暴（与编码器 PHA0 问题同根因）。
4. **波特率调整**：SysConfig 预设 9600，应用层通过 `DL_UART_configBaudRate(uart, 4000000, 115200)` 覆盖。

## 12. 资料依据

- `M0G3519 电路图.pdf`，第 1 页：MCU 引脚复用、DAPLink 区域及 UART 网络/排针引出。
- `MSPM0G351x、MSPM0G151x、MSPM0G351x-Q1、MSPM0G3529-Q1 微控制器.pdf`：器件资源、引脚复用和 UART 章节。
- `MSPM0Gx51x 具有 CAN-FD 接口的混合信号微控制器.pdf`，第 76-77、93-94 页：UART 电气特性和外设特性；第 6 章引脚属性表：UART IOMUX 选项。

本文是基于上述资料的板级使用整理，不替代 MSPM0G 系列技术参考手册。若使用 DriverLib，应以对应 SDK 版本生成的 `SYSCFG`/IOMUX 配置和芯片封装定义为最终依据。
