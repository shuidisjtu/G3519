# 信号题模块实现进度

> 最后更新：2026-07-26
> 覆盖场景：阻抗/RLC 测量、信号发生、频率/幅度/相位测量、通用电压采集等

## 进度总览

| 模块 | 代码 | 硬件验证 | 状态 |
|------|------|---------|------|
| AD5933 阻抗测量 | 完整 | 部分 | 硬件阻断：R31 需换 20kΩ |
| AD9833 DDS 信号源 | 完整 | 已通过 | 可用（S2 退出保持输出） |
| AD8302 幅相检测 | 完整（封存） | 未通过 | 封存：输入网络未焊 + 需 RF 信号 |
| 通用 ADC (J2 五路) | 完整 | 已通过 | ADC0+ADC1: VIN1~VIN5 |
| FFT 频谱分析 | 完整 | 已通过 | CMSIS-DSP Q15, 256 点, 频率/幅值/THD |
| Scope 波形显示 | 完整 | 已通过 | 160×96px, 三档时基, 自动量程, 差分更新 |
| Sweep 扫频分析仪 | 完整 | 已通过 | DDS+ADC 联动, 绝对量程, 1-2-5 频率标记 |
| UART 命令协议 | 完整 | 已通过 | UART0→PC, UART6→K230(待验证) |
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
- **应用入口**：`action_dds_test()` — 交互式波形/频率调整
- **已验证**：方波/正弦/三角波均通过 AD2 在 J22 确认输出正确
- **参考文档**：`development_reference/AD9833_DDS_Use.md`

### AD8302 幅相检测

- **代码文件**：`NUEDC2025/tsp_ad8302.c/.h`（已从编译移除，文件保留）
- **封存文档**：`development_reference/AD8302_Use_Mothball.md`
- **封存原因**：
  1. J21 输入匹配网络元件（R18/R29/R20/R30/R15/R27）未焊，信号无法到达 U7
  2. AD8302 为 RF 器件，1nF 交流耦合电容要求输入频率 >500kHz，不适合低频信号测量
  3. 备赛方向不涉及高频信号处理
- **已实现功能**：驱动代码完整（复用 tsp_adc VIN4/VIN5 通道，飞线 J14→J2），公式正确，但因硬件未焊无法验证
- **恢复方法**：焊接输入网络元件 → 将 tsp_ad8302.c/.h 加回 .ewp → 在 main 中恢复 include/action/菜单项

### 通用 ADC (J2 五路模拟输入)

- **代码文件**：`NUEDC2025/tsp_adc.c/.h`
- **应用入口**：`action_adc_test()` — 电压显示 + 频率测量 + 通道切换
- **已实现通道**：VIN1(ADC0-CH2, PA25), VIN2(ADC1-CH11, PB23), VIN3(ADC0-CH3, PA24), VIN4(ADC0-CH5, PB24), VIN5(ADC1-CH12, PA23)
- **功能**：
  - 实时电压显示（12-bit ADC, 0~3300mV）
  - 频率测量（双速率 burst + 过零检测，~10Hz~165kHz）
  - S0/S1 切换通道（五通道循环）
- **SysConfig**：ADC0 + ADC1, ULPCLK 40MHz, 2.5μs 采样时间, 轮询模式
- **硬件**：J2 排座，各路带 49.9Ω + 220pF 抗混叠滤波
- **参考**：`development_reference/G3519_main_board.md` §5.1

### FFT 频谱分析

- **代码文件**：`NUEDC2025/tsp_fft.c/.h`
- **依赖**：CMSIS-DSP `arm_cortexM0l_math.a`（SDK 自带），`tsp_adc` burst 采样
- **功能**：
  - 256 点 Q15 CFFT（`arm_cfft_q15`）
  - 频率：抛物线插值 sub-bin 精度，0.1Hz 分辨率
  - 幅值：时域 Vpp/2（mV）
  - THD：2~8 次谐波相对基频
  - 可变采样率：FAST(~204kSPS) / MED(~5kSPS) / SLOW(~1.2kSPS)
- **已验证**（DDS→ADC 回环，SSCOM 115200-8N1）：
  - 1kHz SINE: freq=1014.7Hz, amp=305mV, THD=0.4%
  - 1kHz SQR: freq=1014.6Hz, amp=1650mV, THD=1.9%
  - 500Hz SINE: freq=501.3Hz, 2kHz SINE: freq=2019.5Hz
- **限制**：THD 精度受采样率约束（MED 下只能捕获有限谐波），FAST 模式 ms 级定时器分辨率导致 fs 计算误差

### UART 命令协议

- **代码文件**：`NUEDC2025/tsp_uart.c/.h`（UART0, 超时 TX）、`tsp_uart6.c/.h`（UART6）、`tsp_cmd.c/.h`（协议层）
- **UART0→PC**：MFCLK 4MHz, PA10(TX)/PA11(RX), DAPLink 虚拟串口, 已验证
- **UART6→K230**：BUSCLK 80MHz, PC11(TX)/PC10(RX), J11, SysConfig 已配置, 驱动已写, 待硬件验证
- **TX 超时保护**：10ms 超时，脱机（不接 DAPLink）时静默失败不卡死
- **命令集**：VER?, ADC, FREQ, DDS, FFT（详见 CLAUDE.md API 速查）
- **已验证**：SSCOM 115200-8N1 全部命令 OK，脱机启动正常

### 编码器 (旋钮)

- **代码文件**：`NUEDC2025/tsp_encoder.c/.h`
- **状态**：完整可用，已在 CCD Test 和 DDS Test 中作为 UI 交互组件使用

### Scope 波形显示

- **代码文件**：`NUEDC2025/tsp_scope.c/.h`
- **应用入口**：`action_scope_test()` — LCD 实时波形显示
- **功能**：
  - 160×96 像素图形区域（y=16~111），虚线网格
  - 三档时基：FAST(~4.9μs/sample)、MED(~20μs/sample)、SLOW(~100μs/sample)
  - 上升沿触发（中点触发电平，幅值 <50 LSB 时不触发）
  - 自动量程（±5% 余量，噪底 <200 LSB 时固定 0~3.3V）
  - 差分更新渲染（仅擦旧画新，50ms/帧）
- **已验证**：DDS→ADC 回环，正弦/方波/三角波显示正确
- **交互**：S0/S1 切换通道，S2 切换时基，PUSH 退出

### Sweep 扫频分析仪

- **代码文件**：应用层 static 函数，位于 `iar/empty_mspm0g3519.c`
- **应用入口**：`action_sweep_test()` — DDS+ADC 联动扫频测量 DUT 幅频特性
- **功能**：
  - 80 点对数扫频（100Hz → ~50kHz，8.2%/步）
  - 自适应采样率（低频长窗口 + 高频短窗口）
  - 绝对 Y 轴量程：0 到自动上限（100/200/500/1000/1500/2000/2500/3300 mV）
  - 垂直频率标记：1-2-5 序列（200/500/1k/2k/5k/10k/20kHz），"1k"/"10k" 带标签
  - 水平参考网格：25%/50%/75% 虚线
  - 显示峰值 Vpp 及对应频率
- **已验证**：RC 低通滤波器（18kΩ+10nF，fc≈884Hz），曲线在 1kHz 标记附近正确滚降
- **交互**：S0/S1 切换通道，S2 启动扫频，PUSH 退出/中止
