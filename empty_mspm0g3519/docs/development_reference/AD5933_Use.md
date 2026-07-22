# AD5933 阻抗测量使用说明

> 编写日期：2026-07-21
> 状态：✅ 已实装（I2C + 温度 + 扫频 + DFT 读取全部验证通过，AD2 Scope 确认 1kHz 激励输出，Real/Imag/Mag 实时刷新）

## 1. 硬件概览

| 项目 | 说明 |
|------|------|
| 芯片 | AD5933YRS (U5)，12 位阻抗转换器，内置频率发生器和 DSP |
| 主时钟 | X2 有源晶振，16 MHz |
| 控制方式 | I2C1（标准模式 100 kHz） |
| 控制引脚 | PA29=SCL, PA30=SDA |
| 7-bit 地址 | 0x0D |
| 模拟前端 | U6 双运放 AD8606ARZ（激励缓冲 + 回采跨阻放大） |
| 模拟接口 | J15（2×4 排针，见下方针脚定义） |
| 反馈电阻 | R37 = 20 kΩ（板载，未引出；决定 I-V 转换增益） |
| I2C 桥接 | J19（MCU I2C1 ↔ VNA 网络桥接排针，需对照原理图确认跳线位置） |
| I2C 上拉 | R32/R33 = 2.2 kΩ（板载已焊接） |

### AD5933 引脚定义

| Pin | 功能 | Pin | 功能 |
|-----|------|-----|------|
| 1 | NC | 9 | DVDD |
| 2 | NC | 10 | AVDD1 |
| 3 | NC | 11 | AVDD2 |
| 4 | RFB (反馈电阻) | 12 | DGND |
| 5 | VIN (测量输入) | 13 | AGND1 |
| 6 | VOUT (激励输出) | 14 | AGND2 |
| 7 | NC | 15 | SDA (I2C → PA30) |
| 8 | MCLK (16MHz) | 16 | SCL (I2C → PA29) |

### J19 桥接排针（关键）

> ⚠️ **J19 跳线未按原理图安装是应优先排查的 I2C 通信失败原因。** 跳线位置需对照板卡原理图确认（MCU 侧 I2C1_SCL/SDA ↔ VNA 侧 AD5933 总线），不可直接套用其他模块（如 J9 DDS）的跳线布局。上电前用万用表蜂鸣档确认连通性。

### J15 针脚定义（2×4 排针）

| 针脚 | 信号 | 说明 |
|------|------|------|
| 1 | GND | 与 Pin 7 连通 |
| 2 | GND | 与 Pin 8 连通 |
| 3 | NC | 未连接 |
| 4 | `SENSE_IN` | 待测阻抗返回端 → U6A 反相输入 → AD5933 VIN |
| 5 | `VOUT_BUF` | 激励输出：AD5933 VOUT → C48 交流耦合 → U6B 缓冲 |
| 6 | NC | 未连接 |
| 7 | GND | 与 Pin 1 连通 |
| 8 | GND | |

### 接线方式

```
        ┌─────────────────────────────────┐
        │           AD5933 板载调理        │
        │                                 │
VOUT ──→│ C48 ──→ U6B(缓冲) ──→ J15-5    │──→ 待测阻抗 Zx
        │                                 │
VIN  ←──│ U6A(跨阻) ←── J15-4            │←── 待测阻抗 Zx
        │                                 │
        │ RFB = R37 (20kΩ, 板载)          │
        └─────────────────────────────────┘
```

- **J15-5 (VOUT_BUF)** — 接待测阻抗一端
- **J15-4 (SENSE_IN)** —接待测阻抗另一端
- **任意 GND** — 屏蔽线接地参考

---

## 2. 菜单操作

### 主菜单 → AD5933 Test

```
主菜单
  ├─ K230 Test
  ├─ DDS Test
  └─ AD5933 Test  ──→  阻抗测试界面（自动流程）
```

**按键**：
- `S2`：确认进入 AD5933 Test
- `PUSH`：返回主菜单

### 测试流程界面

进入后自动执行初始化，然后分**标定**和**测量**两个阶段：

#### 阶段 1：标定（Calibration）

初始化 + 温度读取后，LCD 提示：

```
┌──────────────────────┐
│AD5933 Impedance      │ ← Row 0
│C: B0 08              │ ← Row 1 (CTRL 回读)
│T: 32.1C              │ ← Row 2 (温度)
│Cal: 18000 Ohm        │ ← Row 3, CYAN (校准电阻值)
│Connect Rcal to J15   │ ← Row 4
│S2=Calibrate          │ ← Row 5
│                      │
│PUSH to exit          │ ← Row 7
└──────────────────────┘
```

**操作**：在 J15-4/5 之间接入 **18kΩ** 校准电阻，按 `S2` 开始标定。标定成功后显示 "Cal OK! Replace DUT"，再按 `S2` 进入测量。

> 校准电阻值在代码中定义为 `CAL_RESISTANCE = 18000.0f`，可根据手头电阻修改。

#### 阶段 2：实时测量

```
┌──────────────────────┐
│AD5933 Impedance      │ ← Row 0, YELLOW/BLUE
│C: B0 08              │ ← Row 1, WHITE
│T: 32.1C              │ ← Row 2, WHITE (每 1s 刷新)
│Freq: 1000 Hz         │ ← Row 3, WHITE
│Real: -16857          │ ← Row 4, WHITE (每次刷新)
│Imag: -21896          │ ← Row 5, WHITE (每次刷新)
│Z   : 18032 Ohm       │ ← Row 6, GREEN (阻抗值)
│PUSH to exit          │ ← Row 7, GRAY
└──────────────────────┘
```

每次 DFT 完成后自动发送 `REPEAT_FREQ` 命令重新测量，Real/Imag/Z 持续刷新。

**自动流程**：
1. `tsp_ad5933_init()` — 复位 + 外部时钟 + 待机
2. 回读 CTRL_H/CTRL_L 并显示
3. `tsp_ad5933_read_temperature()` — 测温
4. 等待 S2 — 用户接入校准电阻
5. `set_sweep(1000, 0, 0, 100, X1)` + `start_sweep()` — 标定测量
6. 等待 `DATA_VALID` — 读 Real/Imag — 计算 `GainFactor = 1 / (R_cal × Mag_cal)`
7. 等待 S2 — 用户换上被测件
8. 重新 `start_sweep()` — 进入 live loop
9. 每帧：等 `DATA_VALID` → 读 Real/Imag → `Z = 1 / (GF × Mag)` → 显示 → `REPEAT_FREQ`

**按键**：
- `PUSH`：任意阶段退出（自动掉电）

---

## 3. 测量原理

### 3.1 阻抗测量流程

AD5933 内置 DDS 频率发生器产生正弦激励信号（VOUT），经外部待测阻抗后，通过 RFB 反馈回片内 ADC。片内 DSP 在每个频点执行 1024 点 DFT，得到实部 (Real) 和虚部 (Imag) 数据。

```
VOUT ──→ Z(未知) ──→ VIN ──→ ADC ──→ DSP (DFT) ──→ {Real, Imag}
              ↑
            RFB (反馈电阻，设定电流-电压增益)
```

### 3.2 阻抗计算

```c
// 幅值（使用 hypotf() 防 16-bit 溢出）
float magnitude = hypotf((float)real, (float)imag);

// 阻抗 = 1 / (GainFactor × Magnitude)
// GainFactor 需通过已知校准电阻 (R_cal) 测量标定：
//   GF = 1 / (R_cal × Magnitude_cal)
float impedance = 1.0f / (gain_factor * magnitude);
```

### 3.3 扫频参数计算

AD5933 片上 DDS 频率字公式（DDS 内核时钟 = MCLK/4）：

```
Start_Freq_Code  = (f_start  × 2^29) / MCLK
Delta_Freq_Code  = (f_delta  × 2^29) / MCLK
```

> 注意：公式中必须使用 MCLK/4 作为 DDS 内核时钟。若直接用 MCLK 代入，输出频率将偏低 4 倍。

| MCLK | 1 kHz 频率字 | 10 kHz | 100 kHz |
|------|-------------|--------|---------|
| 16 MHz | 33,554 | 335,544 | 3,355,443 |

| 参数 | 寄存器 | 位宽 | 说明 |
|------|--------|------|------|
| 起始频率 | 0x82–0x84 | 24 bit | 扫频起始点 |
| 频率增量 | 0x85–0x87 | 24 bit | 每步增量 |
| 增量数 | 0x88–0x89 | 9 bit | 频率步进次数（总频点数 = 增量数 + 1） |
| 建立时间 | 0x8A–0x8B | 9 bit（0–511） | 每频点基础输出周期数；D10:D9 选择倍率（×1/×2/×4），实际周期数 = 基础 × 倍率 |

> **增量数 vs 频点数**：寄存器写入的是频率步进次数，而非总测量点数。若需 100 个频点，应写入 99 (`num_increments = 99`)。
>
> **建立时间范围**：基础周期数为 9 位（D8:D0 across 0x8A/0x8B），最大 511。`settling_cycles` 超出 511 会截断高位的倍率选择位，务必在调用侧校验。

### 3.4 激励电压与 PGA

| 电压范围 | CTRL_H 宏 | 说明 |
|----------|-----------|------|
| 2.0 V p-p | `AD5933_VOLT_2000MV` | 默认值，最大输出摆幅 |
| 1.0 V p-p | `AD5933_VOLT_1000MV` | |
| 0.4 V p-p | `AD5933_VOLT_400MV` | |
| 0.2 V p-p | `AD5933_VOLT_200MV` | |

| PGA 增益 | CTRL_H 宏 | 说明 |
|----------|-----------|------|
| ×1 | `AD5933_PGA_X1` | 默认值 |
| ×5 | `AD5933_PGA_X5` | 小信号场景 |

---

## 4. 代码 API 参考

### 4.1 寄存器宏

```c
#define AD5933_I2C_ADDR              0x0D    // 7-bit I2C 地址

// 寄存器地址
#define AD5933_REG_CTRL_H            0x80    // 控制寄存器高字节
#define AD5933_REG_CTRL_L            0x81    // 控制寄存器低字节
#define AD5933_REG_START_FREQ_H      0x82    // 起始频率 [23:16]
#define AD5933_REG_START_FREQ_M      0x83    // 起始频率 [15:8]
#define AD5933_REG_START_FREQ_L      0x84    // 起始频率 [7:0]
#define AD5933_REG_FREQ_INCR_H       0x85    // 频率增量 [23:16]
#define AD5933_REG_FREQ_INCR_M       0x86    // 频率增量 [15:8]
#define AD5933_REG_FREQ_INCR_L       0x87    // 频率增量 [7:0]
#define AD5933_REG_NUM_INCR_H        0x88    // 扫频点数 [8]
#define AD5933_REG_NUM_INCR_L        0x89    // 扫频点数 [7:0]
#define AD5933_REG_SETTLING_H        0x8A    // 建立时间 [10:8]
#define AD5933_REG_SETTLING_L        0x8B    // 建立时间 [7:0]
#define AD5933_REG_STATUS            0x8F    // 状态寄存器
#define AD5933_REG_TEMP_H            0x92    // 温度 [15:8]
#define AD5933_REG_TEMP_L            0x93    // 温度 [7:0]
#define AD5933_REG_REAL_H            0x94    // 实部 [15:8]
#define AD5933_REG_REAL_L            0x95    // 实部 [7:0]
#define AD5933_REG_IMAG_H            0x96    // 虚部 [15:8]
#define AD5933_REG_IMAG_L            0x97    // 虚部 [7:0]
```

### 4.2 控制命令（写入 CTRL_H 高 4 位）

| 命令 | 宏 | 说明 |
|------|-----|------|
| 初始化起始频率 | `AD5933_CTRL_INIT_FREQ (0x1000)` | 将 DDS 设置到起始频率 |
| 启动扫频 | `AD5933_CTRL_START_SWEEP (0x2000)` | 开始自动扫频 |
| 递增频率 | `AD5933_CTRL_INCREMENT_FREQ (0x3000)` | 步进到下一频点 |
| 重复频率 | `AD5933_CTRL_REPEAT_FREQ (0x4000)` | 重复当前频点测量 |
| 测量温度 | `AD5933_CTRL_MEASURE_TEMP (0x9000)` | 启动温度转换 |
| 掉电 | `AD5933_CTRL_POWER_DOWN (0xA000)` | 低功耗模式 |
| 待机 | `AD5933_CTRL_STANDBY (0xB000)` | 待机模式 |

> **宏的位宽度**：以上宏均为 16 位值（与电压/PGA 宏一致）。写入 CTRL_H 寄存器（8 位）时统一使用 `>> 8`：`(uint8_t)((AD5933_CTRL_STANDBY >> 8) | AD5933_VOLT_2000MV | AD5933_PGA_X1)`。

### 4.3 状态寄存器位 (0x8F)

| 位 | 宏 | 说明 |
|----|-----|------|
| D0 | `AD5933_STATUS_TEMP_VALID (0x01)` | 温度转换有效 |
| D1 | `AD5933_STATUS_DATA_VALID (0x02)` | 实部/虚部数据有效 |
| D2 | `AD5933_STATUS_SWEEP_DONE (0x04)` | 扫频完成 |

### 4.4 核心 API

```c
// ===== 初始化和基础 I2C =====
void    tsp_ad5933_init(void);                       // 复位 + 设外部时钟 + 待机
uint8_t tsp_ad5933_read_reg(uint8_t reg_addr);       // 读单寄存器
void    tsp_ad5933_write_reg(uint8_t reg_addr, uint8_t data);  // 写单寄存器
void    tsp_ad5933_write_block(uint8_t reg_addr, uint8_t *data, uint8_t len);  // 块写

// ===== 温度（已验证）=====
uint8_t tsp_ad5933_read_status(void);                // 读状态寄存器
float   tsp_ad5933_read_temperature(void);            // 返回摄氏度

// ===== 扫频（API 已就绪，待联调）=====
void    tsp_ad5933_set_sweep(uint32_t start_hz,      // 起始频率 Hz
                             uint32_t delta_hz,      // 频率步进 Hz
                             uint16_t num_increments, // 增量数（总点数 = num_increments + 1）
                             uint16_t settling_cycles, // 建立时间基础周期数
                             uint16_t settle_multiplier); // 倍率：AD5933_SETTLE_X1/X2/X4
void    tsp_ad5933_start_sweep(void);                 // 启动扫频
int16_t tsp_ad5933_read_real(void);                   // 读实部 (two's complement)
int16_t tsp_ad5933_read_imag(void);                   // 读虚部 (two's complement)
```

### 4.5 温度转换公式

```c
// AD5933 datasheet: 14-bit two's complement (D15-D14 为无关位), 0.03125°C/LSB
// 先掩码到 14 位，再从 D13 做符号扩展
uint16_t code = raw & 0x3FFF;
int16_t signed_code;
if (code & 0x2000) {
    signed_code = (int16_t)(code - 0x4000);  // 负温度
} else {
    signed_code = (int16_t)code;              // 正温度
}
float temp = signed_code / 32.0f;
```

### 4.6 典型使用流程

```c
// 1. 初始化 AD5933（复位 + 外部时钟 + 待机）
tsp_ad5933_init();

// 2. 读温度（验证 I2C 通信和芯片响应）
float temp = tsp_ad5933_read_temperature();

// 3. 配置扫频参数（100 个频点 → 写入 99 个增量）
tsp_ad5933_set_sweep(
    1000,                 // 起始频率 1 kHz
    100,                  // 步进 100 Hz
    99,                   // 增量数 = 100 点 - 1
    100,                  // 建立时间 100 个输出周期
    AD5933_SETTLE_X1      // ×1 倍率
);

// 4. 启动扫频
tsp_ad5933_start_sweep();

// 5. 逐点读取（主机在每个频点读取数据后，发送 Increment Frequency 命令进入下一频点）
const uint16_t num_increments = 99;
for (uint16_t i = 0; i <= num_increments; i++) {
    // 等待数据有效
    while (!(tsp_ad5933_read_status() & AD5933_STATUS_DATA_VALID));
    int16_t real = tsp_ad5933_read_real();
    int16_t imag = tsp_ad5933_read_imag();
    float mag = hypotf((float)real, (float)imag);
    // 处理当前频点数据...

    // 递增到下一频点（最后一个频点后不递增）
    if (i < num_increments) {
        tsp_ad5933_write_reg(AD5933_REG_CTRL_H,
            (uint8_t)((AD5933_CTRL_INCREMENT_FREQ >> 8) | AD5933_VOLT_2000MV | AD5933_PGA_X1));
    }
}

// 6. 进入掉电
tsp_ad5933_write_reg(AD5933_REG_CTRL_H,
    (uint8_t)(AD5933_CTRL_POWER_DOWN >> 8));
```

### 4.7 文件位置

| 文件 | 内容 |
|------|------|
| `NUEDC2025/tsp_ad5933.h` | 寄存器宏、控制命令、API 声明 |
| `NUEDC2025/tsp_ad5933.c` | I2C 块写、寄存器读写、温度/扫频 API 实现 |
| `iar/empty_mspm0g3519.c` | `action_ad5933_test()` 菜单回调 |
| `iar/ti_msp_dl_config.c` | I2C1 初始化（SysConfig 生成） |
| `iar/empty_mspm0g3519.syscfg` | SysConfig I2C1 配置 |

---

## 5. SysConfig 配置

```javascript
// I2C1 = AD5933 控制总线
I2C1.$name                     = "I2C_AD5933";
I2C1.basicEnableController     = true;
I2C1.basicControllerBusSpeed   = 100000;       // 100 kHz 标准模式
I2C1.peripheral.$assign        = "I2C1";
I2C1.peripheral.sdaPin.$assign = "PA30";
I2C1.peripheral.sclPin.$assign = "PA29";
```

> **注意**：AD5933 支持最高 400 kHz (Fast Mode) I2C，当前保守使用 100 kHz 标准模式。调试通过后可根据电磁环境提升速率。

---

## 6. 故障排查

| 症状 | 最可能原因 | 验证方法 |
|------|-----------|---------|
| 所有寄存器返回 0x00 或 0xFF | J19 跳线未按原理图安装 | 断电测 J19-3↔J19-4 (VNA_SCL↔PA29)、J19-5↔J19-6 (VNA_SDA↔PA30) 通断；J19-1/2 为电源、7/8 为 GND |
| 所有寄存器返回 0xFF (I2C 超时) | SDA/SCL 短路或上拉缺失 | 测 PA29/PA30 对 GND 电阻，应为高阻；测对 3V3 电阻应见 ~2.2kΩ |
| 寄存器值不变或乱码 | MCLK 时钟缺失 | 示波器测 X2 Pin 3 有无 16MHz |
| 温度值异常（如恒定 0°C） | 芯片未退出复位或 TEMP 命令未正确写入 | 检查 CTRL_H 是否写入 0x90，轮询 STATUS D0 位确认转换完成 |
| 温度值跳动大 | 未掩码温度寄存器 D15-D14 无关位，或符号扩展错误 | 确认代码先做 `raw & 0x3FFF` 再 14-bit 符号扩展 |
| 扫频无数据 | PGA 增益或激励电压不匹配待测阻抗 | 先用纯电阻校准，确认 GainFactor |
| I2C 偶尔超时 | 100kHz 速率在长飞线下不稳 | 降低至 50kHz 或检查 J19 接触及上拉电阻 |

---

## 7. 已知限制

- **校准未实现**：GainFactor 标定（需已知电阻）和系统相位补偿尚未编写
- **RFB 固定 20kΩ**：板载 R37，不可软件调节。待测阻抗远小于 RFB 时 VIN 信号微弱，远大于 RFB 时可能 ADC 饱和。更换 RFB 需焊接替换 R37
- **J15 仅两路信号**：SENSE_IN (Pin 4) 和 VOUT_BUF (Pin 5)，其余为 NC 或 GND。不支持四线 Kelvin 接法

---

## 8. 已修复 Bug

### 8.1 read_temperature() 命令截断（2026-07-22）

**现象**：温度始终显示 `--.-C (no resp)`，500ms 后超时。

**根因**：`tsp_ad5933_read_temperature()` 中将 16-bit 宏 `AD5933_CTRL_MEASURE_TEMP (0x9000)` 直接传给 `tsp_ad5933_write_reg(reg, uint8_t data)`，被截断为 `0x00`（NOP 命令）。温度测量从未启动，TEMP_VALID 位始终为 0。

**修复**：与其他 CTRL_H 写入一致，使用 `>> 8` 提取高字节，同时保持外部时钟位：
```c
// 修复前
tsp_ad5933_write_reg(AD5933_REG_CTRL_H, AD5933_CTRL_MEASURE_TEMP);  // → 0x00 (NOP!)
tsp_ad5933_write_reg(AD5933_REG_CTRL_L, 0x00);                       // → 丢失外部时钟

// 修复后
tsp_ad5933_write_reg(AD5933_REG_CTRL_H,
    (uint8_t)(AD5933_CTRL_MEASURE_TEMP >> 8));                       // → 0x90 (正确)
tsp_ad5933_write_reg(AD5933_REG_CTRL_L, AD5933_CLK_EXTERNAL);        // → 0x08 (保持外部时钟)
```

### 8.2 read_temperature() 轮询无超时（2026-07-22）

**现象**：进入 AD5933 Test 后 LCD 仅显示 Row 0/1，Row 2+ 无显示，PUSH 键无法退出，系统完全卡死。

**根因**：温度转换的 `while (!TEMP_VALID)` 轮询循环无超时保护。AD5933 未完成测量时，代码永远阻塞在 `read_temperature()` 内，无法到达 live loop（包含按键扫描）。

**修复**：添加 500ms 超时（datasheet 典型值 ~30ms），超时返回 `NAN`；调用侧检测 NaN 后显示 `T: --.-C (no resp)`（黄色），然后**继续进入 live loop**，保证 PUSH 键始终可退出。

### 8.3 温度不自动更新（2026-07-22）

**现象**：手指按压 AD5933 后温度显示不变，退出再进入菜单才看到新值。

**根因**：温度仅在前导初始化阶段读取一次，live loop 中只刷新 Real/Imag/Mag，温度从不更新。

**修复**：live loop 内添加温度定时刷新（每 20 次迭代 = ~1s），仅在值变化时写 LCD 避免闪烁。
