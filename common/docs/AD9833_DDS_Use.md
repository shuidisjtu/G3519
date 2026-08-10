# AD9833 DDS 波形发生器使用说明

> 编写日期：2026-07-21
> 状态：✅ 已验证（J9 跳线帽已补齐，方波/正弦/三角波均正常工作）

## 1. 硬件概览

| 项目 | 说明 |
|------|------|
| 芯片 | AD9833BRM (U4)，3 线同步串行接口（非 I2C） |
| 主时钟 | X1 有源晶振，25 MHz |
| 控制方式 | MCU GPIO 位脉冲模拟 SPI |
| 控制引脚 | PC2=SCLK, PC3=SDATA, PC24=FSYNC |
| 输出端子 | J12（同轴）/ J22（2-pin 排针） |
| 桥接排针 | **J9 必须装跳线帽**（1-2, 3-4, 5-6），否则 MCU 信号无法到达 AD9833 |

### AD9833 引脚定义

| Pin | 功能 | Pin | 功能 |
|-----|------|-----|------|
| 1 | COMP (退耦) | 6 | SDATA (← PC3) |
| 2 | VDD (3.3V) | 7 | SCLK (← PC2) |
| 3 | CAP/2.5V (LDO) | 8 | FSYNC (← PC24) |
| 4 | DGND | 9 | AGND |
| 5 | MCLK (25MHz) | 10 | VOUT |

### J9 跳线帽（关键）

```
J9-1 (PC3=SDATA)  ──[跳线帽]──  J9-2 (AD9833 Pin 6)
J9-3 (PC2=SCLK)   ──[跳线帽]──  J9-4 (AD9833 Pin 7)
J9-5 (PC24=FSYNC) ──[跳线帽]──  J9-6 (AD9833 Pin 8)
J9-7 — GND
J9-8 — GND
```

> ⚠️ **J9 缺少跳线帽是已验证的最常见故障原因。** 症状：DDS Test 菜单正常进入，但 J22/J12 无任何输出波形。

---

## 2. 菜单操作

主菜单 → DDS Test（单屏交互，无子菜单）

**按键**：
- `S0`：切换上一个波形（Square → Triangle → Sine 循环）
- `S1`：切换下一个波形（Square → Sine → Triangle 循环）
- `编码器`：调整输出频率（100 Hz ~ 50 kHz，自适应步进）
- `PUSH`：停止 DDS 输出并返回主菜单

### LCD 显示布局

```
Row 0: AD9833 DDS Generator  [YELLOW/BLUE]
Row 2: Wave: Square          [CYAN]     ← 当前波形
Row 3: Freq:    1000 Hz      [WHITE]    ← 当前频率
Row 4: Vout ~3.1V (MSB)      [WHITE]    ← 幅度提示
Row 6: S0/S1:Wave  Enc:Freq  [GRAY]
Row 7: PUSH to exit          [GRAY]
```

波形和频率仅在值变化时增量刷新（无闪烁）。

---

## 3. 输出特性

| 波形 | 控制字 | 输出路径 | 实测 Vout | 频率 |
|------|--------|---------|-----------|------|
| **Square** | `0x2028` | DAC MSB (OPBITEN=1, DIV2=1) | ~3.1V (轨到轨) | = FREQ0 |
| **Sine** | `0x2000` | DAC 模拟 (OPBITEN=0, MODE=0) | ~0.6Vpp (典型) | = FREQ0 |
| **Triangle** | `0x2002` | DAC 模拟 (OPBITEN=0, MODE=1) | ~0.6Vpp (典型) | = FREQ0 |

> 方波幅度为实测值（2026-07-21, AD2 @ J22）。频率通过编码器在 100 Hz ~ 50 kHz 范围内调节。

---

## 4. 代码 API 参考

### 4.1 控制字宏

```c
#define AD9833_RESET     0x2100   // B28=1, RESET=1
#define AD9833_FREQ0     0x4000   // D15-D14=01 → FREQ0 register
#define AD9833_SINE      0x2000   // B28=1, OPBITEN=0, MODE=0
#define AD9833_TRIANGLE  0x2002   // B28=1, OPBITEN=0, MODE=1
#define AD9833_SQUARE    0x2028   // B28=1, OPBITEN=1, DIV2=1
```

### 4.2 频率字计算

```c
#define DDS_MCLK    25000000UL    // 25 MHz
#define DDS_FREQ_REG(f_hz) \
    ((uint32_t)(((uint64_t)(f_hz) * 268435456ULL + (DDS_MCLK / 2)) / DDS_MCLK))
```

公式：`FREQREG = round(f_out × 2^28 / 25,000,000)`

**常用频率**：

| f_out | FREQREG (hex) |
|-------|---------------|
| 1 kHz | 0x000029F1 |
| 10 kHz | 0x0001A36A |
| 100 kHz | 0x0010624E |
| 1 MHz | 0x00A3D70A |

### 4.3 核心函数（`tsp_dds.h` / `tsp_dds.c`）

```c
#include "tsp_dds.h"

// 设置频率 + 波形并开始输出（无需单独 init）
tsp_dds_set_output(1000, AD9833_SINE);       // 1 kHz 正弦波
tsp_dds_set_output(5000, AD9833_SQUARE);     // 5 kHz 方波
tsp_dds_set_output(800, AD9833_TRIANGLE);    // 800 Hz 三角波

// 停止输出
tsp_dds_stop();

// 编码器自适应步进（<1kHz: 10Hz, <10kHz: 100Hz, ≥10kHz: 1kHz, 快转×10）
uint32_t step = tsp_dds_get_step(freq, encoder_delta);

// 波形元数据（用于 LCD 显示）
tsp_dds_wave_names[i]   // "Square" / "Sine" / "Triangle"
tsp_dds_wave_ctrl[i]    // AD9833_SQUARE / AD9833_SINE / AD9833_TRIANGLE
tsp_dds_vout_info[i]    // "Vout ~3.1V (MSB)" / "Vout ~0.6Vpp (DAC)" / ...
DDS_WAVE_COUNT           // 3
```

### 4.4 文件位置

| 文件 | 内容 |
|------|------|
| `NUEDC2025/tsp_dds.c` | `tsp_dds_write()`、`tsp_dds_set_output()`、`tsp_dds_stop()`、`tsp_dds_get_step()`、波形元数据 |
| `NUEDC2025/tsp_dds.h` | AD9833 常量、API 原型 |
| `TSP3519/tsp_gpio.h` | DDS GPIO 宏（`DDS_SCLK_HIGH/LOW` 等） |
| `iar/empty_mspm0g3519.c` | `action_dds_test()` 交互测试（调用 tsp_dds API） |
| `iar/empty_mspm0g3519.syscfg` | SysConfig 引脚配置（PC2/PC3/PC24 为 GPIO 输出） |

---

## 5. SysConfig 配置

```javascript
// PC2/PC3/PC24 作为 GPIO 输出（MODE=1, PF=0x01），不绑定 I2C2 外设
GPIO3.associatedPins[4].$name       = "DDS_SCLK";   // PC2
GPIO3.associatedPins[4].assignedPin = "2";
GPIO3.associatedPins[5].$name       = "DDS_SDATA";  // PC3
GPIO3.associatedPins[5].assignedPin = "3";
GPIO3.associatedPins[6].$name       = "DDS_FSYNC";  // PC24
GPIO3.associatedPins[6].assignedPin = "24";
```

> PC2/PC3 在电路中标有 I2C2 复用功能，但 AD9833 是三线串行协议并非 I2C，**必须保持 GPIO 模式（MODE=1）**，不可设为 I2C2（MODE=2）。

---

## 6. AD2 测量参考

### 6.1 示波器测 VOUT（日常验证）

```
AD2 1+ → J22-1（信号）
AD2 1- → J22-2（GND）
```

| 参数 | 方波 | 正弦/三角 |
|------|------|-----------|
| 时间基准 | 0.2 ms/div | 0.2 ms/div |
| 电压范围 | 1 V/div | 500 mV/div |
| 触发 | 上升沿, 1.5V | 上升沿, 0.3V |

### 6.2 示波器测 MCLK（时钟验证）

```
AD2 1+ → AD9833 Pin 5（或 X1 Pin 3）
AD2 1- → GND
```

时间基准 100 ns/div，电压 1 V/div，预期 25MHz ~3.3Vpp。

### 6.3 逻辑分析仪测 SPI（通信验证，高级）

```
AD2 D0 → FSYNC (Pin 8 或 J9-6)
AD2 D1 → SCLK  (Pin 7 或 J9-4)
AD2 D2 → SDATA (Pin 6 或 J9-2)
AD2 GND → 主板 GND
```

Logic 模式，FSYNC↓ 触发，单次捕获。预期 4 帧，每帧 16 bit MSB-first：

| 帧 | 期望值 | 含义 |
|----|--------|------|
| 1 | 0x2100 | RESET |
| 2 | 0x69F1 | FREQ0 LSB |
| 3 | 0x4000 | FREQ0 MSB |
| 4 | 0x2028 | Square (或 0x2000/0x2002) |

---

## 7. 故障排查

| 症状 | 最可能原因 | 验证方法 |
|------|-----------|---------|
| 无任何波形 | J9 缺少跳线帽 | 万用表蜂鸣档测 J9 1-2/3-4/5-6 |
| 无任何波形 (J9 已通) | MCLK 缺失 | AD2 示波器测 Pin 5 有无 25MHz |
| 频率不对 | 频率字计算错误或 DIV2 位设置错误 | 查 §4.2 频率表，确认 DIV2 与波形匹配 |
| J12 无波形但 J22 有 | J12 同轴座或走线断路 | 使用 J22 替代 |
| 正弦/三角幅度异常 | DAC 模拟输出路径受负载影响 | 直接量 Pin 10 确认 |

---

## 8. 已知限制

- **与 CCD 引脚冲突**：PC2/PC3 同时被 AD9833 (GPIO 输出) 和 CCD (ADC 输入) 使用。禁止同时使能两者
- **GPIO 位脉冲速率**：DL_GPIO 函数调用开销使 SCLK 频率自然较低（~2-4MHz），远低于 AD9833 40MHz 上限，暂不影响功能
