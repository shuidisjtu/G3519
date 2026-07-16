# MSPM0G3519 学校开发板 UART 使用说明

版本：V2.0（合并已验证配置与调试清单）  
适用对象：以 MSPM0G3519SPZR 为核心的学校开发主板，以及配套 `Car_2Motor` 拓展板

## 1. 结论速览

主板上最适合连接 PC 调试终端的串口是 **UART0**：

| 信号 | MCU 引脚 | 主板 DAPLink 插座 J1 | 说明 |
|---|---:|---:|---|
| UART0-RX | PA11 | 2 | MCU 接收，连接 USB-UART/DAPLink 的 TX |
| UART0-TX | PA10 | 4 | MCU 发送，连接 USB-UART/DAPLink 的 RX |
| GND | - | 8 | 与外部串口设备共地 |
| MCU_3V3 | - | 10 | 3.3 V 逻辑电源参考/供电 |

J1 同时引出 SWD 和复位信号。主板另设 BSL 按键/相关电路。J1 上的 UART0-RX/UART0-TX 是 **3.3 V TTL 电平**，不是 RS-232 电平，也不是 CAN 总线信号；连接传统 RS-232 设备必须增加电平转换器。

配套拓展板的原理图未显示独立的 UART 收发器或 UART 专用接口。拓展板主要使用电机驱动、舵机、LCD、编码器、按键和蜂鸣器等信号，因此 UART 通信应优先从主板 J1 或主板排针引出，不要把拓展板上的电机/LCD接口直接当作串口接口。

## 2. 硬件连接

### 2.1 连接 PC 串口终端

使用带 3.3 V 电平的 USB-UART 模块时，连接方式如下：

| USB-UART 模块 | 主板 J1 |
|---|---:|
| TXD | 2（UART0-RX） |
| RXD | 4（UART0-TX） |
| GND | 8 |
| VCC（可选） | 10（MCU_3V3，仅在确认模块允许外部供电时连接） |

TX 与 RX 必须交叉连接。若主板已经由其他方式供电，通常只连接 TXD、RXD、GND，避免两个 3.3 V 电源直接并联。

> **本项目实际使用**：通过 DAPLink 的虚拟串口连接，无需外接 USB-UART 模块。DAPLink 通过 J1 的 SWD + UART0 同时实现调试和串口通信。

### 2.2 J1 其他针脚

J1 为 2.54 mm、10 针调试/下载接口。原理图标注的主要信号为：

| 针脚 | 信号 |
|---:|---|
| 1 | RST |
| 2 | UART0-RX |
| 3 | NC |
| 4 | UART0-TX |
| 5 | SWCLK |
| 6 | NC |
| 7 | SWDIO |
| 8 | GND |
| 9 | NC |
| 10 | MCU_3V3 |

### 2.3 拓展板使用注意

`Car_2Motor.pdf` 中的 J14 是主板/拓展板间的多针信号连接器，已分配给 `M1/M2` 电机、舵机、LCD、编码器、按键和蜂鸣器等信号，原理图没有标注 UART0-RX/UART0-TX 专用通道。使用拓展板时，UART0 仍通过主板 J1 使用；若需要第二路串口，应从主板的 GPIO 排针选择未被拓展板占用的 UART 复用引脚。

## 3. MCU UART 资源与引脚复用

### 3.1 器件 UART 资源

MSPM0G3519 共提供 7 路 UART（注意：没有 UART2）：

| UART 实例 | 类型 | 主要扩展能力 |
|---|---|---|
| UART0 | 扩展低功耗 UART (PD0) | LIN、DALI、IrDA、ISO7816 Smart Card、Manchester、低功耗运行 |
| UART1 | 主 UART，低功耗 | 低功耗运行 |
| UART3-UART6 | 主 UART (PD1) | 标准异步 UART、FIFO、硬件流控、9 位配置 |
| UART7 | 扩展低功耗 UART (PD0) | LIN、DALI、IrDA、ISO7816 Smart Card、Manchester、低功耗运行 |

### 3.2 引脚复用候选

每个 UART 的 TX/RX 都需要在 GPIO 分配中选择对应的复用功能；同一个 GPIO 不能同时用于 UART 和电机、SPI、I2C、ADC 等其他功能。

主板原理图中可直接确认的典型复用组合如下。表中"TX/RX"均按 MCU 视角描述：

| UART | TX 引脚候选 | RX 引脚候选 | 备注 |
|---|---|---|---|
| UART0 | PA10 | PA11 | 已连接到 J1，**推荐用于 PC 调试** |
| UART1 | PA3、PA5、PB4、PB6、PA8、PA17、PC0、PB21 | PA4、PA6、PB5、PB7、PA9、PA18、PC1、PB22 | 需按实际排针和拓展板占用情况选择 |
| UART3 | PB2、PC3、PC6、PA26 等 | PB3、PC2、PC7、PA25/PA27 等 | 需核对所选 GPIO 的复用表 |
| UART4 | PB10、PB17、PB21 等 | PB11、PB18、PB22 等 | 与其他外设复用明显 |
| UART5 | PA1、PC29、PB29 | PA0、PC28、PB28 | 主板原理图明确标注 PA0/PA1、PC28/PC29 通道 |
| UART6 | PC11 | PC10 | 主板原理图明确标注 PC10/PC11 通道 |
| UART7 | PB15、PA21、PA23、PC3 等 | PB16、PA22、PA24、PC2 等 | 需按实际 GPIO 复用选择 |

> 以上是根据主板原理图和官方引脚复用信息整理出的候选关系；开发时应以工程中实际封装、排针连接和 SysConfig 生成结果为准。

### 3.3 常见复用冲突

UART 引脚通常还复用了 SPI、I2C、定时器、ADC 或 CAN 相关功能。例如：

- PA0/PA1 同时可用于 UART0 和 UART5。
- PA13/PA14 同时可用于 UART3、UART7 及其他外设。
- PB10/PB11 同时可用于 UART4 和定时器/SPI。
- PB21/PB22 同时可用于 UART4、UART1、UART6 或 CAN1。
- PB28/PB29 同时可用于 UART5、UART6 及 I2C/SPI/定时器。

配置 UART 前应先列出整板已占用的外设，选择不冲突的 TX/RX 组合；仅配置 TX 或 RX 的一半通常会导致发送正常但接收无效，或反之。

## 4. 推荐的软件配置

### 4.1 基本参数

建议初始配置为：

```text
外设：UART0
TX：PA10 (IOMUX_PINCM21)
RX：PA11 (IOMUX_PINCM22)
数据位：8
停止位：1
校验：无
流控：无
波特率：115200
方向：发送 + 接收
```

PC 端串口终端设置为 `115200-8-N-1`，关闭硬件流控制。程序启动后先发送固定字符串，例如 `UART0 ready\r\n`，可快速判断 TX 链路是否正常。

### 4.2 初始化流程

1. 在 `.syscfg` 中添加 UART 模块（MFCLK 时钟源），由 SysConfig 管理电源/时钟/引脚配置
2. 通过 `DL_UART_configBaudRate()` 设置目标波特率
3. 使能 UART 和 NVIC
4. 发送测试字符串验证 TX
5. 按需开启 RX 中断进行接收

### 4.3 接收处理策略

```text
RX 中断触发
    -> 读 RX 数据
    -> 写入软件环形缓冲区
    -> 在主循环中按协议解析
```

不要在接收中断中执行长时间阻塞操作。发送数据时可采用 TX FIFO 空闲中断、DMA 或短报文轮询。

## 5. 波特率、时钟和电气限制

### 5.1 PD0/PD1 时钟约束

官方数据手册给出的 UART 输入时钟上限：

| 项目 | 电源域 1 UART | 电源域 0 UART |
|---|---:|---:|
| UART 输入时钟最大值 | 80 MHz | 40 MHz |
| BITCLK/波特率最大值 | 10 MBaud | 5 MBaud |

> **UART0 在 PD0（低功耗域）**，最大输入时钟 40 MHz。不可直接用 BUSCLK (80 MHz) 作为时钟源。本项目使用 MFCLK (4 MHz)，安全且稳定。

### 5.2 乱码排查顺序

1. 确认 PC 端和 MCU 端波特率、数据位、校验和停止位完全一致。
2. 确认 TX/RX 交叉连接且共地。
3. 确认使用 3.3 V TTL USB-UART，而非 RS-232/RS-485 适配器直连。
4. 确认 PA10/PA11 没有被其他复用功能或拓展板信号占用。
5. 用示波器检查 UART0-TX 空闲电平应为高电平，帧起始时出现低电平。
6. 降低到 9600 或 38400 波特率进行链路验证，再恢复到 115200。

## 6. 低功耗和官方勘误注意事项

官方勘误文档对该系列 UART 给出了以下与实际开发相关的限制：

- **UART_ERR_01**：STANDBY1 下连续出现重复 UART 启动条件时，可能漏检后续传输；预计重复唤醒时使用 STANDBY0 或更高的低功耗模式。
- **UART_ERR_02**：仅使能 TXE 时不会产生 EOT 中断；需要 EOT 时同时使能 TXE 和 RXE，即使没有把某个引脚分配为 UART RX。
- **UART_ERR_04**：低功耗下使用 UART 时，应启用 UART 快速时钟请求，避免 SYSOSC 切换到 LFOSC 时接收错误。
- **UART_ERR_07**：IDLE LINE 模式下不要启用 RTOUT。
- **UART_ERR_08**：不要单独依靠 STAT.BUSY 判断 UART 是否忙，应结合 TX FIFO 状态和 CTL0.ENABLE 判断。
- **UART_ERR_11**：接收超时选择值 RXTOSEL 应大于 1，以避免 STOP 位期间过早触发 RTOUT。

对本开发板的普通 PC 调试串口而言，建议保持 RUN/SLEEP 等简单工作模式，暂不启用 9 位多节点、IDLE LINE、RTOUT、IrDA 或 LIN，待基础收发验证通过后再逐项启用。

## 7. 调试排查清单

### 无输出

- 检查 UART 外设时钟、电源域和模块使能（UART0 必须用 PD0 安全时钟，如 MFCLK）。
- 检查 TX 引脚是否被正确切换到 UART 功能，而不是 GPIO 或其他外设。
- 检查串口工具的波特率、数据位、校验和停止位。
- 检查 GND、供电电压和连接器针脚方向。

### 能发送但不能接收

- 确认外部 TX 接到了 MCU RX（交叉连接）。
- 确认 RX 引脚的 IOMUX 和数字输入已开启。
- 检查是否误启用了输入滤波、硬件流控或错误的 CTS/RTS。
- 检查接收 FIFO 溢出和帧/奇偶校验错误状态。
- **G3519 特别注意**：`tsp_uart_init()` 不自动启用 RX 中断，需显式调用 `tsp_uart_rx_enable()`。

### 数据乱码

- **优先检查 UART 输入时钟和波特率分频值**（最常见根因）。
- 两端必须使用完全一致的帧格式。
- UART0 必须用 PD0 合规时钟源（MFCLK / BUSCLK 需分频到 ≤40MHz）。
- 降低波特率，关闭过长的 RX 抗干扰滤波，检查信号完整性。
- 若通过外部 RS-232/RS-485 收发器，确认收发器方向控制和电平转换逻辑正确。

## 8. G3519 平台已验证配置

以下配置在 NUEDC-2026 G3519 平台上通过 SSCOM 串口调试助手验证（2026-07-16）：

| 项目 | 配置 |
|------|------|
| UART 实例 | UART0 (PD0, 低功耗域) |
| 时钟源 | MFCLK = 4 MHz（SysConfig 管理） |
| 波特率 | 115200, 8N1, 无流控 |
| TX 引脚 | PA10 (IOMUX_PINCM21) |
| RX 引脚 | PA11 (IOMUX_PINCM22) |
| 过采样率 | 16X（`DL_UART_configBaudRate` 自动选择） |
| 波特率分频 | IBRD=2, FBRD=11（由 SDK 自动计算） |
| PC 端 | DAPLink 虚拟串口 (COM11) |

### 关键注意事项

1. **UART0 在 PD0 域**，最大输入时钟 40 MHz。不可直接用 BUSCLK (80 MHz) 作为时钟源。
2. **SysConfig 管理时钟和引脚**：应在 `.syscfg` 中添加 UART 模块并选用 MFCLK，手动配置会引入难以排查的错误。
3. **RX 中断按需开启**：`tsp_uart_init()` 不启用 RX 中断，需调用 `tsp_uart_rx_enable()` 后才接收。防止浮空引脚产生中断风暴。
4. **波特率调整**：SysConfig 预设 9600，应用层通过 `DL_UART_configBaudRate(uart, 4000000, 115200)` 覆盖。

## 9. 自检示例

### 9.1 DAPLink 虚拟串口测试（推荐）

本项目使用 DAPLink 自带虚拟串口，无需额外硬件：

1. 连接 DAPLink 到主板 J1（红边对准 RST/1 脚）
2. PC 端打开 SSCOM，选择 DAPLink 虚拟串口（如 COM11），115200-8N1
3. 烧录并运行程序，开机后应收到 `MSPM0G3519 booted`
4. 进入菜单 → UART Test → SSCOM 发送字符 → 确认收到回显

### 9.2 回环测试（硬件验证）

将 J1 的 2（UART0-RX）和 4（UART0-TX）通过跳线短接，打开 PC 端终端：

```text
发送 "hello\r\n"
等待 RX FIFO 收到数据
将收到的每个字节原样发送回去
```

若终端显示原文，说明 MCU UART 配置和主板 J1 走线基本正常；若发送正常但接收失败，优先检查 J1-2、PA11 和 RX 复用配置。

### 9.3 外部设备测试

外部设备必须满足：

- UART 电平与主板 3.3 V GPIO 兼容；
- 两端共地；
- TX/RX 交叉；
- 参数一致；
- 不得把 5 V 信号直接送入 MCU RX。

## 10. 文件依据

1. `M0G3519电路图.pdf`：主控型号、UART0 与 J1 的连接、UART 复用引脚和主板接口标注。
2. `Car_2Motor.pdf`：拓展板 J14 及电机、舵机、LCD、编码器、按键和蜂鸣器等信号分配；未见独立 UART 收发器。
3. `MSPM0Gx51x具有CAN-FD接口的混合信号微控制器.pdf`：UART 数量、功能、FIFO、过采样、时钟和 BSL 说明，重点参考第 7.19、8.24、8.36 节。
4. `MSPM0G351x,MSPM0G151x,MSPM0G351x-Q1,MSPM0G3529-Q1微控制器.pdf`：UART_ERR_01、02、04、05、06、07、08、10、11 勘误及权变措施。

> 本说明仅覆盖由所提供四份文件能够确认的硬件关系。若后续提供 PCB 版本号、排针丝印图或 SDK 工程，应再对每一路 UART 的最终连接器针脚和软件实例名进行版本化确认。使用 DriverLib 时，应以对应 SDK 版本生成的 `SYSCFG`/IOMUX 配置和芯片封装定义为最终依据。
