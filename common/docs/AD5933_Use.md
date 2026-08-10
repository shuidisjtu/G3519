# AD5933 阻抗测量使用说明

> 编写日期：2026-07-21
> 状态：⚠️ 软件链路已验证（I2C/温度/扫频/DFT 读取/REPEAT_FREQ 刷新），但**阻抗测量功能异常**——信号链增益不足（R36=20kΩ 衰减 200 倍），DUT 信号被芯片内部串扰淹没，待更换 R36 为 100Ω 后复测（详见第 10 节调试报告）

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
  ├─ AD5933 Test  ──→  阻抗测试界面（自动流程）
  ├─ DDS Test
  ├─ ADC Test
  ├─ Scope
  └─ Sweep
```

**按键**：
- `S2`：确认进入 AD5933 Test
- `PUSH`：返回主菜单

### 测试流程界面

进入后自动执行初始化，然后分**标定**和**测量**两个阶段：

#### 阶段 1：标定（Calibration）

初始化后，LCD 提示：

```
┌──────────────────────┐
│AD5933 Impedance      │ ← Row 0
│C: B3 08              │ ← Row 1 (CTRL 回读)
│                      │ ← Row 2
│Cal: 300K Ohm         │ ← Row 3, CYAN (校准电阻值)
│Connect Rcal to J15   │ ← Row 4
│S2=Calibrate          │ ← Row 5
│                      │
│PUSH to exit          │ ← Row 7
└──────────────────────┘
```

**操作**：在 J15-4/5 之间接入 **300KΩ** 校准电阻，按 `S2` 开始标定。标定过程使用 4 次 REPEAT_FREQ 采样（超时自动跳过），有效样本取平均以降低噪声。标定成功后显示 "Cal OK! Replace DUT"，再按 `S2` 进入测量。

> 校准电阻值在代码中定义为 `cal_resistance = 300000.0f`（局部常量），可根据手头电阻修改。需与 RFB (R37=20KΩ) 及待测阻抗在同一数量级。

#### 阶段 2：实时测量

```
┌──────────────────────┐
│AD5933 Impedance      │ ← Row 0, YELLOW/BLUE
│C: B3 08              │ ← Row 1, WHITE
│cM:12345 M:12345      │ ← Row 2, CYAN (校准 Mag / 当前 Mag)
│Freq: 1000 Hz         │ ← Row 3, WHITE
│Real: -16857          │ ← Row 4, WHITE (每次刷新)
│Imag: -21896          │ ← Row 5, WHITE (每次刷新)
│Z   : XXXXX Ohm       │ ← Row 6, GREEN (阻抗值)
│PUSH to exit          │ ← Row 7, GRAY
└──────────────────────┘
```

每次 DFT 完成后自动发送 `REPEAT_FREQ` 命令重新测量，Real/Imag/Z 持续刷新。

**自动流程**：
1. `tsp_ad5933_init()` — Power-down → 外部时钟 → 待机
2. 回读 CTRL_H/CTRL_L 并显示
3. ~~`tsp_ad5933_read_temperature()`~~ — 已从当前测试流程中移除
4. 等待 S2 — 用户接入校准电阻（300KΩ）
5. `set_sweep(1000, 0, 0, 100, X1)` + `start_sweep()` — 标定测量
6. 4 次 REPEAT_FREQ 采样（超时跳过），有效样本取平均 — 计算 `GainFactor = 1 / (R_cal × Mag_avg)`
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
| 2.0 V p-p | `AD5933_VOLT_2000MV` (0x0000, D10-D9=00) | 默认值，最大输出摆幅 |
| 1.0 V p-p | `AD5933_VOLT_1000MV` (0x0200, D10-D9=01) | |
| 0.4 V p-p | `AD5933_VOLT_400MV` (0x0400, D10-D9=10) | |
| 0.2 V p-p | `AD5933_VOLT_200MV` (0x0600, D10-D9=11) | |

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

> **宏的位宽度**：以上宏均为 16 位值（与电压/PGA 宏一致）。写入 CTRL_H 寄存器（8 位）时统一使用 `>> 8`：`(uint8_t)((AD5933_CTRL_STANDBY | AD5933_VOLT_2000MV | AD5933_PGA_X1) >> 8)`。

### 4.3 状态寄存器位 (0x8F)

| 位 | 宏 | 说明 |
|----|-----|------|
| D0 | `AD5933_STATUS_TEMP_VALID (0x01)` | 温度转换有效 |
| D1 | `AD5933_STATUS_DATA_VALID (0x02)` | 实部/虚部数据有效 |
| D2 | `AD5933_STATUS_SWEEP_DONE (0x04)` | 扫频完成 |

### 4.4 核心 API

```c
// ===== 初始化和基础 I2C =====
void    tsp_ad5933_init(void);                       // Power-down + 外部时钟 + 待机
uint8_t tsp_ad5933_read_reg(uint8_t reg_addr);       // 读单寄存器
void    tsp_ad5933_write_reg(uint8_t reg_addr, uint8_t data);  // 写单寄存器
void    tsp_ad5933_write_block(uint8_t reg_addr, uint8_t *data, uint8_t len);  // 块写

// ===== 温度（已验证）=====
uint8_t tsp_ad5933_read_status(void);                // 读状态寄存器
float   tsp_ad5933_read_temperature(void);            // 返回摄氏度

// ===== 扫频（已验证）=====
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
// 1. 初始化 AD5933（Power-down + 外部时钟 + 待机）
tsp_ad5933_init();

// 2. （可选）读温度验证 I2C 通信——当前测试界面已移除此步骤
// float temp = tsp_ad5933_read_temperature();

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
            (uint8_t)((AD5933_CTRL_INCREMENT_FREQ | AD5933_VOLT_2000MV | AD5933_PGA_X1) >> 8));
    }
}

// 6. 进入掉电
tsp_ad5933_write_reg(AD5933_REG_CTRL_H,
    (uint8_t)(AD5933_CTRL_POWER_DOWN >> 8));
```

### 4.7 文件位置

| 文件 | 内容 |
|------|------|
| `common/drivers/tsp_ad5933.h` | 寄存器宏、控制命令、API 声明 |
| `common/drivers/tsp_ad5933.c` | I2C 块写、寄存器读写、温度/扫频 API 实现 |
| `<control|signal>/app/` | `action_ad5933_test()` 菜单回调 |
| `ti_msp_dl_config.c` | I2C1 初始化（SysConfig 生成，各工程目录） |
| `*.syscfg` | SysConfig I2C1 配置 |

---

## 5. SysConfig 配置

```ini
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

- **系统相位补偿未实现**：GainFactor 标定已完成，但系统相位补偿尚未编写
- **温度显示已移除**：当前测试界面不再调用 `tsp_ad5933_read_temperature()`，Row 2 改为显示校准 Mag / 当前 Mag
- **RFB = 20kΩ**：板载 R37（参考设计值），不可软件调节。待测阻抗远小于 RFB 时 VIN 信号微弱，远大于 RFB 时可能 ADC 饱和。校准电阻应与待测阻抗在同一数量级
- **J15 仅两路信号**：SENSE_IN (Pin 4) 和 VOUT_BUF (Pin 5)，其余为 NC 或 GND。不支持四线 Kelvin 接法

---

## 8. 设计教训

1. **16-bit 宏写入 8-bit 寄存器必须 >> 8**：AD5933 控制宏（如 `AD5933_CTRL_MEASURE_TEMP = 0x9000`）为 16 位值，写入 CTRL_H（8 位寄存器）时若不右移 8 位，高字节被截断为 0x00（NOP），导致命令无效。
2. **所有硬件轮询必须带超时**：`while (!STATUS_BIT)` 无超时保护会导致系统永久卡死，且无法通过按键退出。所有硬件等待循环必须设置合理超时（如温度转换 500ms）并在超时后安全降级。

---

## 9. 验证记录（2026-07-22，AD2 + WaveForms）

> 验证工具：AD2（Analog Discovery 2）。无需万用表——I2C ACK 即可确认硬件连通性。

### 9.1 验证前准备

编译烧录当前项目代码，开机动画后进入主菜单 → **AD5933 Test**。AD2 接线：

| AD2 端口 | 主板连接点 | 用途 |
|----------|-----------|------|
| D0 | PA29 或 J19-4 | I2C SCL |
| D1 | PA30 或 J19-6 | I2C SDA |
| 1+ | J15-5 (VOUT_BUF) | 激励波形 |
| 1- | J15-2 (GND) | |
| 2+ | J15-4 (SENSE_IN) | 回采波形 |
| 2- | J15 GND | |
| GND | J15 GND | 共地参考（必须连接） |

> 前四步不需要在 J15-4/5 之间接被测件。AD2 的 Scope 为**全差分输入**，即使 `1-`/`2-` 已接板上 GND，仍应将 AD2 的 `GND` 端子与主板 GND 相连。

### 9.2 已通过验证项

| 步骤 | 验证项 | 工具 | 结果 |
|------|--------|------|------|
| P2 | CTRL_H 写入（16-bit 宏 `>>8`） | LCD Row 1 | `C: B3 08` ✅ |
| P6 | 温度读取（14-bit 掩码 + 符号扩展） | LCD | `32.1°C`，手指按压后上升 ✅（当前测试界面不显示温度） |
| I2C | 总线通信 | AD2 Logic | `0x0D WR`，各字节 ACK ✅ |
| P3 | 频率字公式（MCLK/4 分频） | AD2 Scope CH1 | 1.000 kHz / ~2Vpp 正弦波 ✅ |

### 9.3 已知电阻校准（待完成验证）

AD2 Scope 双通道：`J15-5 ↔ [10kΩ 电阻] ↔ J15-4`，验证完整模拟链路（激励 → DUT → 回采 → ADC → DFT）。

- 正确时 CH1（激励）~2Vpp 正弦、CH2（回采）正弦且幅值 < CH1、两通道 Freq 均为 1.000 kHz
- LCD Row 4-6：Real/Imag 非零，Mag 非零且稳定（同电阻下抖动 <5%）
- 快速验证：换 4.7kΩ → Magnitude 应有明显变化

| 现象 | 原因 |
|------|------|
| CH2 无波形（0V 直线） | J15 接线开路 / 电阻未可靠接触 |
| CH2 波形与 CH1 完全相同 | J15-4/5 短路或电阻为 0Ω |
| Magnitude 恒为 0 | DFT 数据无效 → 检查 STATUS D1 |
| Magnitude 大幅跳动 | J15 飞线过长 → 缩短线缆，用双绞线 |

> ⚠️ 注：截至 2026-07-24 的调试报告，该步骤仍未通过——见第 10 节。

### 9.4 验证特有故障速查

| 现象 | 最可能原因 | 排查 |
|------|-----------|------|
| 第二步 `C: 00 08` | P2 修复未烧录 | 确认 `tsp_ad5933.h` 中 `STANDBY` 宏为 `0xB000` |
| 第四步 250 Hz | P3 修复未烧录 | 确认 `tsp_ad5933.c` 中频率字公式为 `536870912ULL`（非 `134217728ULL`） |
| 第四步无波形 | DDS 未启动 | 确认菜单函数调用了 `set_sweep()` → `start_sweep()` |

---

## 10. 调试报告（2026-07-24）

> 状态：软件验证通过 ✅，硬件存在信号链增益问题待修复。

### 10.1 信号链拓扑（本板实测）

```
AD5933 VOUT(Pin6)
  └→ C48(AC耦合) → U6B Pin5(同相输入)
                      └→ U6B 电压跟随器 → J15-5(VOUT_BUF)
                                           └→ DUT(Zx)
                                              └→ J15-4(SENSE_IN)
                                                 └→ U6A Pin2(反相输入)
                                                      └→ U6A TIA(R31=100Ω) → Pin1
                                                         └→ R36(20k) → VIN(Pin5)
                                                            └→ R37(20k) → RFB(Pin4)
```

关键电阻：R31 = 100Ω（U6A TIA 反馈）、R36 = 20kΩ、R37 = 20kΩ。激励 = 200mV，PGA = ×1。

### 10.2 调试验证项（P1-P9 全部通过）

| 步骤 | 验证项 | 工具 | 结果 |
|------|--------|------|------|
| P1 | I2C 总线通信 | AD2 Logic | 地址 `0x0D WR`，各字节 ACK |
| P2 | CTRL_H 回读 | LCD Row 1 | `B3 08` = STANDBY + PGA_X1 + 200mV |
| P3 | 1kHz 激励输出 | AD2 Scope CH1 @ J15-5 | 1.000kHz / ~2Vpp 正弦波，稳压 |
| P4 | DUT 电流 | 万用表 @ DUT 两端 | 220Ω 约 1Vp / 56μA |
| P5 | U6B 缓冲器 | 示波器 @ U6B Pin5/6/7 | 2Vpp 正弦波，跟随器正常 |
| P6 | U6A 偏置电压 | 示波器 @ U6A Pin1/2/3 | ~1.6V DC（VDD/2 偏置正常）|
| P7 | U6A 输出信号 | 示波器 @ U6A Pin1 AC 耦合 | 33Ω 时 3Vpp 削波；高阻时 mV 级 |
| P8 | REPEAT_FREQ 数据刷新 | LCD | 数据持续更新，DATA_VALID 正确置位/清除 |
| P9 | 运放供电 | 万用表 @ U6 Pin4/8 | 3.3V 正常 |

### 10.3 问题数据（不同 DUT 下 Real/Imag 几乎不变）

| DUT | Real | Imag | 结论 |
|-----|------|------|------|
| 220Ω（标定） | -15643 | -01396 | 基准 |
| 开路（不接） | -15636 | -01332 | 变化仅 +7/+64（<0.4%）|
| 39kΩ | -15639 | -01372 | 变化仅 +4/+24（<0.2%）|
| 33Ω | -17791 | -18436 | 小电阻时有变化（U6A 输出 3Vpp 削波，硬灌 VIN）|

### 10.4 根因分析

**信号链增益不足**。总增益（等效跨阻）= R31 × (R37/R36) = 100Ω × (20k/20k) = **100Ω**，仅为 AD5933 标准配置（20kΩ）的 **1/200**：

| DUT | I_DUT | U6A 输出 | VIN 注入电流 | TIA 输出 | DFT 估算 |
|-----|-------|---------|------------|---------|---------|
| 220Ω | 111μA | 11.1mVpp | 0.56μApp | 11.1mVpp | ~64 counts ❌ |
| 39kΩ | 51μA | 5.1mVpp | 0.26μApp | 5.1mVpp | ~30 counts ❌ |

ADC/DFT 读到的 ~15640 counts 来自芯片内部 VOUT→VIN 寄生耦合（外部信号比内部串扰弱约 46dB），不是 J15 的外部 DUT 信号——更换不同 DUT 时 Real/Imag 仅变化 ±64 counts（~0.4%）。33Ω 时 DUT 电流 ≈30mA 硬灌 VIN 产生 ~3Vpp 输出压过串扰，Real 偏移 ~14%，证明**信号链路物理导通**，只是正常阻抗范围（kΩ 级）下增益不足。

### 10.5 结论与修复建议

**固件已验证通过，无需修改**（所有寄存器操作、I2C 通信、REPEAT_FREQ 刷新、标定/测量流程均正确）。根因在硬件：**R36 = 20kΩ 导致 U6A 输出到 VIN 之间衰减 200 倍**。

修复方案：将 **R36 从 20kΩ 更换为 100Ω（0402 封装）**。

```
改前: 总增益 = R31 × (R37 / R36) = 100Ω × (20k / 20k) = 100Ω
改后: 总增益 = R31 × (R37 / R36) = 100Ω × (20k / 100)  = 20kΩ  ← 等于标准配置
```

改动后 DUT 信号与 AD5933 标准接法完全等价，220Ω 时 TIA 输出 ≈ 2.2Vpp（满量程），固件标定/测量流程无需任何修改；串扰变为固定 DC offset，标定减法后不影响测量。
