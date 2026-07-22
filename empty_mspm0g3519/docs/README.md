# 文档目录

## 开发参考

位于 `development_reference/`：

| 文档 | 说明 |
|---|---|
| [G3519_main_board.md](development_reference/G3519_main_board.md) | MSPM0G3519 主板：引脚定义、外设连接、电源树、调试接口 |
| [G3510_expansion_board.md](development_reference/G3510_expansion_board.md) | G3510 扩展板：附加外设、接口说明、与主板连接方式 |
| [M0G3519_UART_Use.md](development_reference/M0G3519_UART_Use.md) | UART 使用说明：J1 针脚、引脚复用、时钟约束、已验证配置、排查清单 |
| [K230_Vision_Module_Use.md](development_reference/K230_Vision_Module_Use.md) | K230 视觉模块对接（UART6/J11，双向已验证，驱动/解析/菜单已实装） |
| [AD9833_DDS_Use.md](development_reference/AD9833_DDS_Use.md) | AD9833 DDS 波形发生器 [已封存：PC2/PC3 与 CCD 共用，已验证通过，保留代码供信号题使用] |
| [AD5933_Use.md](development_reference/AD5933_Use.md) | AD5933 阻抗测量使用说明：I2C 硬件连接、菜单操作、温度/扫频 API、SysConfig、故障排查 |
| [AD5933_Examine.md](development_reference/AD5933_Examine.md) | AD5933 功能验证指南：AD2 五步验证流程（I2C→库函数→温度→VOUT→校准） |
| [AD8302_Use_Mothball.md](development_reference/AD8302_Use_Mothball.md) | AD8302 幅相检测 [封存：需飞线改硬件，有完整修正方案] |
| [DRV8874_Motor_Use.md](development_reference/DRV8874_Motor_Use.md) | DRV8874 电机驱动使用说明：TIMA0 PWM 配置、M1/M2 独立控制、AD2 测量、nFAULT 注意事项 |
| [CCD_AD2_Test.md](development_reference/CCD_AD2_Test.md) | CCD 代码功能验证：AD2 无传感器测试方案（SI/CLK 时序、ADC 精度、波形响应、双通道） |

## 项目进度

位于 `project_schedule/`：

| 文档 | 说明 |
|---|---|
| [signal_modules.md](project_schedule/signal_modules.md) | 信号题相关模块实现进度（AD5933/AD9833/CCD/AD8302/通用ADC） |
| [control_modules.md](project_schedule/control_modules.md) | 控制题（小车题）相关模块实现进度（DRV8874/K230/CCD/MPU6050/编码器/通用ADC） |

## K230 固件参考

位于 `k230_firmware_ref/`：亚博 K230 模块 SD 卡固件源码存档（仅供分析参考）。

## 软件文档

软件架构、开发环境搭建、工程结构等请参见仓库根目录的 [README.md](../../README.md)。
