# AD8302 幅相检测模块（封存）

> 状态：**封存**。原理图已核对、实施计划已审查、4 项阻断问题已确认。赛时需要时按修正方案快速启动。

---

## 1. 硬件信息（原理图核对完成）

> 核对依据：`M0G3519电路图.pdf`（U7 / J14 / J21 区域）。
> 结论：AD8302（U7，`AD8302ARUZ`）的测量输出在本版原理图中**未接入 MCU 的 ADC**，仅引至 2×3 排针 J14。

### 1.1 AD8302 模拟输出

| AD8302 输出 | U7 引脚 | 原理图网络名 | 实际去向 |
|---|---:|---|---|
| VMAG | 13 | `V_MAG` | 经 R17（0 Ω）→ J14-1 |
| VPHS | 9 | `V_PHS` | 经 R28（0 Ω）→ J14-5 |
| VREF | 11 | `V_REF` | 直接接 J14-3 |

### 1.2 J14 针脚定义（2×3 排针）

| 针脚 | 信号 | 说明 |
|---:|---|---|
| 1 | `V_MAG` | U7-13 VMAG 经 R17（0 Ω） |
| 2 | GND | — |
| 3 | `V_REF` | U7-11 VREF 直接连接 |
| 4 | GND | — |
| 5 | `V_PHS` | U7-9 VPHS 经 R28（0 Ω） |
| 6 | GND | — |

### 1.3 J21 针脚定义（2×4 输入排针）

| 针脚 | 信号 | 说明 |
|---:|---|---|
| 1 | `INPA` | 经 71.5Ω + 95.3Ω×2 + 1nF → U7-2 |
| 2 | `INPA` | 与 J21-1 同网 |
| 3 | 未接 AD8302 | 与 J21-4 形成孤立短接网络 |
| 4 | 未接 AD8302 | 与 J21-3 形成孤立短接网络 |
| 5 | `INPB` | 经 71.5Ω + 95.3Ω×2 + 1nF → U7-6 |
| 6 | `INPB` | 与 J21-5 同网 |
| 7 | 未接 AD8302 | 与 J21-8 形成孤立短接网络 |
| 8 | 未接 AD8302 | 与 J21-7 形成孤立短接网络 |

### 1.4 供电与滤波

- **供电**：3.3V（`MCU_3V3`），旁路 C20(4.7µF) + CF7(0.1µF) + C22(100pF)
- **MFLT**：U7-14 接 C24（0.1µF）到 GND
- **PFLT**：U7-8 接 C45（0.1µF）到 GND
- **测量模式**：VMAG↔MSET、VPHS↔PSET 通过 R24/R25（各 1kΩ）接到 VREF

### 1.5 现有 ADC 占用情况

| 功能 | ADC 通道 | GPIO | 原理图网络标号 |
|---|---|---|---|
| CCD1 | ADC1 CH5 | PB18 | `A1-5` |
| CCD2 | ADC1 CH6 | PB19 | `A1-6` |
| VIN1 | ADC0 CH2 | PA25 | `A0-2` |
| VIN2 | ADC1 CH11 | PB23 | `A1-11` |
| VIN3 | ADC0 CH3 | PA24 | `A0-3` |
| VIN4 | ADC0 CH5 | PB24 | `A0-5` |
| VIN5 | ADC1 CH12 | PA23 | `A1-12` |
| Vsense | ADC0 CH4 | PB25 | `A0-4` |

---

## 2. AD8302 传输函数

### 2.1 增益输出（VMAG）

$$V_{MAG} \approx 900\text{mV} + 30\text{mV/dB} \times (P_{INPA} - P_{INPB})$$

- 标称斜率：**30 mV/dB**
- 中心点（0 dB，两路幅度相等）：**900 mV**
- 范围：−30 dB ≈ 30 mV，+30 dB ≈ 1.8 V
- 高频下斜率略有下降（1.9 GHz: 27.5 mV/dB; 2.2 GHz: 27.5 mV/dB）

```c
gain_db = (vmag_mv - 900) / 30.0f;   // > 0 = VINA > VINB (放大)
```

### 2.2 相位输出（VPHS）

$$V_{PHS} = \pm V_F(\Phi - 90°) + V_{CP}$$

- $V_F = 10\text{mV/°}$，$V_{CP} = 900\text{mV}$
- **选定 0°–180° 分支**（负斜率）：

```c
phase_deg = (1800.0f - vphs_mv) / 10.0f;  // 0°→1.8V, 90°→0.9V, 180°→0V
```

| VPHS 电压 | 相位差 |
|----------|--------|
| ~1.8 V | 0° |
| ~0.9 V | 90° |
| ~0 V | 180° |

> ⚠️ AD8302 只能给出 180° 无歧义区间，不能区分正/负相位。

---

## 3. 输入注意事项

- INPA/INPB 必须**交流耦合**，阻抗/终端匹配，且同时处于有效输入范围内（约 −60 至 0 dBm @ 50Ω）
- **输入悬空不是受保证的工作状态**——不能据此设计 pass/fail 判断
- VMAG/VPHS 输出带宽由 MFLT/PFLT 电容（0.1µF）决定，适合准静态显示

---

## 4. 审查发现的阻断问题（2026-07-22）

原实施计划 `docs/superpowers/plans/2026-07-22-AD8302-implementation.md` 存在 4 项阻断问题，**必须修正后才能实施**。

| # | 问题 | 详细 | 修正方案 |
|---|------|------|---------|
| 1 | **PB14/PB13 不是 ADC 输入** | PB14 已被 SPI1 (LCD) 占用；PB13 无 ADC 模拟通道（LQFP-100 引脚 1 为纯数字 GPIO） | 改用 **PA12/A0_8 (VMAG) + PA13/A0_9 (VPHS)**，运行时切 IOMUX |
| 2 | **ADC 采样模式不匹配** | `initSingleSample` 设 STARTADD=ENDADD=0，MEM1 不在序列中；两次单独 `startConversion()` 采样时刻不同 | 改用 `DL_ADC12_initSeqSample`，STARTADD=0, ENDADD=1，一次触发双通道 |
| 3 | **相位公式错误** | 原代码 `(vphs-900)/10` 将 0° 误算为 90° | 改为 `(1800 - vphs) / 10` |
| 4 | **悬空验证无依据** | "无输入≈900mV" 不是规格值，不能作为 pass/fail 判据 | 删除悬空验证，用两个有效交流耦合信号验证 |

---

## 5. 修正后的实施概要

### 5.1 硬件飞线

| 信号 | 从 | 到 | GPIO | ADC 通道 |
|------|----|----|------|---------|
| VMAG | J14-1 | PA12 (QFP 51 / J5-35) | PA12 | ADC0 CH8 (A0_8) |
| VPHS | J14-5 | PA13 (QFP 52 / J5-36) | PA13 | ADC0 CH9 (A0_9) |
| GND | J14-2 或 J14-4 | GND | — | — |

### 5.2 ADC 配置（修正后）

- **ADC 实例**：ADC0（两路在同一实例，可 sequence）
- **模式**：Sequence（`DL_ADC12_initSeqSample`），非重复
- **STARTADD**：MEM0，**ENDADD**：MEM1
- **触发**：软件触发，一次 `startConversion()` 完成 MEM0→MEM1
- **参考电压**：内部 VREF 2.5V
- **IOMUX**：`tsp_ad8302_init()` 中将 PA12/PA13 从 GPIO 临时切为模拟模式；退出时恢复

### 5.3 采样结构（推荐）

```c
typedef struct {
    uint16_t vmag_mv;
    uint16_t vphs_mv;
} ad8302_sample_t;

bool tsp_ad8302_read(ad8302_sample_t *sample);  // 一次触发，双通道
```

LCD 刷新使用同一对数据计算 gain 和 phase，保证一致性。

### 5.4 相位公式（修正后）

```c
float tsp_ad8302_phase_deg(void) {
    int16_t vphs_mv = ...;
    float phase = (1800.0f - vphs_mv) / 10.0f;
    if (phase < 0.0f)   phase = 0.0f;
    if (phase > 180.0f) phase = 180.0f;
    return phase;
}
```

---

## 6. 启动前检查清单

- [ ] 确认 PA12/PA13 在 SysConfig 中未新增冲突
- [ ] 确认 AD8302 处于测量模式（VMAG↔MSET、VPHS↔PSET 闭环）
- [ ] 确认两路 RF/IF 输入均交流耦合且处于有效输入范围
- [ ] 确认 VMAG/VPHS 对 MCU 共地且不超过 2.5V ADC 参考范围
- [ ] 使用已知 0 dB、90° 信号作为初始标定点

---

## 7. 参考资料

- Analog Devices, [AD8302 Datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/ad8302.pdf)
- Analog Devices, [Accurate Gain/Phase Measurement at Radio Frequencies](https://www.analog.com/en/resources/analog-dialogue/articles/accurate-gain-phase-measurement-up-to-2-5-ghz.html)
- Texas Instruments, [MSPM0G3519 Datasheet](https://www.ti.com/lit/ds/symlink/mspm0g3519.pdf)
- 审查报告：`F:\Test\AD8302_实施计划审查报告.md`（2026-07-22）
