# DRV8874 直流电机驱动使用说明

> 状态：**已验证**（2026-07-26）— 双 PWM 4 通道架构，两轮同步起转、方向正确、开环速度对称。

## 1. 硬件概要

| 项目 | 说明 |
|---|---|
| 驱动芯片 | DRV8874PWPR × 2（U6=M1/右轮, U7=M2/左轮） |
| 控制模式 | PMODE=HIGH（R43 上拉） |
| VM 电源 | VBAT（经 XT30/SW1），**必须打开 SW1** |
| 逻辑电源 | VREF=MCU_3V3 |
| PWM 源 | TIMA0 (80MHz BUSCLK)，20kHz，period=3999，LOAD=3998 |
| nSLEEP | PB1，两路共用，HIGH=使能 |
| nFAULT | PA7，两路共用，开漏+10kΩ 上拉到 MCU_3V3 |

### 1.1 引脚映射（4 通道 PWM，无 GPIO 方向引脚）

| 功能 | MCU 引脚 | IOMUX | 定时器通道 |
|---|---|---|---|
| M1 IN2 (PWM) | PB3 | PINCM16 | TIMA0_CCP0 (CC0) |
| M1 IN1 (PWM) | PB4 | PINCM17 | TIMA0_CCP1 (CC1) |
| M2 IN2 (PWM) | PB0 | PINCM12 | TIMA0_CCP2 (CC2) |
| M2 IN1 (PWM) | PB2 | PINCM15 | TIMA0_CCP3 (CC3) |

> **旧架构**（已废弃）：PB4/PB2 曾作为 GPIO 方向引脚（IN1=静态电平）。该方案导致正向（IN1=L, fast-decay）和反向（IN1=H, slow-decay）的衰减模式不同，造成两轮起转阈值严重不对称（5% vs 55%）。2026-07-26 迁移为双 PWM 架构，两个方向均为 fast-decay。

### 1.2 DRV8874 双 PWM 控制逻辑 (PMODE=HIGH)

| 模式 | IN1 | IN2 | 输出 | 衰减模式 |
|---|---|---|---|---|
| Forward | 0% (LOW) | duty% (PWM) | OUT1→OUT2 | PWM-low: (0,0)=Coast = **fast-decay** |
| Backward | duty% (PWM) | 0% (LOW) | OUT2→OUT1 | PWM-low: (0,0)=Coast = **fast-decay** |
| Coast | 0% (LOW) | 0% (LOW) | Hi-Z | — |
| Brake | 100% (HIGH) | 100% (HIGH) | 低侧制动 | — |

> **关键**：驱动侧给 duty，空闲侧钉 0%。两个方向的 PWM-low 态均为 (IN1=0, IN2=0)=Coast=fast-decay，消除了旧架构的衰减模式不对称问题。

### 1.3 镜像安装补偿

差速底盘左右电机安装方向镜像，同一电气方向产生相反的车轮旋转。`tsp_motor_set()` 内部对调 MOTOR1/MOTOR2 的 IN1/IN2 通道角色：
- `MOTOR_FORWARD` → MOTOR1 驱动 IN2、MOTOR2 驱动 IN1 → 两轮同向前进
- `MOTOR_BACKWARD` → 反之 → 两轮同向后退

### 1.4 死区补偿

| 参数 | 值 | 说明 |
|---|---|---|
| `MOTOR_DEAD_ZONE` | 50 | 任何非零请求自动 +50，映射到可用转速区间 |
| `MOTOR_DC_LIMIT` | 99 | 最大占空比限制 |

用户设 1% → 实际输出 51%（刚越过起转阈值）。设 0% → 直通 0%，不补偿。

## 2. 测试接口

| 接口 | 信号 | 用途 |
|---|---|---|
| J10 / JM1 | M1+, M1- | 电机 1 输出（DRV8874 OUT1/OUT2） |
| J11 / JM2 | M2+, M2- | 电机 2 输出 |
| J14 Pin 13-16 | M1IN2/M1IN1/M2IN2/M2IN1 | 逻辑侧 4 路 PWM（3.3V，可接逻辑分析仪） |

## 3. 菜单操作 (Motor OpenLoop)

进入路径：主菜单 → Motor OpenLoop

| 按键 | 功能 |
|---|---|
| S0 | Duty -5% |
| S1 | Duty +5% |
| S2 | 切换方向 (FWD↔REV) |
| 旋钮左转 | 切换到 M2 |
| 旋钮右转 | 切换到 M1 |
| PUSH | 退出 |

## 4. 软件 API

```c
#include "tsp_motor.h"

tsp_motor_init();                            // 复位 4 通道 CC + 启动 TIMA0
SLEEP_HIGH();                                // 使能 H 桥
tsp_motor_set(MOTOR1, MOTOR_FORWARD, 50);    // 右轮前进（实际 99%，受 DC_LIMIT 限制）
tsp_motor_set(MOTOR2, MOTOR_FORWARD, 30);    // 左轮前进（实际 80%）
tsp_motor_set(MOTOR1, MOTOR_BRAKE, 0);       // 右轮制动（IN1=H, IN2=H）
tsp_motor_stop(MOTOR1);                      // 停止右轮 (coast)
tsp_motor_stop_all();                        // 停止全部
if (tsp_motor_fault()) { /* nFAULT LOW */ }  // 故障检测
SLEEP_LOW();                                 // 禁用 H 桥
```

### 4.1 CC 与 Duty 关系

EDGE_ALIGN 模式下计数器从 LOAD(=3998) 倒数，LACT=CCP_HIGH（LOAD 时输出高），CDACT=CCP_LOW（匹配时输出低）。

```
CC 值越大 → 匹配越早 → 高电平越短 → duty 越低
duty_to_cc(duty_pct) = LOAD - (duty_pct * LOAD / 100)

duty=0%   → CC=3998 (LOAD)，输出始终低
duty=100% → CC=0，输出始终高
```

SysConfig 中 `ccValue = 3998` 对应 0% 占空比初始状态。

## 5. nFAULT 注意事项

nFAULT 是开漏输出，拓展板有 10kΩ 上拉到 MCU_3V3。

- **DRV8874 上电正常**：无故障时 nFAULT 不拉低 → 读 HIGH → "OK"
- **DRV8874 未上电**（无 VBAT）：开漏悬空，上拉拉高 → 也读 HIGH → 显示 "OK"
- **真实故障**（过流/过温）：DRV8874 拉低 nFAULT → 读 LOW → 显示 "FAULT!"

**软件无法区分"无 VBAT"与"正常无故障"**，需用户自行确认 SW1 是否打开。

## 6. 已修复的问题

| 编号 | 问题 | 根因 | 修复 |
|---|---|---|---|
| P1 | 进 OpenLoop duty=0% 电机满速转 | CC 值跨场景残留 + 未在启动前推零 | `tsp_motor_init()` 复位 4 通道 CC，入口调 `tsp_motor_stop_all()` |
| P2 | ccValue=3999 → 两轮 100% | LOAD=period-1=3998, CC>LOAD 不匹配 → 输出钉高 | `.syscfg` 中 ccValue 改为 3998 |
| P3 | 两轮转向相反 | 镜像安装，同一电气方向产生反向车轮旋转 | `tsp_motor_set()` 对调 M1/M2 通道角色 |
| P4 | 右轮 5% 飞快、左轮 55% 才转 | IN1 为 GPIO 时正/反向衰减模式不同（fast vs slow decay） | PB4/PB2 改为 TIMA0 CCP1/CCP3，双 PWM 架构 |
| P5 | 开环两轮起转阈值高（~55%） | 静摩擦死区 | 添加 MOTOR_DEAD_ZONE=50 死区补偿 |

## 7. 参考工程

[HSPv2](https://github.com/yyx1248722477/hsp.git) `EDC26_HSPv2/Utilities/HSP_MOTOR.c` — 使用相同底盘和扩展板，4 通道 PWM（GD32 TIMER0 CH0-CH3），`MOTOR_DEAD_ZONE=38`，`MOTOR_DC_LIMIT=99`。本项目架构直接参照该工程设计。
