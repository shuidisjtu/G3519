# DRV8874 直流电机驱动使用说明

> 状态：**待复测**（2026-07-26）— PWM 波形曾于 2026-07-22 经 AD2 确认、M1/M2 独立控制正常，但此后 TIMA0 已迁移至 SysConfig，需按 §7 复测。

## 1. 硬件概要

| 项目 | 说明 |
|---|---|
| 驱动芯片 | DRV8874PWPR × 2（U6=M1, U7=M2） |
| 控制模式 | PMODE=HIGH（R43 上拉） |
| VM 电源 | VBAT（经 XT30/SW1），**必须打开 SW1** |
| 逻辑电源 | VREF=MCU_3V3 |
| PWM 源 | TIMA0 (80MHz BUSCLK)，20kHz，period=3999 |
| nSLEEP | PB1，两路共用，HIGH=使能 |
| nFAULT | PA7，两路共用，开漏+10kΩ 上拉到 MCU_3V3 |

### 1.1 引脚映射

| 功能 | MCU 引脚 | IOMUX | 定时器通道 |
|---|---|---|---|
| M1 PWM (IN2) | PB3 | PINCM16 | TIMA0_CCP0 (CC0) |
| M1 DIR (IN1) | PB4 | PINCM17 | GPIO |
| M2 PWM (IN2) | PB0 | PINCM12 | TIMA0_CCP2 (CC2) |
| M2 DIR (IN1) | PB2 | PINCM15 | GPIO |

### 1.2 DRV8874 控制逻辑 (PMODE=HIGH)

| 模式 | IN1 (DIR) | IN2 (PWM) | 输出 |
|---|---|---|---|
| Forward | LOW | PWM | OUT1→OUT2 |
| Backward | HIGH | PWM | OUT2→OUT1 |
| Coast | LOW | LOW | Hi-Z |
| Brake | HIGH | HIGH | 低侧制动 |

## 2. 测试接口

| 接口 | 信号 | 用途 |
|---|---|---|
| J10 / JM1 | M1+, M1- | 电机 1 输出（DRV8874 OUT1/OUT2） |
| J11 / JM2 | M2+, M2- | 电机 2 输出 |
| J14 Pin 13-16 | M1IN2/M1IN1/M2IN2/M2IN1 | 逻辑侧 PWM+DIR（3.3V，可接逻辑分析仪） |

**AD2 示波器测量**：探头接 J10 或 J11，量程 ±25V。

- **有 VBAT**：可观察到 20kHz PWM 调制的电机驱动波形
- **无 VBAT**：DRV8874 无输出，J10/J11 仅浮动电平。此时用逻辑分析仪接 J14 侧观测 IN1/IN2（3.3V 逻辑电平）

## 3. 菜单操作 (Motor Test)

进入路径：主菜单 → Motor Test

| 按键 | 功能 |
|---|---|
| S0 | Duty -5% |
| S1 | Duty +5% |
| S2 | 切换方向 (FWD↔REV) |
| 编码器左转 | 切换到 M2 |
| 编码器右转 | 切换到 M1 |
| PUSH | 退出 |

### 3.1 LCD 显示布局

| 行 | 内容 |
|---|---|
| 0 | 标题 "Motor Test TIMA0" |
| 2 | 当前电机/方向/占空比 (如 "M1: FWD 50%") |
| 3 | Duty 进度条 "Duty:[#####     ]" |
| 4 | PWM 参数 "PWM: 20kHz (50us)" |
| 5 | nFAULT 状态（绿色 OK / 红色 FAULT!） |
| 6-7 | 操作提示 |

### 3.2 M1/M2 独立控制

两个电机的方向和占空比**独立存储**。编码器切换电机后，LCD 显示切换到对应电机的参数，另一个电机保持原设定继续运行。

## 4. 软件 API

```c
#include "tsp_motor.h"

tsp_motor_init();                            // TIMA0 PWM 初始化 (20kHz)
SLEEP_HIGH();                                // 使能 H 桥
tsp_motor_set(MOTOR1, MOTOR_FORWARD, 50);    // M1 正向 50%
tsp_motor_set(MOTOR2, MOTOR_BACKWARD, 30);   // M2 反向 30%
tsp_motor_stop(MOTOR1);                      // 停止 M1 (coast)
tsp_motor_stop_all();                        // 停止全部
if (tsp_motor_fault()) { /* nFAULT LOW */ }  // 故障检测
SLEEP_LOW();                                 // 禁用 H 桥
```

### 4.1 初始化要点

1. TIMA0 不在 SysConfig 中配置，由 `tsp_motor_init()` 手动初始化
2. PB4/PB2 (DIR) 在 SysConfig GPIO 中配置为输出
3. PB3/PB0 (PWM) 在 `tsp_motor_init()` 中切换为 TIMA0 外设功能
4. `initPWMMode()` 设置 CC0/CC2 的 OCTL 为 `INIT_VAL_LOW`（SDK 默认），**不可覆盖为 `INIT_VAL_HIGH`**，否则 duty=0 时输出恒高

### 4.2 占空比计算

```
cc_val = duty_pct * 4000 / 100    (0..3999)
CC=0    → 输出 LOW (0%)
CC=3999 → 输出 HIGH (100%)
```

## 5. nFAULT 注意事项

nFAULT 是开漏输出，拓展板有 10kΩ 上拉到 MCU_3V3。

- **DRV8874 上电正常**：无故障时 nFAULT 不拉低 → 读 HIGH → "OK"
- **DRV8874 未上电**（无 VBAT）：开漏悬空，上拉拉高 → 也读 HIGH → 显示 "OK"
- **真实故障**（过流/过温）：DRV8874 拉低 nFAULT → 读 LOW → 显示 "FAULT!"

**软件无法区分"无 VBAT"与"正常无故障"**，需用户自行确认 SW1 是否打开。

## 6. 已修复的问题

| 编号 | 问题 | 根因 | 修复 |
|---|---|---|---|
| P1 | 编码器切换 M1/M2 无效 | `tsp_encoder_init()` 默认禁用中断，Motor Test 未调用 `tsp_encoder_enable()` | 进入时 enable，退出时 disable |
| P2 | TIMA0 enablePower 后无延迟 | 缺少 `delay_cycles(POWER_STARTUP_DELAY)` | 已补上 |
| P3 | PWM 极性反转，J10 无波形 | `setCaptureCompareOutCtl()` 手动覆盖为 `INIT_VAL_HIGH`，导致 CC=0 时输出恒高 | 删除覆盖，保留 SDK 默认 `INIT_VAL_LOW` |
| P4 | 进度条溢出 LCD | 20 段 + 前缀 = 27 字符 > LCD 20 字符 | 缩为 10 段 |
| P5 | nFAULT 显示不随 duty 更新 | 仅跟踪 fault 值，未跟踪 duty 变化 | 改为无条件刷新 |

## 7. SysConfig 迁移与 AD2 复测（2026-07-26）

TIMA0 已从手动初始化迁移到 SysConfig（实例 `MOTOR_PWM`，提交 `fec46c1`）。生成代码与原手写逐行等价，但 **P3 那条极性坑现在换了一种形式存在**，复测必须重点验证。

### 为什么必须复测

原手写代码**不调用** `setCaptureCompareOutCtl()`，靠 `initPWMMode()` 的默认值。SysConfig **会显式调用**它，值取自 `PWM_CHANNEL_x.initVal`（默认 `LOW`，已确认生成为 `INIT_VAL_LOW`）。此外 SysConfig 对 `ccValue` 的换算与本项目相反：

| | 本项目 `tsp_motor_set()` | SysConfig `updateCCValue()` |
|---|---|---|
| 公式 | `cc = duty*4000/100`（正比） | `cc = (100-duty)*period/100-1`（反比） |
| duty=0% | cc=0 | cc=3999 |
| duty=100% | cc=4000 | cc=0 |

已通过在 `.syscfg` 中固定 `ccValue = 0` 使复位状态与迁移前一致。**若复测发现占空比方向反了（设 20% 实测 80%），根因就在此处**，届时应修正 `tsp_motor_set()` 的换算方向，而不是去改 `.syscfg`。

### 复测步骤

**准备**：打开 SW1 接通 VBAT（否则 J10/J11 无输出）；DAPLink VTref 接 `MCU_3V3`；烧录当前固件。

| 步骤 | 操作 | 预期现象 | 若不符 |
|---|---|---|---|
| 1 | 开机，停在主菜单，AD2 探头接 J14 Pin13(M1IN2) | **无波形，恒低电平**。`timerStartTimer=false` 使计数器未启动 | 若已有 20kHz 波形 → `.syscfg` 的 `timerStartTimer` 被改成了 true |
| 2 | 进入 `Motor OpenLoop`，duty 保持 0% | 仍**恒低电平** | 若为恒高或满占空比 → `ccValue` 未固定为 0，见上表 |
| 3 | S1 按 4 次调到 duty=20% | 20kHz 方波（周期 50µs），**高电平占 20%**（10µs 高 / 40µs 低） | 高电平占 80% → 极性反了，改 `tsp_motor_set()` 换算方向 |
| 4 | 继续调到 50% / 80% | 高电平占比随之变化，周期始终 50µs | 周期不是 50µs → 检查 `.syscfg` 的 `timerCount`（应 3999）与 `clockPrescale`（应 1） |
| 5 | S2 切换方向 | J14 Pin14(M1IN1) 电平翻转；Pin13 PWM 波形不变 | DIR 不翻转 → 查 PB4 GPIO 配置 |
| 6 | 旋钮左旋切到 M2，重复步骤 3 | J14 Pin15(M2IN2) 出现同样波形；**M1 波形保持不变** | M1 跟着变 → CC0/CC2 通道映射错误 |
| 7 | 探头改接 J10（量程 ±25V） | 有 VBAT 时可见 20kHz 调制的电机驱动波形（幅值接近 VBAT） | 无输出 → 确认 SW1、检查 nSLEEP(PB1) 是否为高 |
| 8 | 退出菜单（PUSH） | J14 恢复恒低；LCD 回主菜单 | — |

**LCD 侧预期**：`Motor OpenLoop` 页面显示 `M1: FWD  20%`、进度条 `Duty:[##        ]`、`nFAULT: OK`。注意 nFAULT 在无 VBAT 时也显示 OK（见 §5），不能作为 VBAT 已接通的判据。

**闭环连带验证**：`Motor ClsLoop` 需接电机编码器才有意义（当前板载仅旋钮编码器，见 `control_modules.md`）。未接编码器时反馈恒为 0，PID 积分会把占空比推到上限 99 — 属预期行为，非缺陷。
