# 信号题模块实现进度

> 最后更新：2026-07-27

## 进度总览

| 模块 | 状态 | 说明 |
|------|------|------|
| AD5933 阻抗测量 | 硬件阻断 | GainFactor 标定已实现，R36 增益不足致 SNR 过低。详见 [AD5933_Use.md] / [Debug_Report.md] |
| AD9833 DDS 信号源 | **可用** | 方波/正弦/三角波，100Hz~50kHz。详见 [AD9833_DDS_Use.md] |
| AD8302 幅相检测 | 封存 | 输入网络未焊 + 需 RF 信号。详见 [AD8302_Use_Mothball.md] |
| 通用 ADC (J2 五路) | **可用** | ADC0+ADC1 五通道，电压/频率/burst 采样 |
| FFT 频谱分析 | **可用** | CMSIS-DSP Q15 256 点，已验证 1kHz SINE/SQR。含单频相位提取（Sweep 用） |
| Scope 波形显示 | **可用** | 160×96px，三档时基，自动量程，差分更新 |
| Sweep 扫频分析仪 | **已验证** | Cal/Meas/View 三阶段，增益归一化+相位测量，80 点对数扫频。三组验证通过：直连(G≈100%/φ≈0°)、RC低通18kΩ+10nF(fc≈884Hz滚降+相位→-90°)、220Ω+220Ω分压(G≈30.7%平坦/φ≈0°，含DDS~200Ω源阻抗) |
| UART 命令协议 | **可用** | UART0→PC 已验证，UART6→K230 待验证。详见 [M0G3519_UART_Use.md] |
| 编码器 (旋钮) | **可用** | DDS Test 中使用 |

## 待完成

- AD5933：硬件修复（R31 换 20kΩ 或 R36 换 100Ω）→ 重新标定验证
- AD5933：多频扫频（INCREMENT_FREQ 循环）
- UART6→K230：硬件验证

[AD5933_Use.md]: ../development_reference/AD5933_Use.md
[Debug_Report.md]: ../development_reference/AD5933_Debug_Report.md
[AD9833_DDS_Use.md]: ../development_reference/AD9833_DDS_Use.md
[AD8302_Use_Mothball.md]: ../development_reference/AD8302_Use_Mothball.md
[M0G3519_UART_Use.md]: ../development_reference/M0G3519_UART_Use.md
