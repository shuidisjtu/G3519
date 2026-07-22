# 信号题模块实现进度

> 最后更新：2026-07-23
> 覆盖场景：阻抗/RLC 测量、信号发生、频率/幅度/相位测量、光电信号采集、通用电压采集等

## 进度总览

| 模块 | 代码 | 硬件验证 | 状态 |
|------|------|---------|------|
| AD5933 阻抗测量 | 完整 | 部分 | 阻抗测量未通过 |
| AD9833 DDS 信号源 | 完整 | 已通过 | 封存 (与CCD引脚互斥) |
| CCD 线阵光电 | 完整 | 未测 | 传感器未采购 |
| AD8302 幅相检测 | 无代码 | 未验证 | 封存，有实施方案 |
| 通用 ADC (J2 五路) | 无代码 | 未验证 | 空白 |
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
- **未通过**：GainFactor 标定 + DUT 阻抗测量（2026-07-22 下午持续异常，排查中）
- **待完成**：多频扫频（INCREMENT_FREQ 循环）仅有文档示例，未写入 action 代码
- **硬件限制**：RFB 固定 20kΩ (板载 R37)，J15 仅两线接口
- **参考文档**：`development_reference/AD5933_Use.md`、`AD5933_Examine.md`

### AD9833 DDS 信号源

- **代码文件**：`NUEDC2025/tsp_dds.c/.h`
- **应用入口**：`action_dds_test()` — 封存在 `#if 0` 中
- **已验证**：方波/正弦/三角波均通过 AD2 在 J22 确认输出正确
- **封存原因**：PC2/PC3 与 CCD 共用，为验证 CCD 功能而主动封存
- **启用方法**：取消注释 `#include "tsp_dds.h"` + 恢复菜单项 + 确保 CCD 未初始化
- **参考文档**：`development_reference/AD9833_DDS_Use.md`

### CCD 线阵光电

- **代码文件**：`TSP3519/tsp_ccd.c/.h`
- **应用入口**：`action_ccd_test()` — 连续/单拍模式、双通道切换、曝光调节、LCD 波形
- **代码审查**：2026-07-22 已完成静态审查，修复 5 个问题：
  - CSTACK 512B→2KB（栈溢出）
  - VREF 改 VDDA_VSSA（内部参考未初始化）
  - ADC1 加 reset + delay_cycles + setClockConfig（SDK 标准序列）
  - ADC BUSY 轮询加超时
- **未测原因**：CCD 传感器模组尚未采购
- **AD2 测试方案**：`development_reference/CCD_AD2_Test.md`（无传感器可用 AD2 波形发生器模拟验证）
- **引脚互斥**：与 AD9833 DDS 共用 PC2/PC3，不能同时使用

### AD8302 幅相检测

- **代码文件**：无
- **封存文档**：`development_reference/AD8302_Use_Mothball.md`
- **封存原因**：4 项阻断问题——PB14/PB13 非 ADC 引脚需飞线至 PA12/PA13、ADC 模式需改 Sequence、相位公式需修正、悬空验证无效
- **实施方案**：文档中已记录修正方案（飞线接法、ADC0 配置、公式），可快速实现

### 通用 ADC (J2 五路模拟输入)

- **代码文件**：无
- **硬件**：J2 排座 VIN1~VIN5（PA25/PB23/PA24/PB24/PA23），各带 49.9Ω + 220pF 抗混叠
- **跨 ADC 实例**：VIN1/VIN3 在 ADC0，VIN2/VIN4/VIN5 分布在 ADC0 和 ADC1
- **参考**：`development_reference/G3519_main_board.md` §5.1

### 编码器 (旋钮)

- **代码文件**：`NUEDC2025/tsp_encoder.c/.h`
- **状态**：完整可用，已在 CCD Test 和 DDS Test 中作为 UI 交互组件使用
