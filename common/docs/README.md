# common/docs 文档索引

G3519 共享平台文档（硬件参考、外设驱动使用、测试方案）。控制题/信号题专有文档见 `control/docs/` 与 `signal/docs/`。

## 硬件参考

| 文档 | 说明 |
|------|------|
| [G3519_main_board.md](G3519_main_board.md) | 主控板硬件参考（MSPM0G3519） |
| [G3510_expansion_board.md](G3510_expansion_board.md) | 双电机扩展板硬件参考 |
| [MSPM0G3519_UART_Use.md](MSPM0G3519_UART_Use.md) | UART0 调试串口使用 |
| [3519_hardware/](3519_hardware/) | 板卡原理图/芯片手册/接线图（PDF/图片） |
| [datasheets/](datasheets/) | 芯片官方手册库（mcu/peripherals/instruments，见下文） |

## 外设与模块使用

| 文档 | 说明 |
|------|------|
| [AD5933_Use.md](AD5933_Use.md) | AD5933 阻抗测量使用（含验证记录与调试报告） |
| [AD9833_DDS_Use.md](AD9833_DDS_Use.md) | AD9833 DDS 信号源使用（**封存**：与 CCD 共用 PC2/PC3，启用需互斥） |
| [AD8302_Use_Mothball.md](AD8302_Use_Mothball.md) | AD8302 幅相检测（封存） |
| [DRV8874_Motor_Use.md](DRV8874_Motor_Use.md) | DRV8874 双电机驱动使用 |
| [K230_Vision_Module_Use.md](K230_Vision_Module_Use.md) | K230 视觉模块集成 |

## 测试与验证方案

| 文档 | 说明 |
|------|------|
| [CCD_AD2_Test.md](CCD_AD2_Test.md) | CCD 代码 AD2 验证方案 |

## 赛题材料

| 文档 | 说明 |
|------|------|
| [2026_material_list.md](2026_material_list.md) | 2026 电赛材料清单（控制题/信号题共享） |

## 芯片手册库（`datasheets/`）

精选自 F 盘备赛资料库（`F:\电赛\备赛知识相关`），与项目在用的主控/外设/仪器配套。参考代码工程不入库（按需查 F 盘）。

- `mcu/`：MSPM0 G-Series TRM（80MHz 系列完整手册，SLAU846D，覆盖 G351x）、Errata（ZHCZ042F）
  - ⚠️ **库内 Errata 仅覆盖 MSPM0G3x0x/G1x0x/G3x0x-Q1 家族，不含主控 G351x**；G351x 正确勘误为 TI SLAZ758D（EN）/ ZHCZ040B（CN，见 TI 官网），需要时自行下载
- `peripherals/`：AD5933（含 UG-364 评估板）、AD9833、AD8302、AD8605/8606/8608（单文件覆盖三型号）、DRV8874、MPU6050、AD603、AD620、SA5532、WRA_S-1WR2（电源）、MPY634
- `instruments/`：AD2 用户手册

> ⚠️ 部分文件名含中文/空格（如 `MSPM0 G-Series 80MHz...pdf`、`AD2用户手册.pdf`），shell 脚本/CI 处理时注意加引号。

## K230 固件参考（`k230_firmware_ref/`）

K230 SD 卡固件源码参考（YbProtocol 通信协议）。

| 文件 | 说明 |
|------|------|
| [YbProtocol.py](k230_firmware_ref/YbProtocol.py) | 亚博 UART 协议封装 |
| [YbUart.py](k230_firmware_ref/YbUart.py) | K230 UART 驱动层 |
| [uart.py](k230_firmware_ref/uart.py) | UART 底层接口 |

## 相关目录

- [`control/docs/`](../../control/docs/) — 控制题模块进度（control_modules.md）
- [`signal/docs/`](../../signal/docs/) — 信号题模块进度（signal_modules.md）、模块参考手册、TJC 屏使用、2026 G 题官方赛题与模拟前端设计存档
