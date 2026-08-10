# 架构设计

本文记录 G3519 平台的架构决策与约束(2026-08-10 重构后)。所有开发必须遵守。

## 目录结构

```
G3519(main 分支,单仓库单主线)
├── common/            ← 共享平台库(物理一份,稳定层)
│   ├── TSP3519/       ← 板级支持库: gpio/lcd/ccd/menu/头文件枢纽
│   ├── NUEDC2025/     ← 基础驱动: isr/key/encoder/uart/uart_k230
│   ├── drivers/       ← 通用外设驱动: motor/mpu6050/pid/ad5933/dds/adc/uart3
│   └── docs/          ← 共享文档(硬件手册/主板资料/通用外设用法)
├── control/           ← 控制题应用(iar 工程 + app + 特有文档)
├── signal/            ← 信号题应用(iar 工程 + app + 特有文档)
└── ARCHITECTURE.md / README.md / CLAUDE.md
```

## 为什么这样分层(决策记录)

1. **为什么单仓库单主线?** 三分支方案(main 基础平台 / control / signal)在赛后将共享代码演化为三份拷贝(真相分裂),且分支间无法共享文件。单仓库 + 目录分离使共享代码物理存在一份,后续题目直接复用。历史仍完整保留在 main 的 merge commit 中(三次不相交历史的第二父提交)。

2. **为什么依赖方向是 app → drivers → TSP3519?** 低耦合高内聚。题目应用只依赖稳定的驱动层 API;驱动层只依赖板级寄存器封装。反向依赖(common 引用题目代码)已出现过一次(tsp_isr.c 引用 tsp_wheel_enc.h),用 `TSP_USE_WHEEL_ENC` 编译宏隔离——这是允许的扩展方式,禁止直接 include。

3. **什么不能做?**
   - 禁止在 control/signal 应用层复制 common 代码再修改(真相分裂)
   - 禁止 common 新增对题目特有目录的依赖;新增跨题能力用编译宏(`TSP_USE_*`)隔离
   - 禁止删除 common 公共 API 或改变其语义(稳定契约,见下)

## 稳定 API 契约(改动需评审并记录理由)

以下接口为稳定契约,后续题目依赖,改动必须写理由链(为什么改、影响面):

- `common/NUEDC2025`: `tsp_uart_*`(含 printf 重定向)、`tsp_uart_k230_*`、`tsp_key_*`、`tsp_encoder_*`、`tsp_isr`(SysTick/delay_1ms)
- `common/TSP3519`: `tsp_gpio` 宏(`LED_*`/`BUZZ_*`/按键/`PGA_*`)、`TSP_TFT18_*`、`tsp_menu_*`、`tsp_ccd_*`
- `common/drivers`: `tsp_motor_*`、`tsp_mpu6050_*`、`tsp_pid_*`、`tsp_ad5933_*`、`tsp_dds_*`、`tsp_adc_*`、`tsp_uart3_*`

## 防腐原则(来自架构防腐实践,2026-08-10 重构落地)

1. 消灭真相分裂:同一条业务规则物理存在一份
2. 稳定层物理隔离:common 中 3 个月无提交的模块视为稳定,新题目以引用方式使用
3. 清理疤痕组织:封存模块在 header 显式标注状态与原因;不保留"可能有用"的死代码
4. 文档记录"为什么":模块 header 注明用途/硬件/API;本文件记录架构决策
5. 承重 bug 不变成契约:发现"错误行为被下游依赖"时,记录并修正

## 编译开关

| 宏 | 用途 | 生效工程 |
|---|---|---|
| `TSP_USE_WHEEL_ENC` | 启用双轮 QEI 编码器(SysTick 20ms 更新) | control |
