# 信号题模块参考手册

> 实验室现有信号题相关模块的参数、接口、应用要点与资源索引。
> 按功能分类，供电赛备赛与现场查阅。
>
> 合并自：`电子模块清单.md` + `器件汇总.md`
> 最后更新：2026-07-28

---

## 总览

| 序号 | 模块 | 功能分类 | G3519 平台状态 |
|------|------|----------|---------------|
| 1 | AD9834 DDS | 信号发生 | 可复用 AD9833 驱动（`tsp_dds`），寄存器兼容 |
| 2 | ICL8038 | 信号发生 | 纯模拟，MCU 无需驱动 |
| 3 | AD603 VGA | 增益控制 | 纯模拟 VCA，MCU 可通过 DAC 设控制电压 |
| 4 | AD8367 VGA + AGC | 增益控制 | 纯模拟，MCU 可通过 DAC/ADC 交互 |
| 5 | AD620 / AD623 程控放大器 | 增益控制 | ✅ 已实现（`tsp_mcp41010` 控制 MCP41010 数字电位器） |
| 6 | AD8302 增益/相位检测 | 检测测量 | ⛔ 封存（板载但输入网络未焊） |
| 7 | MPY634 模拟乘法器 | 模拟处理 | 纯模拟，MCU 无需驱动 |
| 8 | AD633 模拟乘法器 | 模拟处理 | 纯模拟，MCU 无需驱动 |
| 9 | MAX4466 声音传感器 | 传感器 | 模拟输出，可用现有 ADC 驱动（`tsp_adc`） |
| 10 | XD5615 10-bit DAC | 数模转换 | 需开发 SPI 驱动（兼容 TLC5615） |
| 11 | BK2461 / JDY-40 无线串口 | 无线通信 | UART 透传，可用现有 UART 驱动 |

---

## 一、信号发生

### 1.1 AD9834 — 75 MHz 低功耗 DDS

**制造商**：Analog Devices
**核心用途**：正弦/三角/方波发生器、FSK/PSK 调制、传感器激励源

#### 主要参数

| 参数 | 规格 |
|------|------|
| 主时钟 MCLK | 75 MHz（C 级）/ 50 MHz（B 级） |
| 最大输出频率 | 37.5 MHz (@ 75 MHz MCLK) |
| DAC 分辨率 | 10 位 |
| 频率字宽度 | 28 位 |
| 频率分辨率 | 0.28 Hz @ 75 MHz MCLK |
| 相位寄存器 | 12 位（0.088° 步进） |
| SFDR | >72 dB |
| 功耗 | 20 mW @ 3V |
| 供电 | 2.3 V ~ 5.5 V |
| 满量程输出电流 | 3.0 mA |

#### 引脚定义 (20-TSSOP)

| 引脚 | 名称 | 功能 |
|------|------|------|
| 1 | COMP | 比较器输入 |
| 2 | VIN | 比较器输入（外部信号） |
| 3 | DACBP | DAC 偏置去耦（接 100nF 到 AGND） |
| 4 | AGND | 模拟地 |
| 5 | AVDD | 模拟电源 |
| 6 | IOUT | DAC 电流输出 |
| 7 | IOUTB | DAC 互补电流输出 |
| 8 | FS ADJUST | 满量程电流调节（经 Rset 到 AGND） |
| 9 | REFOUT | 1.20V 基准输出 |
| 10 | DVDD | 数字电源 |
| 11 | DGND | 数字地 |
| 12 | MCLK | 主时钟输入 |
| 13 | FSELECT | 频率寄存器选择 |
| 14 | PSELECT | 相位寄存器选择 |
| 15 | RESET | 复位（高有效） |
| 16 | SLEEP | 休眠（高有效） |
| 17 | SDATA | SPI 数据输入 |
| 18 | SCLK | SPI 时钟 |
| 19 | FSYNC | SPI 帧同步（低有效） |
| 20 | SIGN BIT OUT | 比较器方波输出 |

#### 内部结构

```text
MCLK -> [28位相位累加器] -> [12位截断] -> [SIN ROM] -> [10位DAC] -> IOUT/IOUTB
         ^                        ^
    FREQ0/FREQ1              PHASE0/PHASE1

    IOUT -> [外部滤波] -> COMP -> [片内比较器] -> SIGN BIT OUT (方波)
```

#### 频率计算

```text
Fout = (FREQ寄存器值 / 2^28) × MCLK

举例（MCLK = 75 MHz）：
  1 MHz  → FREQ ≈ 3,579,140
  10 kHz → FREQ ≈ 35,792
```

#### 关键寄存器 (SPI)

| 寄存器 | 宽度 | 地址前缀 | 功能 |
|--------|------|----------|------|
| FREQ0 | 28 位 | 010x | 频率寄存器 0 |
| FREQ1 | 28 位 | 011x | 频率寄存器 1 |
| PHASE0 | 12 位 | 100x | 相位寄存器 0 |
| PHASE1 | 12 位 | 101x | 相位寄存器 1 |
| 控制寄存器 | 16 位 | 001x | B28/HLB/FSEL/PSEL/SLEEP/RESET/OPBITEN/MODE |

#### AD9834 vs AD9833

| 对比项 | AD9834 | AD9833 |
|--------|--------|--------|
| 封装 | 20-TSSOP | 10-MSOP |
| MCLK max | 75 MHz | 25 MHz |
| 片内比较器 | 有 | 无 |
| 输出类型 | IOUT（电流）+ 方波 | VOUT（电压） |
| 功耗 | 20 mW | 12.65 mW |

#### 应用要点

- **SPI 时序**：FSYNC 拉低 → 16 位数据在 SCLK 下降沿移入 → FSYNC 拉高完成写入
- **FS ADJUST**：Rset 典型 6.8kΩ → IOUT_FS ≈ 3mA
- **电源去耦**：AVDD / DVDD 各自加 0.1μF + 10μF，AGND/DGND 单点连接
- **频率字写入**：需按手册先写控制寄存器再写 FREQ LSB/MSB

#### 参考资源

- [产品页](https://www.analog.com/en/products/ad9834.html) · [Datasheet PDF](https://www.analog.com/media/en/technical-documentation/data-sheets/ad9834.pdf)
- [Arduino 库: AliBarber/AD9834](https://github.com/AliBarber/AD9834/)
- [STM32 HAL 驱动: AD9833/AD9834](https://github.com/Bardia-Afshar/AD9833-AD9834-STM32-HAL)

---

### 1.2 ICL8038 — 精密模拟波形发生器

**制造商**：Intersil（现 Renesas） **状态**：已停产，市场存量为主
**核心用途**：函数信号发生器、VCO、扫频信号源

#### 主要参数

| 参数 | 规格 |
|------|------|
| 频率范围 | 0.001 Hz ~ 300 kHz+ |
| 供电 | 单电源 +10~+30V；双电源 ±5~±15V |
| 正弦波 THD | 1%~2%（可调至 0.8%） |
| 三角波线性度 | 0.1%（典型） |
| 占空比调节 | 2% ~ 98% |
| 扫频比 | 35:1（可扩展至 1000:1） |
| 频率温漂 | 120~250 ppm/°C |

#### 引脚定义 (14-PDIP)

| 引脚 | 名称 | 功能 |
|------|------|------|
| 1 | ADJ-SINE1 | 正弦波失真调整 1 |
| 2 | SINE OUT | 正弦波输出 |
| 3 | TRI OUT | 三角波输出 |
| 4 | ADJ-FREQ1 | 频率/占空比调节 1 |
| 5 | ADJ-FREQ2 | 频率/占空比调节 2 |
| 6 | V+ | 正电源 |
| 7 | FM BIAS | 调频偏置（内部基准 ≈ V+/5） |
| 8 | FM SWEEP IN | 调频/扫频电压输入 |
| 9 | SQ OUT | 方波输出（集电极开路，需上拉） |
| 10 | C | 定时电容 |
| 11 | V- / GND | 负电源或地 |
| 12 | ADJ-SINE2 | 正弦波失真调整 2 |

#### 频率计算

```text
f = 1 / (R × C)     （占空比 50% 时，R = R_A = R_B）
```

| 频率 | R_A = R_B | C |
|------|-----------|---|
| 1 kHz | 10 kΩ | 0.1 μF |
| 10 kHz | 10 kΩ | 0.01 μF |
| 100 kHz | 10 kΩ | 1 nF |

#### 应用要点

- **方波输出 (Pin 9)**：开集电极，必须外接上拉电阻到 V+（如 10kΩ）
- **正弦失真调整**：Pin 1/12 各接 100kΩ 电位器到 V+，微调 THD
- **定时电容**：选 NPO/COG 陶瓷或聚苯乙烯电容以降低温漂
- **FM 扫频**：Pin 8 输入 0 ~ 3/5·V+ 的电压改变频率

#### ICL8038 vs AD9834 vs MAX038

| 对比项 | ICL8038 | AD9834 (DDS) | MAX038 |
|--------|---------|--------------|--------|
| 频率范围 | ~300kHz | ~37.5MHz | ~20MHz |
| 频率设定 | RC 模拟 | SPI 数字 | RC + 引脚 |
| 失真 | 1% | 60dB SNR | 0.75% |
| 状态 | 停产 | 量产 | 停产 |

#### 参考资源

- [Datasheet](https://www.renesas.com/en/document/dst/icl8038-datasheet)
- [AN013 应用笔记](https://www.renesas.com/us/en/document/apn/an013-everything-you-always-wanted-know-about-icl8038)

---

## 二、增益控制与放大

### 2.1 AD603 — 90 MHz 压控增益放大器

**制造商**：Analog Devices
**架构**：X-AMP（无源衰减器 + 固定增益放大器）
**核心用途**：RF/IF AGC、视频增益控制、信号测量

#### 主要参数

| 参数 | 数值 |
|------|------|
| 增益控制 | 线性 dB，25 mV/dB (40 dB/V) |
| 带宽 | 90 MHz（-11~+31dB 模式） |
| 增益精度 | ±0.5 dB |
| 输入噪声密度 | 1.3 nV/√Hz |
| 供电 | ±5V |
| 功耗 | 125 mW |
| 压摆率 | 275 V/μs |
| 增益响应时间 | <1 μs（40dB 变化） |
| 输入阻抗 | 100Ω (±3%) |
| 峰值输出 | ±2.5V（500Ω 负载） |

#### 引脚定义 (8-SOIC)

| 引脚 | 名称 | 功能 |
|------|------|------|
| 1 | GPOS | 增益控制高端（+V → 增益增大） |
| 2 | GNEG | 增益控制低端 |
| 3 | VINP | 信号输入 |
| 4 | COMM | 地 |
| 5 | FDBK | 反馈（设增益范围） |
| 6 | VNEG | -5V |
| 7 | VOUT | 输出 |
| 8 | VPOS | +5V |

#### 增益控制

```text
Gain (dB) = 40 × (V_GPOS - V_GNEG) + 范围偏移
```

| FDBK 接法 | 带宽 | 增益范围 |
|-----------|------|---------|
| 短接至 VOUT | 90 MHz | -11 ~ +31 dB |
| 悬空 | 9 MHz | +9 ~ +51 dB |
| 电阻/电容网络 | 30 MHz | -1 ~ +41 dB |

#### AGC 板常见标注速查

实验室 AD603 AGC 板上常见的测试点标注含义：

| 标注 | 含义 | 信号类型 | 测量方法 |
|------|------|----------|---------|
| **Vsine** | 正弦信号输入 | AC (mV~V) | 示波器 AC 档 |
| **Ve** | AGC 误差/控制电压 | DC 慢变 | 万用表 / 示波器 DC 档 |
| **WFG** | 波形发生器接口 | AC | 示波器看信号 |

```text
Vsine ----> [AD603] ----> [VOUT] ----> [检波器] ----+
                                                    |
              Ve (误差电压，从积分器返回 GPOS/GNEG) <--+
```

| AGC 参考参数 | 典型值 |
|-------------|-------|
| GPOS/GNEG 控制范围 | -500mV ~ +500mV 差分 |
| Ve 典型范围 | -0.65V ~ +0.65V |
| 输出稳定幅度 | ~1.4Vrms / 3.6Vpp |

#### 应用要点

- 单端输入时，另一输入端对地交流耦合
- 供电 ±5V，去耦 0.1μF + 10μF 钽电容就近放置
- 增益控制输入阻抗 50MΩ，偏置电流 200nA

#### 参考资源

- [产品页](https://www.analog.com/en/products/ad603.html) · [Datasheet PDF](https://www.analog.com/media/en/technical-documentation/data-sheets/AD603.pdf)
- [评估板 EVAL-AD603](https://www.analog.com/en/resources/evaluation-hardware-and-software/evaluation-boards-kits/eval-ad603.html)

---

### 2.2 AD8367 — 500 MHz VGA + AGC 检波器

**制造商**：Analog Devices
**架构**：X-AMP（9 级衰减器 + 固定增益放大器 + 高斯插值器）
**核心用途**：宽带 AGC、PA 功控环路、IF AGC

#### 主要参数

| 参数 | 规格 |
|------|------|
| 带宽 | 500 MHz |
| 增益范围 | -2.5 ~ +42.5 dB (45 dB) |
| 增益控制 | 20 mV/dB，VGAIN 0~1V |
| 输入阻抗 | 200Ω |
| 最大输入 | 700 mVpp |
| OIP3 | +36.5 dBm @ 70 MHz |
| 噪声系数 | 6.2 dB |
| 供电 | 2.7~5.5V 单电源 |
| 静态电流 | 26 mA |

#### 引脚定义 (14-TSSOP)

| 引脚 | 名称 | 功能 |
|------|------|------|
| 1, 3 | ICOM | 输入公共地 |
| 2 | INPT | 信号输入 |
| 4 | ENBL | 使能（高有效） |
| 5, 7 | OCOM | 输出公共地 |
| 6 | VOUT | 信号输出 |
| 8 | VPSI | 输入级电源 |
| 9 | VPSO | 输出级电源 |
| 10 | VGAIN | 增益控制输入 (0~1V) |
| 11 | MODE | 模式（HI=递增 / LO=递减） |
| 12 | DETO | 检波器输出 (RSSI) |
| 13 | DECL | 检波器去耦（接电容到地） |
| 14 | HPFL | 高通滤波器 |

#### 增益公式

```text
递增模式 (MODE=HI):  Gain (dB) = 50 × VGAIN - 5
递减模式 (MODE=LO):  Gain (dB) = 45 - 50 × VGAIN    ← AGC 专用
```

#### AGC 闭环配置

```text
VOUT → [平方律检波器 DETO] → [CAGC 积分] → VGAIN (MODE=LO)
```

- 输出设定点：354 mVrms（与波峰因子无关）
- CAGC 100pF → 响应时间 ~1μs（6dB 阶跃）

#### AD8367 vs AD603

| 对比项 | AD8367 | AD603 |
|--------|--------|-------|
| 带宽 | 500 MHz | 90 MHz |
| 增益范围 | 45 dB | 42 dB |
| 供电 | 单电源 2.7~5.5V | 双电源 ±5V |
| 片内检波器 | 有 | 无 |
| 片上 AGC | 仅需一个电容 | 需外部检波+积分 |
| 噪声 | NF 6.2dB | 1.3 nV/√Hz |

#### 应用要点

- VGAIN 纹波必须 <1mV，否则增益抖动
- AGC：MODE 接低，DETO 经 CAGC 接 VGAIN
- ENBL 不可悬空，不用时上拉到 VPS
- DECL 接 0.1μF 到地

#### 参考资源

- [产品页](https://www.analog.com/cn/products/ad8367.html) · [Datasheet PDF](https://www.analog.com/media/en/technical-documentation/data-sheets/AD8367.pdf)
- [评估板 EVAL-AD8367](https://www.analog.com/en/resources/evaluation-hardware-and-software/evaluation-boards-kits/eval-ad8367.html)
- [技术文章：集成 VGA 精确增益控制](https://www.analog.com/en/resources/technical-articles/integrated-vga-aids-precise-gain-control.html)
- [EngineerZone：AGC 接线与削波问题](https://ez.analog.com/rf/f/q-a/554225/ad8367---output-doesn-t-look-like-a-full-sine-wave)

---

### 2.3 AD620 / AD623 — 程控仪表放大器

**核心用途**：差分信号放大、电桥测量、生物电信号。配合数字电位器实现程控增益。

#### 增益公式

| 芯片 | 公式 |
|------|------|
| AD620 | G = 1 + 49.4kΩ / Rg |
| AD623 | G = 1 + 100kΩ / Rg |

#### G3519 平台实现

实验室"程控仪表放大器"模块使用 AD620 + MCP41010（100kΩ 数字电位器）。
G3519 驱动：`tsp_mcp41010.c/.h`，GPIO bit-bang SPI (PC25/PC26/PC27)。

```c
tsp_pga_init();           // GPIO 初始化
tsp_pga_set(127);         // 设置电位器值 0~255
tsp_pga_set_gain(20);     // 设置目标增益 ×10 单位 (20 = 2.0x)
// MCP41010: Rg = data × 391 + 52Ω
// AD620:    G  = 1 + 49.4kΩ / Rg
```

#### MCP41010 SPI 协议

- Mode 0 (CPOL=0, CPHA=0)，MSB first
- 命令字 0x11（写 Pot 0） + 8-bit data
- 3.3V 时序：tSU ≥ 50ns, tCH ≥ 50ns, tCL ≥ 50ns
- G3519 @ 80MHz 需插入 `__NOP()` 延时满足时序

#### 参考资源

- [AD620 Datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/AD620.pdf) · [AD623 Datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/ad623.pdf)
- [AN-579：数字电位器做程控放大器](https://www.analog.com/media/en/technical-documentation/application-notes/an-579.pdf)
- [CN0273 参考电路](https://www.analog.com/en/resources/reference-designs/circuit-collections/cn0273.html)

---

## 三、检测与测量

### 3.1 AD8302 — LF~2.7 GHz 增益/相位检测器（封存）

一片同时输出幅度比 (VMAG) + 相位差 (VPHS)，用于 RF/IF 增益与相位测量、VSWR/回波损耗、PA 线性化。**G3519 板载但输入网络未焊，已封存**。

> 详细资料（参数/引脚/传递函数/三种工作模式/应用要点）见 [`common/docs/AD8302_Use_Mothball.md`](../../common/docs/AD8302_Use_Mothball.md)。

---

## 四、模拟信号处理

### 4.1 MPY634 — 宽带精密模拟乘法器

**制造商**：Texas Instruments（原 Burr-Brown）
**核心用途**：精密乘法/除法/平方/平方根、调制解调、压控放大

#### 主要参数

| 参数 | 数值 |
|------|------|
| 带宽 | 10 MHz |
| 精度 | ±0.5%（AM/BM 级） |
| 压摆率 | 20 V/μs |
| 比例因子 SF | 10V（可调 0.1~10） |
| 供电 | ±8V ~ ±15V（额定 ±15V） |
| 输入范围 | ±10V |
| 静态电流 | 4 mA |

#### 传递函数

```text
V_OUT = (X1 - X2)(Y1 - Y2) / SF + Z2     SF = 10V（标称）
```

#### 引脚定义 (14-DIP)

| 引脚 | 名称 | 功能 |
|------|------|------|
| 1 | X1 | X 差分正端 |
| 2 | X2 | X 差分负端 |
| 3 | -VS | 负电源 |
| 4 | Y1 | Y 差分正端 |
| 5 | Y2 | Y 差分负端 |
| 7 | Z1 | 求和负端 |
| 8 | Z2 | 求和正端 |
| 10 | OUT | 输出 |
| 11 | +VS | 正电源 |

#### 典型接法

| 功能 | 接法 |
|------|------|
| 乘法器 | X1/Y1 接信号，X2/Y2 接地，Z1 接 OUT，Z2 接地 |
| 平方器 | X 和 Y 并联接同一信号 |
| 除法器 | 乘法器置于运放反馈回路 |
| 压控放大器 | Y 接控制电压，X 接信号 |

#### 参考资源

- [产品页](https://www.ti.com/product/MPY634) · [Datasheet PDF](https://www.ti.com/lit/ds/symlink/mpy634.pdf)

---

### 4.2 AD633 — 低成本四象限模拟乘法器

**制造商**：Analog Devices
**特点**：8 脚封装，无需外部器件，低成本

#### 主要参数

| 参数 | 典型值 |
|------|--------|
| 总误差 | ±1% (±2% max) |
| 带宽 | 1 MHz |
| 压摆率 | 20 V/μs |
| 建立时间 (1%) | 2 μs |
| 供电 | ±8V ~ ±18V（典型 ±15V） |
| 输出摆幅 | ±11V |
| 静态电流 | 4 mA |

#### 传递函数

```text
W = (X1 - X2)(Y1 - Y2) / 10V + Z
```

内置激光修调 10V 比例基准。Z 为高阻抗单位增益求和端。

#### 引脚定义 (8-PDIP/SOIC)

| 引脚 | 名称 | 功能 |
|------|------|------|
| 1 | X1 | X 差分正端 |
| 2 | X2 | X 差分负端 |
| 3 | Y1 | Y 差分正端 |
| 4 | Y2 | Y 差分负端 |
| 5 | -VS | 负电源 |
| 6 | Z | 求和输入 |
| 7 | W | 输出 |
| 8 | +VS | 正电源 |

#### AD633 vs MPY634

| 对比项 | AD633 | MPY634 |
|--------|-------|--------|
| 带宽 | 1 MHz | 10 MHz |
| 精度 | ±2% max | ±0.5% max |
| 封装 | 8-PDIP/SOIC | 14-DIP/SOIC-16 |
| 外部器件 | 无需 | 通常无需 |
| 成本 | 低 | 中高 |
| 适用场景 | 通用低成本 | 高频高精度 |

#### 应用要点

- 无需外部器件即可实现完整四象限乘法
- Z 输入可作求和节点，方便加偏置或多路叠加
- 不用的输入端必须接地（不可悬空）
- 带宽仅 1 MHz，高频选 AD834/AD835

#### 参考资源

- [产品页](https://www.analog.com/cn/products/AD633.html) · [Datasheet PDF](https://www.analog.com/media/en/technical-documentation/data-sheets/AD633.pdf)

---

## 五、传感器

### 5.1 MAX4466 — 驻极体麦克风声音传感器

**芯片**：Maxim/ADI MAX4466
**模块型号**：G54 / GY-MAX4466
**核心用途**：声音检测、噪声测量、音频前置放大

#### 主要特性

| 参数 | 规格 |
|------|------|
| 增益 | 25× ~ 125×（板载电位器可调） |
| 输出 | 模拟电压，偏置约 VCC/2 |
| 推荐供电 | 3.3V（也支持 5V） |
| 频率响应 | 20 Hz ~ 20 kHz |

#### 接线

| 引脚 | 接法 |
|------|------|
| VCC | 3.3V（推荐）或 5V |
| GND | GND |
| OUT | ADC 输入（如 G3519 的 J2 通道） |

#### 使用技巧

- 用短时间窗取 max-min 得峰峰值，比单次采样更稳定
- G3519 平台可直接使用 `tsp_adc_read_mv()` 采样
- 板载电位器调节增益，顺时针增大

#### 参考资源

- [MAX4465-MAX4469 Datasheet](https://cdn-shop.adafruit.com/datasheets/MAX4465-MAX4469.pdf)
- [Adafruit MAX4466 产品页](https://www.adafruit.com/product/1063)
- [Maker Guides: MAX4466 + Arduino](https://www.makerguides.com/detect-sound-with-max4466-and-arduino/)
- [ElectroPeak: 接线与示例](https://electropeak.com/learn/interfacing-max4466-microphone-module-with-arduino/)

---

## 六、数模转换

### 6.1 XD5615 — 10 位 SPI DAC

**芯片**：信路达 XD5615 / XL5615，与 TI TLC5615 引脚兼容
**核心用途**：控制电压输出、波形生成、设定基准

#### 主要特性

| 参数 | 规格 |
|------|------|
| 分辨率 | 10 位 |
| 接口 | 三线 SPI（CS/SCLK/DIN） |
| 供电 | 5V 单电源 |
| 输出 | 约 2 × VREF |
| 串行字 | 16 位（有效 10 位） |

#### SPI 协议

- CS 低有效，SCLK 上升沿移入数据
- 16-bit 串行字，高 10 位为数据，低 4 位补零，最高 2 位为无关位

#### G3519 集成建议

需开发 GPIO bit-bang SPI 驱动（参考 `tsp_mcp41010.c` 的实现模式）。
可用于为 AD603/AD8367 提供增益控制电压。

#### 参考资源

- [TLC5615 Datasheet（兼容）](https://www.ti.com/lit/ds/symlink/tlc5615.pdf)
- [Arduino 库: ArduinoMax/TLC5615](https://github.com/ArduinoMax/TLC5615)
- [立创: XD5615](https://item.szlcsc.com/1056050.html)

---

## 七、无线通信

### 7.1 BK2461 / JDY-40 — 2.4 GHz 无线串口透传

**核心用途**：无线串口数据透传、遥控、远程传感器数据采集

#### 主要特性

| 参数 | 规格 |
|------|------|
| 频段 | 2.4 GHz |
| 接口 | UART（默认 9600 baud） |
| 供电 | **3.3V（勿接 5V）** |
| 模式 | AT 命令模式 (SET=LOW) / 透传模式 (SET=HIGH) |

#### 使用要点

- 配对需 RFID / DVID / 信道一致（AT 命令配置）
- SET 引脚拉低进入 AT 命令模式，拉高为透传
- G3519 可直接使用现有 UART 驱动（`tsp_uart`/`tsp_uart3`/`tsp_uart6`）

#### 参考资源

- [JDY-40 英文手册](https://arduinolab.pw/wp-content/uploads/2019/05/EY-40_English_manual.pdf)
- [RalphBacon 项目: BK2461/JDY-40 汇总](https://github.com/RalphBacon/257-Serial-Wireless-Comms)

---

## 附录 A：快速资源索引

| 类型 | 模块 | 链接 |
|------|------|------|
| Datasheet | AD603 | [PDF](https://www.analog.com/media/en/technical-documentation/data-sheets/AD603.pdf) |
| Datasheet | AD8367 | [PDF](https://www.analog.com/media/en/technical-documentation/data-sheets/AD8367.pdf) |
| Datasheet | AD620 | [PDF](https://www.analog.com/media/en/technical-documentation/data-sheets/AD620.pdf) |
| Datasheet | AD623 | [PDF](https://www.analog.com/media/en/technical-documentation/data-sheets/ad623.pdf) |
| Datasheet | AD8302 | [PDF](https://www.analog.com/media/en/technical-documentation/data-sheets/ad8302.pdf) |
| Datasheet | AD9834 | [PDF](https://www.analog.com/media/en/technical-documentation/data-sheets/ad9834.pdf) |
| Datasheet | AD633 | [PDF](https://www.analog.com/media/en/technical-documentation/data-sheets/AD633.pdf) |
| Datasheet | MPY634 | [PDF](https://www.ti.com/lit/ds/symlink/mpy634.pdf) |
| Datasheet | MAX4466 | [PDF](https://cdn-shop.adafruit.com/datasheets/MAX4465-MAX4469.pdf) |
| Datasheet | TLC5615 | [PDF](https://www.ti.com/lit/ds/symlink/tlc5615.pdf) |
| Datasheet | ICL8038 | [PDF](https://www.renesas.com/en/document/dst/icl8038-datasheet) |
| 用户手册 | JDY-40 | [PDF](https://arduinolab.pw/wp-content/uploads/2019/05/EY-40_English_manual.pdf) |
| GitHub 驱动 | AD9834 Arduino | [AliBarber/AD9834](https://github.com/AliBarber/AD9834/) |
| GitHub 驱动 | AD9834 STM32 | [Bardia-Afshar/AD9833-AD9834-STM32-HAL](https://github.com/Bardia-Afshar/AD9833-AD9834-STM32-HAL) |
| GitHub 驱动 | TLC5615 Arduino | [ArduinoMax/TLC5615](https://github.com/ArduinoMax/TLC5615) |
| 应用笔记 | AD620 程控增益 | [AN-579](https://www.analog.com/media/en/technical-documentation/application-notes/an-579.pdf) |
| 应用笔记 | ICL8038 详解 | [AN013](https://www.renesas.com/us/en/document/apn/an013-everything-you-always-wanted-know-about-icl8038) |

---

*合并自 `电子模块清单.md` + `器件汇总.md`。链接整理时间 2026-07-28，部分第三方链接可能失效，以芯片型号在 ADI / TI / Renesas / 立创官网检索为准。*

*PS：已有的电源小模块：MORNSUN WRA0512S-1WR2（隔离电源，手册见 `common/docs/datasheets/peripherals/WRA_S-1WR2.pdf`）。*
