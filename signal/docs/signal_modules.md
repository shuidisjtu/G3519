# 信号题模块实现进度

> 最后更新：2026-07-27

## 进度总览

| 模块 | 状态 | 说明 |
|------|------|------|
| AD5933 阻抗测量 | **待验证** | 激励 2Vpp + RFB 20KΩ + 300KΩ标定 + 4次平均（对标参考设计），需换 RFB 后验证。详见 [AD5933_Use.md] |
| AD9833 DDS 信号源 | **可用** | 方波/正弦/三角波，100Hz~50kHz。详见 [AD9833_DDS_Use.md] |
| AD8302 幅相检测 | 封存 | 输入网络未焊 + 需 RF 信号。详见 [AD8302_Use_Mothball.md] |
| 通用 ADC（J2 五路） | **可用** | ADC0+ADC1 五通道，电压/频率/burst 采样 |
| FFT 频谱分析 | **可用** | CMSIS-DSP Q15 256 点，已验证 1kHz SINE/SQR。含单频相位提取（Sweep 用） |
| Scope 波形显示 | **可用** | 160×96px，三档时基，自动量程，差分更新 |
| Sweep 扫频分析仪 | **已验证** | Cal/Meas/View 三阶段，增益归一化+相位测量，80 点对数扫频。三组验证通过：直连(G≈100%/φ≈0°)、RC 低通 18kΩ+10nF（fc≈884Hz 滚降+相位→-90°）、220Ω+220Ω 分压（G≈30.7% 平坦/φ≈0°，含 DDS~200Ω 源阻抗） |
| TJC 串口屏（4.3 寸） | **已验证** | UART3 (J4, PC6/PC7) 115200-8N1 双向通信验证通过（按钮事件/页面跳转/数值更新/文本更新/addt 波形透传）。J4 5V 带载不足，屏需独立供电。详见 [TJC4827X543_011C_Use.md] |
| UART 命令协议 | **可用** | UART0→PC 已验证，UART6→K230 待验证。详见 [MSPM0G3519_UART_Use.md] |
| 编码器（旋钮） | **可用** | DDS Test 中使用 |

## 待完成

- AD5933：硬件换 RFB=20KΩ (R37) → 接 300KΩ 标定电阻验证
- AD5933：多频扫频（INCREMENT_FREQ 循环）
- MCP41010 SPI 程控放大：已修复 SPI 时序（加 __NOP 延时），待硬件验证
- UART6→K230：硬件验证
- TJC 屏：比赛正式 UI 工程（根据实际题目搭建）

[AD5933_Use.md]: ../../common/docs/AD5933_Use.md
[AD9833_DDS_Use.md]: ../../common/docs/AD9833_DDS_Use.md
[AD8302_Use_Mothball.md]: ../../common/docs/AD8302_Use_Mothball.md
[MSPM0G3519_UART_Use.md]: ../../common/docs/MSPM0G3519_UART_Use.md
[TJC4827X543_011C_Use.md]: TJC4827X543_011C_Use.md
