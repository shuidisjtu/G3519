# 文档目录

## 开发参考

位于 `development_reference/`：

| 文档 | 说明 |
|---|---|
| [G3519_main_board.md](development_reference/G3519_main_board.md) | MSPM0G3519 主板：引脚定义、外设连接、电源树、调试接口 |
| [G3510_expansion_board.md](development_reference/G3510_expansion_board.md) | G3510 扩展板：附加外设、接口说明、与主板连接方式 |
| [M0G3519_UART_Use.md](development_reference/M0G3519_UART_Use.md) | UART 使用说明：J1 针脚、引脚复用、时钟约束、已验证配置、排查清单 |
| [AD5933_Use.md](development_reference/AD5933_Use.md) | AD5933 阻抗测量使用说明：I2C 硬件连接、菜单操作、温度/扫频 API、SysConfig、故障排查 |
| [AD5933_Examine.md](development_reference/AD5933_Examine.md) | AD5933 功能验证指南：AD2 五步验证流程（I2C→库函数→温度→VOUT→校准） |
| [AD9833_DDS_Use.md](development_reference/AD9833_DDS_Use.md) | AD9833 DDS 波形发生器使用说明：GPIO 接线、波形切换、编码器调频、AD2 验证 |
| [AD8302_Use_Mothball.md](development_reference/AD8302_Use_Mothball.md) | AD8302 幅相检测 [封存：需飞线改硬件，有完整修正方案] |
| [AD5933_Debug_Report.md](development_reference/AD5933_Debug_Report.md) | AD5933 调试报告：信号链增益问题分析，R31/R36 修复方案 |

## 项目进度

位于 `project_schedule/`：

| 文档 | 说明 |
|---|---|
| [signal_modules.md](project_schedule/signal_modules.md) | 信号题模块实现进度（AD5933/AD9833/ADC/FFT/UART 命令协议） |

## 软件文档

软件架构、开发环境搭建、工程结构等请参见仓库根目录的 [README.md](../../README.md)。
