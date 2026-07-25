# CLAUDE.md

给 Claude Code 的编程参考。环境搭建、构建步骤、已知问题等见根目录 `README.md`。

## ⚠️ 开发首要规则：SysConfig 先行

在开发**任何**新功能/模块之前，必须按以下顺序操作：

1. **阅读 `empty_mspm0g3519/docs/` 中相关硬件文档**（引脚约束、电源域、时钟限制）
2. **查阅 SDK 官方例程**的 `.syscfg` 配置（路径 `C:\ti\mspm0_sdk_2_10_00_04\examples\`）
3. **在 `.syscfg` 中添加外设模块**，确认引脚/时钟源/电源域配置正确
4. **向用户求证** SysConfig 配置是否正确，用户确认后才能继续
5. 重新生成 `ti_msp_dl_config.c/.h`（可用 SysConfig CLI 或 IAR 自动触发）
6. 最后才写应用代码

> **反例**：UART0 调试时跳过 SysConfig 手动配置 BUSCLK (80MHz) 时钟源，忽略了 UART0 是 PD0 外设（最大 40MHz），导致波特率错误 + 中断风暴。正确做法是先读 `docs/M0G3519_UART_Use.md` 和 SDK `uart_echo_interrupts_standby` 例程，在 SysConfig 中用 MFCLK (4MHz)，再向用户确认。

## 项目概述

MSPM0G3519 控制题平台（NUEDC-2026 SAIS@SJTU），从 G3519 基础平台拆分，专注小车/控制类赛题。主控 TI `MSPM0G3519SPZR`（Cortex-M0+, 80MHz），IAR EWARM 9.60.3 + DAPLink。

工程入口：`empty_mspm0g3519/iar/empty_mspm0g3519.c`。

菜单项（8 项）：K230 Test、CCD Test、Motor OpenLoop、Motor ClsLoop、Odometer、Line Follow、MPU6050 Test、Speed Setting。

## 工程结构

`$PROJ_DIR$` = `empty_mspm0g3519/iar/`，库目录与工程平级。`.ewp` 中路径使用 `$MSPM0_SDK_INSTALL_DIR$`、`$SYSCONFIG_ROOT$`。

```
G3519_control/
├── empty_mspm0g3519/
│   ├── iar/                              ← $PROJ_DIR$
│   │   ├── empty_mspm0g3519.c            ← 主程序（开机动画 + 8 项菜单）
│   │   ├── empty_mspm0g3519.syscfg       ← SysConfig 配置
│   │   └── ti_msp_dl_config.c/.h         ← SysConfig 生成（勿手动编辑）
│   ├── TSP3519/                           ← 板级支持库
│   │   ├── tsp_gpio.h/.c                  ← GPIO 宏（LED/蜂鸣器/LCD/CCD/按键/编码器/电机）
│   │   ├── TSP_TFT18.h/.c                 ← TFT LCD 驱动（ST7735, 160x128, SPI1）
│   │   ├── tsp_ccd.h/.c                   ← 128 像素线阵 CCD（双通道，ADC1 序列采样）
│   │   └── tsp_menu.h/.c                  ← LCD 菜单系统（列表+子菜单+增量重绘）
│   ├── NUEDC2025/                         ← 应用层驱动
│   │   ├── tsp_isr.h/.c                   ← SysTick 延时 + GROUP1/UART0/UART6 中断分发
│   │   ├── tsp_key.h/.c                   ← 4 键扫描（20ms 消抖，边沿检测）
│   │   ├── tsp_encoder.h/.c               ← 编码器（PHA0 中断正交解码，20ms 速度）
│   │   ├── tsp_uart.h/.c                  ← UART0（MFCLK 4MHz, 115200-8N1, 环形缓冲 RX）
│   │   ├── tsp_uart_k230.h/.c             ← UART6（K230, BUSCLK 80MHz, 115200, 环形缓冲 RX）
│   │   ├── tsp_k230.h/.c                  ← K230 YbProtocol 解析（主循环状态机, $...# 断帧）
│   │   ├── tsp_motor.h/.c                 ← DRV8874 直流电机驱动（TIMA0 PWM, 20kHz, M1/M2独立控制）
│   │   ├── tsp_mpu6050.h/.c               ← MPU6050 六轴 IMU（I2C0 轮询, Yaw 积分）
│   │   ├── tsp_pid.h/.c                   ← PID 控制器（位置式 + 增量式 D-on-PV）
│   │   ├── tsp_linefollow.h/.c            ← CCD 循迹（PD 转向 + PD 速度 + 差速驱动）
│   │   └── tsp_odometer.h/.c              ← 里程计（编码器距离/角度测量）
│   ├── docs/                              ← 硬件文档与项目进度
│   │   ├── development_reference/         ← 开发参考（硬件手册、驱动使用说明）
│   │   ├── project_schedule/              ← 项目进度（控制题模块进度表）
│   │   └── k230_firmware_ref/             ← K230 亚博固件参考代码
│   └── k230_scripts/                      ← K230 MicroPython 测试脚本（CanMV IDE 中运行）
└── k230_scripts/                          ← K230 脚本（根目录副本，便于快速访问）
```

## 关键硬件约束

- **LED (PB5)**: **低电平点亮**，`LED_ON()` = `clearPins`
- **调试口 J1**: 红边对准 RST/1 脚。DAPLink VTref 接 `MCU_3V3`（不接 3.3V 输出）
- **供电**: 仅 USB-C，禁止多路同时供电
- **CPUCLK = 80MHz**: HFXT 40MHz -> SYSPLL -> 80MHz, Flash wait state = 2
- **SysTick**: 1ms (period=80000)，驱动 `delay_1ms()`
- **LFXT**: 32.768kHz 外部晶振已使能
- **电机 VBAT**: 必须打开 SW1 接通电池才能在输出端看到 PWM 波形

## 控制题引脚映射

| 类别 | 引脚 | 宏 |
|---|---|---|
| LED | PB5 | `LED_ON/OFF/TOGGLE` |
| 蜂鸣器 | PA13 | `BUZZ_ON/OFF/TOGGLE` |
| LCD SPI | PB30(PICO), PB31(SCLK), PB14(POCI) | `LCD_INST`=SPI1 |
| LCD 控制 | PA8(RST), PA9(BL), PB28(CS), PB29(DC) | `LCD_RST/BL/CS/DC` |
| 按键 | PA18(S0), PC0(S1), PA16(S2), PA12(PUSH) | `S0()/S1()/S2()/PUSH()` |
| 编码器 | PA14(PHA0), PA15(PHB0) | `PHA0()/PHB0()` |
| CCD 数字 | PC9(SI1), PB20(CLK1), PC4(SI2), PC5(CLK2) | `CCD_SI1/CLK1/SI2/CLK2` |
| CCD ADC | PB18(CH5-CCD1), PB19(CH6-CCD2), PB17(CH4-CCD3), PA17(CH2-CCD4) | `CCD_ADC_INST`=ADC1 (SysConfig) |
| 电机 PWM | PB3(CCP0), PB0(CCP2) | `MOTOR_PWM_INST`=TIMA0 |
| 电机 DIR | PB4(M1DIR), PB2(M2DIR) | `PORTB_M1DIR/M2DIR_PIN` |
| 电源控制 | PB1(SLEEP), PA7(FAULT) | `SLEEP_HIGH/LOW`, `FAULT()` |
| UART6 (K230) | PC11(TX), PC10(RX)，J11 排座 | `UART_K230_INST`（SysConfig 宏） |
| UART0 | PA10(TX), PA11(RX) | IOMUX_PINCM21/22 |
| MPU6050 (I2C0) | PB21(SCL), PB22(SDA), PC8(INT) | `IMU_I2C_INST`（SysConfig 宏） |

## SysConfig 模块列表

当前 `.syscfg` 中已配置的模块：

| SysConfig 模块 | 实例名 | 用途 |
|---|---|---|
| Board | - | DEBUGSS (SWD) |
| SYSCTL | - | 时钟树（HFXT 40MHz -> SYSPLL -> 80MHz） |
| SYSTICK | - | 1ms 定时（period=80000） |
| GPIO1 | PORTB | 7 引脚：LED/CCD_CLK1/SLEEP/LCD_CS/LCD_DC/M1DIR/M2DIR |
| GPIO2 | PORTA | 9 引脚：S0/PHA0/PHB0/PUSH/BUZZ/S2/FAULT/LCD_RST/LCD_BL |
| GPIO3 | PORTC | 4 引脚：CCD_SI1/CCD_SI2/CCD_CLK2/S1 |
| SPI1 | LCD | ST7735 LCD（BUSCLK, 10MHz, PB30/PB31/PB14） |
| UART1 | UART_0 | 调试串口（MFCLK 4MHz, PA10/PA11） |
| UART2 | UART_K230 | K230 视觉模块（BUSCLK 80MHz, 115200, PC10/PC11） |
| ADC12 | CCD_ADC | CCD 模拟采样（ADC1, ULPCLK/8, 序列 MEM0-3→CH5/6/4/2） |
| I2C | IMU_I2C | MPU6050 六轴 IMU（I2C0, 400kHz, PB21-SCL/PB22-SDA） |

**手动初始化（不在 SysConfig 中）**：
- **TIMA0**：电机 PWM（20kHz），在 `tsp_motor_init()` 中手动配置

## API 速查

```c
// ===== 初始化顺序（见 main） =====
SYSCFG_DL_init();                      // SysConfig 生成（GPIO/SPI/时钟/SysTick）
tsp_tft18_init();                      // LCD
boot_animation();                      // 开机动画（色彩测试+启动信息+蜂鸣器）
tsp_encoder_init();                    // 编码器（默认禁用 PHA0 中断）
// tsp_uart_init(115200);              // UART0 [已移除：脱机 NRST=2.5V 时 TX 阻塞，见 README 已知问题]
tsp_uart_k230_init();                   // UART6->K230（SysConfig 已定 115200，RX 按需开）
tsp_k230_init();                        // K230 协议解析器复位
// tsp_ccd_init();                     // CCD 在 action_ccd_test() 中按需初始化
// tsp_motor_init();                   // 电机在 action_motor_test() 中按需初始化
tsp_key_init();                        // 按键
tsp_menu_init(title, items, count);    // 菜单（8 项：K230/CCD/MotorOpen/MotorCls/Odom/LineFol/MPU/SpeedSet）

// ===== GPIO 宏（tsp_gpio.h） =====
LED_ON(); LED_OFF(); LED_TOGGLE();
BUZZ_ON(); BUZZ_OFF();

// ===== 延时（tsp_isr.c） =====
delay_1ms(100);

// ===== LCD（TSP_TFT18.c），y 参数为行号（16px/行） =====
tsp_tft18_show_str_color(x, y, "text", fcolor, bcolor);
tsp_tft18_show_int16(x, y, val);
tsp_tft18_show_uint16(x, y, val);
tsp_tft18_clear(BLACK);

// ===== 按键（tsp_key.c），主循环每 ~10ms 调 scan =====
tsp_key_scan();
if (tsp_key_pressed(KEY_S0)) { ... }   // 上升沿，触发一次自动清除
uint8_t held = tsp_key_state(KEY_PUSH);

// ===== 菜单（tsp_menu.c） =====
tsp_menu_init(title, items, count);    // S0(上) S1(下) S2(确认) PUSH(返回)
uint8_t back = tsp_menu_run();         // 主循环调用
tsp_menu_switch(title, items, count);  // 切换子菜单

// ===== 编码器（tsp_encoder.c） =====
tsp_encoder_enable();                  // 启用 PHA0 中断 + 复位计数（进入编码器场景时调用）
tsp_encoder_disable();                 // 禁用 PHA0 中断 + 复位（退出时调用，防浮空中断）
int32_t cnt = tsp_encoder_get_count(); // 原子读取
int16_t spd = tsp_encoder_get_speed(); // 脉冲/20ms
tsp_encoder_reset();

// ===== UART0（tsp_uart.c，时钟=MFCLK 4MHz，PD0 安全） =====
// ⚠️ 已从 main() 移除：脱机（不接 DAPLink）时 NRST=2.5V 导致 MFCLK 不稳定，
//    UART0 TX（含 printf）会永久阻塞。仅接 DAPLink 调试时可临时启用。见 README 已知问题。
tsp_uart_init(115200);                  // SysConfig 预设后再调（仅改波特率+缓冲）
tsp_uart_send_string("hello\r\n");
printf("val=%d\n", x);                 // 已重定向到 UART0（__write -> DL_UART_transmitDataBlocking）
if (tsp_uart_available()) { uint8_t ch = tsp_uart_read_byte(); }
tsp_uart_rx_enable();                   // 按需开启 RX 中断（防止浮空噪声风暴）
tsp_uart_rx_disable();                  // 用完后关闭 RX 中断

// ===== K230 视觉模块（tsp_uart_k230.c + tsp_k230.c，UART6/J11，双向已验证） =====
// 坐标映射：K230 传感器 640x480 -> LCD 画布 160x80 (y=16..95)
//   lcd_x = tgt.x / 4,  lcd_w = tgt.w / 4
//   lcd_y = 16 + tgt.y / 6,  lcd_h = tgt.h / 6
tsp_uart_k230_rx_enable();              // 进入使用场景时开启接收
tsp_k230_task();                        // 主循环调用：消费环形缓冲 + 解析 YbProtocol
k230_target_t t;
if (tsp_k230_get_target(&t)) { ... }    // 有新帧返回 1: t.func_id/x/y/w/h/msg
tsp_k230_frame_count();                 // 成功帧计数（错误帧见 error_count）
tsp_uart_k230_send_string("...\n");     // G3519->K230 TX（阻塞式，115200 约 0.87ms/10B）
tsp_uart_k230_rx_disable();             // 退出场景时关闭

// ===== CCD（tsp_ccd.c）=====
// 128 像素线阵 CCD，4通道2组：
//   Group1: CCD1(J3: PB18-AO, PC9-SI, PB20-CLK) + CCD2(PB19-AO)
//   Group2: CCD3(J17: PB17-AO, PC4-SI, PC5-CLK) + CCD4(PA17-AO)
// ADC1 via SysConfig（实例 CCD_ADC，序列模式 MEM0-3→CH5/6/4/2）
// VREF = VDDA (3.3V)，BUSY 轮询带超时 (10000 cycles)
ccd_data_t pixels;                     // uint16_t[128]，注意栈占用 256B
tsp_ccd_init();                        // ADC1 (reset+power+clock) + GPIO -> 模拟/数字引脚
tsp_ccd_snapshot(CCD1, pixels);        // flush -> 曝光 -> 读 128 像素（ADC1 CH5）
tsp_ccd_snapshot(CCD2, pixels);        // 同上，读取 MEM1 (ADC1 CH4)
tsp_ccd_set_exposure(10);              // 曝光时间 ms (1-100, 默认 10)
// LCD 波形绘制见 action_ccd_test(): 128px x 63px 区域，erasure-based 增量更新

// ===== DRV8874 DC Motor Driver（TIMA0 手动初始化, 不在SysConfig中）=====
// 硬件: PB3(CCP0=M1 PWM), PB4(GPIO=M1 DIR), PB0(CCP2=M2 PWM), PB2(GPIO=M2 DIR)
// 控制: nSLEEP=PB1, nFAULT=PA7(10kΩ上拉, 无VBAT时也读HIGH)
// ⚠️ initPWMMode() 设 CC OCTL = INIT_VAL_LOW (SDK默认), 不可覆盖为 INIT_VAL_HIGH
// AD2: J10(M1)/J11(M2) 需VBAT; 无VBAT时接J14侧IN1/IN2看3.3V逻辑
tsp_motor_init();                            // 初始化 TIMA0 PWM (20kHz)
SLEEP_HIGH();                                // 使能 H 桥 (必须在 set 之前)
tsp_motor_set(MOTOR1, MOTOR_FORWARD, 50);    // 电机1 正向 50%
tsp_motor_set(MOTOR2, MOTOR_BACKWARD, 30);   // 电机2 反向 30% (M1/M2 独立控制)
tsp_motor_stop(MOTOR1);                      // 停止电机1 (coast)
tsp_motor_stop_all();                        // 停止全部
if (tsp_motor_fault()) { ... }               // 检测 nFAULT (LOW=故障)
SLEEP_LOW();                                 // 禁用 H 桥

// ===== MPU6050 六轴 IMU（tsp_mpu6050.c，I2C0 轮询，SysConfig IMU_I2C） =====
// 硬件: I2C0 (PB21-SCL, PB22-SDA), 400kHz, AD0=GND(0x68), INT=PC8(未用)
int8_t ok = tsp_mpu6050_init();              // 复位+配置(检查每步write_reg)，校准gyroZ，返回 0=成功 -1=失败
uint8_t id = tsp_mpu6050_who_am_i();         // 读 WHO_AM_I（成功=0x68，I2C失败=0xFF）
mpu6050_raw_t mpu;
tsp_mpu6050_read_raw(&mpu);                  // 14B 突发读：mpu.ax/ay/az/temp/gx/gy/gz (int16_t)
tsp_mpu6050_yaw_enable();                    // 启用 Yaw 积分
tsp_mpu6050_update_yaw();                    // 主循环调用（自节拍 10ms，读 gyro Z 积分）
float yaw = tsp_mpu6050_get_yaw();           // 当前 Yaw 角度（°）
tsp_mpu6050_reset_yaw();                     // 清零 Yaw
tsp_mpu6050_yaw_disable();                   // 禁用 Yaw 积分
int16_t ofs = tsp_mpu6050_get_gz_offset();   // 校准偏移值（诊断用）

// ===== PID 控制器（tsp_pid.c，位置式 + 增量式 D-on-PV） =====
// 位置式：循迹转向 PD、速度 PD（Ki=0 时退化为 PD）
pid_pos_t steer_pid;
tsp_pid_pos_init(&steer_pid, kp, ki, kd, out_min, out_max, int_max);
float out = tsp_pid_pos_step(&steer_pid, setpoint, measurement);
tsp_pid_pos_reset(&steer_pid);

// 增量式 D-on-PV：电机闭环速度控制（防设定值突变微分冲击）
pid_inc_t spd_pid;
tsp_pid_inc_init(&spd_pid, kp, ki, kd, out_min, out_max);
float out = tsp_pid_inc_step(&spd_pid, setpoint, measurement, dt);
tsp_pid_inc_reset(&spd_pid);

// ===== CCD 循迹（tsp_linefollow.c，PD 转向 + PD 速度 + 差速驱动） =====
// 算法：CCD 128px 扫描黑线边沿 → 中线误差 → PD 转向 + PD 速度 → 差速电机输出
// 丢线保护：≤3帧保持、>3帧渐减、>13帧停车
tsp_linefollow_init();                         // 初始化 CCD/PID/状态（电机/编码器由调用者初始化）
lf_result_t lf;
tsp_linefollow_process(CCD1, &lf);             // 采集+检测+PD计算，结果存入 lf
tsp_linefollow_drive(&lf);                     // 将 lf 结果输出到电机（差速驱动）
tsp_linefollow_demo();                         // 交互式循迹演示（LCD+按键，S2 启停）
tsp_linefollow_set_speed(50);                  // 设置基础速度 (0-100)
tsp_linefollow_set_steer_gains(kp, kd);        // 设置转向 PD 增益
tsp_linefollow_set_speed_gains(kp, kd);        // 设置速度 PD 增益

// ===== 里程计（tsp_odometer.c，编码器距离/角度测量） =====
// 两种模式：STRAIGHT（直线距离 cm）、ROTATE（旋转角度 deg）
// 编码器脉冲积分，LCD 增量刷新（距离/角度/原始脉冲/速度）
tsp_odometer_demo();                           // 交互式里程计（S0:直线 S1:旋转 S2:清零 PUSH:退出）
```

## IAR 关键路径

| 文件 | 路径 |
|---|---|
| 编译器 | `D:\iar\ewarm-9.60.3\arm\bin\iccarm.exe` |
| CMSIS-DAP 驱动 | `D:\iar\ewarm-9.60.3\arm\bin\swtdarm_cmsisdap.dll` |
| G3519 DDF | `D:\iar\ewarm-9.60.3\arm\config\debugger\TexasInstruments\MSPM0G3519.ddf` |
| Flash loader | `FlashMSPM0GX519.*` |
| SDK | `C:\ti\mspm0_sdk_2_10_00_04` |
| SysConfig | `C:\ti\sysconfig_1.28.0` |
