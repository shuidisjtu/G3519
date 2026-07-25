# 信号题模块实现进度

> 最后更新：2026-07-24
> 覆盖场景：阻抗/RLC 测量、信号发生、频率/幅度/相位测量、通用电压采集等

## 进度总览

| 模块 | 代码 | 硬件验证 | 状态 |
|------|------|---------|------|
| AD5933 阻抗测量 | 完整 | 部分 | 硬件阻断：R31 需换 20kΩ |
| AD9833 DDS 信号源 | 完整 | 已通过 | 可用 |
| AD8302 幅相检测 | 无代码 | 未验证 | 封存，有实施方案 |
| 通用 ADC (J2 三路) | 完整 | 待验证 | ADC0: VIN1/VIN3/VIN4 |
| 编码器 (旋钮) | 完整 | 在用 | 可用 |

## 各模块详情

### AD5933 阻抗测量

- **代码文件**：`NUEDC2025/tsp_ad5933.c/.h`
- **应用入口**：`action_ad5933_test()` — 标定 + 单频阻抗测量 + 温度刷新
- **已验证**：
  - I2C 通信（ACK at 0x0D, CTRL 回读 B0 08）
  - 温度读取（32.1°C，触摸响应正常）
  - 1kHz 激励输出（AD2 示波器确认）
  - 三个软件 Bug 已修复（命令截断 / BUSY 超时 / 温度刷新）
- **未通过**：GainFactor 标定 + DUT 阻抗测量
- **根因**：R36=20kΩ 导致信号链增益不足 200 倍，DUT 信号被芯片内部串扰淹没（SNR≈-46dB）
- **修复方案**：R31 从 100Ω 换为 20kΩ（测量范围 15kΩ~50kΩ），或 R36 从 20kΩ 换为 100Ω（通用方案），软件无需改动
- **待完成**：多频扫频（INCREMENT_FREQ 循环）仅有文档示例，未写入 action 代码
- **硬件限制**：RFB 固定 20kΩ (板载 R37)，J15 仅两线接口
- **参考文档**：`development_reference/AD5933_Use.md`、`AD5933_Examine.md`、`AD5933_Debug_Report.md`

### AD9833 DDS 信号源

- **代码文件**：`NUEDC2025/tsp_dds.c/.h`
- **应用入口**：`action_dds_test()` — 封存在 `#if 0` 中
- **已验证**：方波/正弦/三角波均通过 AD2 在 J22 确认输出正确
- **参考文档**：`development_reference/AD9833_DDS_Use.md`

### AD8302 幅相检测

- **代码文件**：无
- **封存文档**：`development_reference/AD8302_Use_Mothball.md`
- **封存原因**：4 项阻断问题——PB14/PB13 非 ADC 引脚需飞线至 PA12/PA13、ADC 模式需改 Sequence、相位公式需修正、悬空验证无效
- **实施方案**：文档中已记录修正方案（飞线接法、ADC0 配置、公式），可快速实现

### 通用 ADC (J2 三路模拟输入)

- **代码文件**：`NUEDC2025/tsp_adc.c/.h`
- **应用入口**：`action_adc_test()` — 电压显示 + 频率测量 + 通道切换
- **已实现通道**：VIN1(ADC0-CH2, PA25), VIN3(ADC0-CH3, PA24), VIN4(ADC0-CH5, PB24)
- **功能**：
  - 实时电压显示（12-bit ADC, 0~3300mV）
  - 频率测量（双速率 burst + 过零检测，~10Hz~165kHz）
  - S0/S1 切换通道
- **SysConfig**：ADC0, ULPCLK 40MHz, 2.5μs 采样时间, 轮询模式
- **待扩展**：VIN2(ADC1-CH11, PB23), VIN5(ADC1-CH12, PA23) 需添加 ADC1 实例
- **硬件**：J2 排座，各路带 49.9Ω + 220pF 抗混叠滤波
- **参考**：`development_reference/G3519_main_board.md` §5.1

### 编码器 (旋钮)

- **代码文件**：`NUEDC2025/tsp_encoder.c/.h`
- **状态**：完整可用，已在 CCD Test 和 DDS Test 中作为 UI 交互组件使用
