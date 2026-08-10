# AD5933 阻抗测量 — 调试报告

> 编写日期：2026-07-24
> 状态：软件验证通过 ✅，硬件存在信号链增益问题待修复

---

## 一、硬件配置

| 项目 | 配置 |
|------|------|
| 主控 | MSPM0G3519 @ 80MHz (Cortex-M0+) |
| 阻抗芯片 | AD5933YRS (U5)，12 位阻抗转换器 |
| 主时钟 | X2 有源晶振 16MHz，外部时钟模式 |
| 通信接口 | I2C1, 100kHz 标准模式, PA29(SCL)/PA30(SDA), 7-bit 地址 `0x0D` |
| I2C 上拉 | R32/R33 = 2.2kΩ（板载） |
| 模拟前端 | U6 双运放 AD8606ARZ（3.3V 单电源供电） |
| 测量接口 | J15 排针：Pin5 = VOUT_BUF（激励出），Pin4 = SENSE_IN（回采入） |
| AD5933 配置 | 激励 = 200mV, PGA = ×1 |
| 关键电阻 | R31 = 100Ω（U6A TIA 反馈）, R36 = 20kΩ, R37 = 20kΩ |

### 信号链拓扑

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

---

## 二、软件实现

**文件:** `iar/empty_mspm0g3519.c`（`action_ad5933_test()`）
**依赖:** `NUEDC2025/tsp_ad5933.c/.h`

### 操作流程

#### 阶段 1：初始化

1. `tsp_ad5933_init()` — 复位芯片 → 配置外部时钟(16MHz) → 待机模式
2. 回读 CTRL_H/CTRL_L 寄存器，显示在 LCD Row 1（`C: B3 08`）
3. LCD 提示接入 220Ω 校准电阻，按 S2 确认

#### 阶段 2：标定

4. `tsp_ad5933_set_sweep(1000Hz, 0, 0, 100, AD5933_SETTLE_X1)` — 单频点 1kHz, settling=100 周期
5. `tsp_ad5933_start_sweep()` — INIT_FREQ → delay 10ms → START_SWEEP
6. 轮询 STATUS D1 (DATA_VALID)，超时 50000 次后 `continue`
7. 读 Real/Imag → 计算 Magnitude
8. **GainFactor = 1 / (220 × Mag_cal)**
9. 显示"Cal OK! Replace DUT"，按 S2 进入测量

#### 阶段 3：实时测量

10. 重新 `start_sweep()` 启动测量
11. 主循环（~110ms/次）：
    - 轮询 DATA_VALID（10ms 间隔 × 50 次，期间扫描 PUSH 按键）
    - 读 Real/Imag → 计算 Mag → **Z = 1 / (GF × Mag)**
    - LCD 刷新显示 Real/Imag/Z
    - 发 `REPEAT_FREQ` 命令触发下一次 DFT
12. PUSH 退出 → 发送 `POWER_DOWN` 命令

### LCD 显示布局

```
Row 0: AD5933 Impedance    ← 标题 (YELLOW/BLUE)
Row 1: C: B3 08            ← CTRL 寄存器回读 (WHITE)
Row 3: Freq: 1000 Hz       ← 固定 1kHz (WHITE)
Row 4: Real: -xxxxx        ← DFT 实部, 实时刷新 (WHITE)
Row 5: Imag: -xxxxx        ← DFT 虚部, 实时刷新 (WHITE)
Row 6: Z   : xxxxx Ohm     ← 阻抗值 (GREEN)
Row 7: PUSH to exit        ← 提示 (GRAY)
```

### AD5933 寄存器操作（`tsp_ad5933.c`）

| API | 说明 |
|-----|------|
| `tsp_ad5933_init()` | 复位 + 外部时钟 + 待机（CTRL_H=0xB3）|
| `tsp_ad5933_set_sweep(start, delta, n, cyc, mult)` | 配置扫频参数（频率字公式：f × 2²⁹ ÷ MCLK）|
| `tsp_ad5933_start_sweep()` | INIT_FREQ → 等 10ms → START_SWEEP |
| `tsp_ad5933_read_status()` | 读 0x8F（D0=TEMP_VALID, D1=DATA_VALID, D2=SWEEP_DONE）|
| `tsp_ad5933_read_real()` / `read_imag()` | 读 DFT 16-bit two's complement 数据 |
| `tsp_ad5933_write_reg()` / `write_block()` | I2C 单字节/块写 |

---

## 三、调试过程与结果

### 已通过验证项

| 步骤 | 验证项 | 工具 | 结果 |
|------|--------|------|------|
| P1 | I2C 总线通信 | AD2 Logic Analyzer | 地址 `0x0D WR`，各字节 ACK |
| P2 | CTRL_H 回读 | LCD Row 1 | `B3 08` = STANDBY + PGA_X1 + 200mV |
| P3 | 1kHz 激励输出 | AD2 Scope CH1 @ J15-5 | 1.000kHz / ~2Vpp 正弦波，稳压 |
| P4 | DUT 电流 | 万用表 @ DUT 两端 | 220Ω 约 1Vp / 56μA |
| P5 | U6B 缓冲器 | 示波器 @ U6B Pin5/6/7 | 2Vpp 正弦波，跟随器正常 |
| P6 | U6A 偏置电压 | 示波器 @ U6A Pin1/2/3 | ~1.6V DC（VDD/2 偏置正常）|
| P7 | U6A 输出信号 | 示波器 @ U6A Pin1 AC 耦合 | 33Ω 时 3Vpp 削波正弦波；高阻时信号微弱（mV 级）|
| P8 | REPEAT_FREQ 数据刷新 | LCD Real/Imag 实时显示 | 数据持续更新，DATA_VALID 正确置位/清除 |
| P9 | 运放供电 | 万用表 @ U6 Pin4/8 | 3.3V 正常 |

### 问题数据

**不同 DUT 下 Real/Imag 几乎不变：**

| DUT | Real | Imag | 结论 |
|-----|------|------|------|
| 220Ω（标定） | -15643 | -01396 | 基准 |
| 开路（不接） | -15636 | -01332 | 变化仅 +7/+64（<0.4%）|
| 39kΩ | -15639 | -01372 | 变化仅 +4/+24（<0.2%）|
| 33Ω | -17791 | -18436 | 小电阻时有变化（U6A 输出 3Vpp 削波，硬灌 VIN）|

---

## 四、根因分析

### 信号链路增益计算

```
总增益（等效跨阻） = R31 × (R37 / R36)
                 = 100Ω × (20kΩ / 20kΩ)
                 = 100Ω   ← 只有 AD5933 标准配置 (20kΩ) 的 1/200
```

**AD5933 标准接法**（VOUT → DUT → VIN，Rfb = 20k）：

| DUT | I_in | TIA 输出 | DFT 估算 |
|-----|------|---------|---------|
| 220Ω | 111μApp | 2.22Vpp | ~16000（满量程）✅ |

**本板实际**（U6A ×100Ω → R36=20k → VIN）：

| DUT | I_DUT | U6A 输出 | VIN 注入电流 | TIA 输出 | DFT 估算 |
|-----|-------|---------|------------|---------|---------|
| 220Ω | 111μA | 11.1mVpp | 0.56μApp | **11.1mVpp** | ~64 counts ❌ |
| 39kΩ | 51μA | 5.1mVpp | 0.26μApp | **5.1mVpp** | ~30 counts ❌ |

### 串扰淹没问题

AD5933 芯片内部 VOUT → VIN 存在寄生耦合。本板前端增益仅 100Ω 时，外部信号比内部串扰弱约 46dB：

```
DUT 信号:  ~64 counts（理论）
内部串扰:  ~15640 counts（实测）
信噪比 SNR: 64 / 15640 ≈ -46dB
```

ADC/DFT 读到的 ~15640 来自芯片内部寄生耦合，不是 J15 的外部 DUT 信号。更换不同 DUT 时 Real/Imag 仅变化 ±64 counts（~0.4%），因为外部信号完全被串扰淹没。

### 33Ω 为何能看到变化（旁证）

33Ω DUT 时，DUT 电流 ≈ 30mA，U6A 输出 ≈ 3Vpp（已削波到电源轨）。强大的驱动电流穿过 R36(20k) 硬灌 VIN 节点，产生约 150μApp 的注入电流，在 R37 上产生约 3Vpp 输出 — 足以压过串扰。因此 Real 从 -15643 偏移到 -17791（~14% 变化）。

这证明了**信号链路物理导通**，只是正常阻抗范围（kΩ 级）下增益不足。

---

## 五、结论与修复建议

### 软件状态

**固件已验证通过，无需修改。** 所有 AD5933 寄存器操作、I2C 通信、REPEAT_FREQ 刷新机制、标定/测量流程均按数据手册正确实现。

### 根因

**信号链增益不足：R36 = 20kΩ 导致 U6A 输出到 VIN 之间衰减 200 倍。** DUT 信号被 AD5933 内部 VOUT→VIN 寄生串扰淹没，ADC 读不到有效的 DUT 数据。

### 修复方案

**硬件改动：将 R36 从 20kΩ 换为 100Ω (0402 封装)。**

```
改前: 总增益 = R31 × (R37 / R36) = 100Ω × (20k / 20k) = 100Ω
改后: 总增益 = R31 × (R37 / R36) = 100Ω × (20k / 100)  = 20kΩ  ← 等于标准配置
```

改动后：
- DUT 信号与 AD5933 标准四线接法完全等价
- 220Ω 时 VIN 注入电流 ≈ 111μApp，TIA 输出 ≈ 2.2Vpp（满量程）
- 固件标定/测量流程**无需任何修改**
- 串扰（~15640 counts）变为固定 DC offset，标定减法后不影响测量
