# G3519 重构设计:统一平台库 + 双题目应用

日期:2026-08-10
状态:已获用户批准(2026-08-10)

## 背景

NUEDC-2026 比赛结束,仓库刚完成三分支整合(main 基础平台 / control 控制题 / signal 信号题)。用户提出两个新需求:

1. `docs/superpowers/` 为本地开发产物,已从 git 出库(不参与本重构)
2. **大规模重构**:三条线存在大量重复内容(TSP3519 板级库、基础驱动、共享文档各三份拷贝,且已分叉),需要统一

## 目标与动机

**动机**:未来复用——共享代码整理为干净可用的平台库,后续题目直接引用。

**目标**:
1. 共享代码物理存在一份(common 平台库)
2. 控制题/信号题各自保留应用层,共用 common
3. 分叉模块内容级合并(取最新最完整,融合改进)
4. 每阶段编译验证(IAR iarbuild,0 错误)

**非目标**:
- 不改动各模块的对外 API 行为(合并时保持功能,不重新设计)
- 不重写 git 历史(合并保历史)
- 不处理 `2026TI` / `translate`
- 信号题封存模块(AD8302 已封存)保留原样,不删除

## 决策记录(用户确认)

| 决策点 | 结论 |
|---|---|
| 重构动机 | 未来复用 |
| 目标形态 | 单仓库单 main 分支,目录分离(推翻三分支结构) |
| 分叉模块统一 | 内容级合并(逐模块对比取舍,取最新最完整) |
| docs/superpowers | 出库,不参与重构 |
| 架构参考 | 参考 `F:\Test\Delay architectural decay.txt`(架构防腐实践),目标:功能性强、兼容性好、易维护 |

## 架构原则(来自防腐实践,本项目落地版)

以下原则为"宪法级"约束,所有重构动作必须遵守:

1. **消灭真相分裂**:同一条业务规则物理存在一份(common)。任何题目需求不得在应用层复制 common 代码再改
2. **低耦合高内聚**:依赖方向单向——`control/app`、`signal/app` → `common/drivers` → `common/TSP3519`。common 禁止引用题目特有代码;题目应用禁止绕过 drivers 直接操作寄存器
3. **物理隔离(稳定层)**:common 为稳定平台库。判断标准:合并完成后 3 个月内无提交的模块即视为稳定,后续题目以"引用"方式使用,不随题目修改
4. **稳定 API 契约**:common 公共接口(如 `tsp_motor_set`、`tsp_uart_send_string`、`tsp_menu_*`)为不可随意改动核心。改动需记录理由链(为什么改、影响面)
5. **清理疤痕组织**:合并时清除僵尸分支/字段(如 main 初版 motor 全部废弃;封存模块在 header 显式标注状态与原因)。不保留"可能有用"的死代码
6. **文档记录"为什么"**:合并后的每个模块保留并完善 header 注释(用途/硬件/API);CLAUDE.md 记录架构决策理由(为什么这样分层、依赖方向约束、哪些不可改动)
7. **修复承重 bug**:合并中发现某处"错误行为被下游依赖"的情况,记录并修正,不把 bug 当契约带入 common
8. **持续代谢**:本次重构即一次大规模代谢;合并动作逐模块小步提交,保持可审查

## 目标结构

```
G3519(main 分支,单仓库单主线)
├── BRANCHES.md                  ← 改写为目录结构说明(或删除,并入 README)
├── README.md                    ← 重写:平台总览 + 双题目说明
├── CLAUDE.md                    ← 重写:目录结构 + 编译方法(两套工程)
├── common/                      ← 共享平台库(未来复用)
│   ├── TSP3519/                 ← 板级支持库(物理一份)
│   │   ├── tsp_gpio.h/.c        ← GPIO 宏
│   │   ├── TSP_TFT18.h/.c       ← LCD(ST7735)
│   │   ├── tsp_ccd.h/.c         ← 线阵 CCD
│   │   ├── tsp_menu.h/.c        ← LCD 菜单系统
│   │   └── tsp_common_headfile.h ← 头文件枢纽(路径适配)
│   ├── NUEDC2025/               ← 基础驱动(物理一份)
│   │   ├── tsp_isr.h/.c         ← SysTick + 中断分发
│   │   ├── tsp_key.h/.c         ← 4 键扫描
│   │   ├── tsp_encoder.h/.c     ← 旋钮编码器
│   │   ├── tsp_uart.h/.c        ← UART0(超时保护版)
│   │   └── tsp_uart_k230.h/.c   ← UART6 驱动(与 signal tsp_uart6 合并)
│   ├── drivers/                 ← 通用外设驱动(合并三分支差异)
│   │   ├── tsp_motor.h/.c       ← DRV8874 双 PWM(control 版最全)
│   │   ├── tsp_mpu6050.h/.c     ← IMU
│   │   ├── tsp_ad5933.h/.c      ← 阻抗分析(signal 增强版)
│   │   ├── tsp_dds.h/.c         ← DDS 波形(signal 版)
│   │   ├── tsp_adc.h/.c         ← 通用 ADC(signal 5 通道版)
│   │   ├── tsp_pid.h/.c         ← PID(control 版)
│   │   └── tsp_uart3.h/.c       ← UART3(signal 版,主板接口)
│   └── docs/                    ← 共享文档(物理一份)
│       ├── 3519_hardware/       ← 硬件手册(刚入库的 8 文件)
│       ├── G3519_main_board.md / G3510_expansion_board.md / M0G3519_UART_Use.md
│       ├── CCD_AD2_Test.md / DRV8874_Motor_Use.md / K230_Vision_Module_Use.md
│       ├── AD5933_Use.md / AD9833_DDS_Use.md(两题共享的通用文档)
│       └── YbProtocol.py / YbUart.py / uart.py
├── control/                     ← 控制题应用
│   ├── iar/                     ← 控制题 IAR 工程(.ewp/.syscfg/ti_msp_dl_config/startup/icf)
│   ├── app/                     ← 控制题特有模块
│   │   ├── tsp_k230.h/.c        ← K230 协议解析(control 独有)
│   │   ├── tsp_wheel_enc.h/.c   ← 双轮 QEI 编码器(control 独有,底盘硬件)
│   │   ├── tsp_linefollow.h/.c  ← CCD 循迹
│   │   └── tsp_odometer.h/.c    ← 里程计
│   ├── k230_scripts/            ← K230 脚本
│   └── docs/                    ← 控制题特有文档(control_modules/题猜/物料清单/gen_test)
├── signal/                      ← 信号题应用
│   ├── iar/                     ← 信号题 IAR 工程
│   ├── app/                     ← 信号题特有模块
│   │   ├── tsp_cmd.h/.c         ← UART 命令协议
│   │   ├── tsp_fft.h/.c         ← FFT 频谱(CMSIS-DSP)
│   │   ├── tsp_scope.h/.c       ← 示波器显示
│   │   ├── tsp_tjc.h/.c         ← TJC 串口屏应用
│   │   ├── tsp_mcp41010.h/.c    ← PGA 数字电位器
│   │   └── tsp_ad8302.h/.c      ← [已封存] 保留原样
│   └── docs/                    ← 信号题特有文档(signal_modules/参考/采购/赛题整理)
└── .gitignore                   ← 保留,补 docs/superpowers 已有
```

## 模块归属与合并决策点

### 直接取用(无分叉或仅单边有):约 12 个

| 模块 | 取用来源 | 说明 |
|---|---|---|
| tsp_gpio | control | 三线差异小,control 最新 |
| tsp_menu | control | control 有增量重绘改进(193 行 vs 170) |
| TSP_TFT18 | control | 604 行 vs 586,control 有扩展 |
| tsp_ccd | control | main/control 有,signal 无 |
| tsp_key / tsp_isr / tsp_encoder | control | 基础模块 |
| tsp_motor | control | 双 PWM 架构,main 初版废弃 |
| tsp_mpu6050 | control | main 无驱动,signal 无 |
| tsp_pid | control | 唯一来源 |
| tsp_wheel_enc / tsp_k230 / tsp_linefollow / tsp_odometer | control | 控制题独有 → control/app |

### 内容级合并(两分支分叉):约 7 个

| 模块 | control 版 | signal 版 | 合并要点 |
|---|---|---|---|
| tsp_uart | 超时保护+环形缓冲 | 超时保护+命令协议用 | 取 control 主体,对比差异 |
| tsp_uart_k230 / tsp_uart6 | UART6 驱动 | UART6 驱动(同接口) | 合并为 common 一份 |
| tsp_ad5933 | main 初版 | 增强版(重对齐参考设计) | 取 signal 版 |
| tsp_dds | main 封存版 | signal 版 | 取 signal 版 |
| tsp_adc | main 无 | 5 通道版 | 取 signal 版 |
| tsp_uart3 | 无 | TJC 屏用 | 取 signal 版 |
| tsp_common_headfile | 三线各有 | 三线各有 | 合并路径引用 |

**合并方法**:取较新/较全版本为基,逐行 diff 另一版本,确认无遗漏改进后保留。每个模块合并后立即编译验证。

### 文档合并

- 共享 → common/docs(物理一份)
- 题目特有 → 各自 docs
- 散落临时文件(control 分支的 250.pdf/276.pdf/277.jpg 等比赛截图)→ 保留在 control/docs 或删除(执行时与用户确认)

## git 历史策略

1. 当前 main = 基础平台(60029d7 + BRANCHES.md)
2. `git merge control --allow-unrelated-histories` → 冲突即重组点(同名文件×3 份拷贝)
3. 解决冲突 = 目录重组 + 内容选择(common/control 结构落地)
4. `git merge signal --allow-unrelated-histories` → 同样处理(common/signal 落地)
5. 共享模块逐个内容级合并(冲突文件中人工融合)
6. 完成后删除 control/signal 分支(本地+远端),全部历史保留在 main
7. push(临时豁免 main 保护,与上次相同)

**风险**:合并冲突量大(每个共享文件一个决策点);分两步合并分阶段验证,避免一次性大爆炸。

## 验证

- 每阶段 `iarbuild.exe <工程>.ewp -build Debug` 编译对应题目工程,0 错误
- 控制题工程(重构后路径 control/iar/)与信号题工程(signal/iar/)都要编译通过
- IAR 全局变量(MSPM0_SDK_INSTALL_DIR / SYSCONFIG_ROOT)已配置,路径引用按新目录更新
- 编译产物为目标代码,不做硬件验证(比赛结束)

## 阶段划分(每阶段独立验证)

| 阶段 | 内容 | 验证 |
|---|---|---|
| 1 | main merge control → common+control 目录重组,共享模块取 control 版 | control 工程编译 |
| 2 | main merge signal → common+signal 落地,内容级合并分叉模块 | signal 工程编译 + control 回归编译 |
| 3 | 文档整理(共享/特有分家、散落文件确认)、BRANCHES/README/CLAUDE 重写 | 文档链接检查 |
| 4 | 删除旧分支、push、最终验证 | 远端结构确认 |

## 风险与保护

- 合并冲突 = 设计内的决策点,逐文件处理,不自动化覆盖
- 编译验证兜底:每阶段 0 错误才进入下一阶段
- 旧分支删除前确认 main 已包含其全部提交
- push main 需临时豁免 branch-guard hook(用户已授权此模式)
