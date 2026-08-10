# CLAUDE.md

给 Claude Code 的编程参考。架构约束见 [`ARCHITECTURE.md`](ARCHITECTURE.md)，环境搭建见根目录 [`README.md`](README.md)。

## ⚠️ 开发首要规则

1. **架构约束先行**:改 common 前读 `ARCHITECTURE.md`(依赖方向、稳定 API 契约)
2. **SysConfig 先行**:任何新外设先在对应工程 `.syscfg` 配置(引脚/时钟/电源域),参考 SDK 例程(`C:\ti\mspm0_sdk_2_10_00_04\examples\`),向用户求证后再生成 `ti_msp_dl_config.c/.h`,最后写应用代码
3. **编译验证**:改完必须 `iarbuild` 编译受影响工程(control 或 signal),0 errors
4. **共享代码禁止依赖题目特有代码**;跨题能力用 `TSP_USE_*` 编译宏隔离

## 项目概述

NUEDC-2026(SAIS@SJTU)电赛平台,比赛结束后重构为共享平台库 + 双题目应用:

```
G3519/
├── common/TSP3519/     ← 板级支持库(gpio/lcd/ccd/menu/头文件枢纽)
├── common/NUEDC2025/   ← 基础驱动(isr/key/encoder/uart/uart_k230)
├── common/drivers/     ← 通用外设驱动(motor/mpu6050/pid/ad5933/dds/adc/uart3)
├── common/docs/        ← 共享文档(3519_hardware 硬件手册/主板资料)
├── control/            ← 控制题: iar 工程 + app(循迹/里程计/K230/轮编码器)+ docs
└── signal/             ← 信号题: iar 工程 + app(FFT/示波器/TJC/cmd)+ docs
```

两套 IAR 工程:`control/iar/empty_mspm0g3519_nortos_iar.ewp`(控制题)、`signal/iar/empty_mspm0g3519_nortos_iar.ewp`(信号题)。

## 关键硬件约束

- **LED (PB5)**: 低电平点亮,`LED_ON()` = `clearPins`
- **调试口 J1**: 红边对准 RST/1 脚。DAPLink VTref 接 `MCU_3V3`
- **供电**: 仅 USB-C,禁止多路同时供电
- **CPUCLK = 80MHz**: HFXT 40MHz → SYSPLL → 80MHz, Flash wait state = 2
- **SysTick**: 1ms (period=80000),驱动 `delay_1ms()`
- **电机 VBAT**: 必须打开 SW1 接通电池才能在输出端看到 PWM 波形

## 引脚映射(主板)

| 类别 | 引脚 | 宏 |
|---|---|---|
| LED | PB5 | `LED_ON/OFF/TOGGLE` |
| 蜂鸣器 | PA13 | `BUZZ_ON/OFF/TOGGLE` |
| LCD SPI | PB30(PICO), PB31(SCLK), PB14(POCI) | `LCD_INST`=SPI1 |
| LCD 控制 | PA8(RST), PA9(BL), PB28(CS), PB29(DC) | `LCD_RST/BL/CS/DC` |
| 按键 | PA18(S0), PC0(S1), PA16(S2), PA12(PUSH) | `S0()/S1()/S2()/PUSH()` |
| 旋钮编码器 | PA14(PHA0), PA15(PHB0) | `PHA0()/PHB0()` |
| CCD 数字 | PC9(SI1), PB20(CLK1), PC4(SI2), PC5(CLK2) | `CCD_SI1/CLK1/SI2/CLK2` |
| CCD ADC | PB18(CH5-CCD1), PB19(CH6-CCD2), PB17(CH4-CCD3), PA17(CH2-CCD4) | `CCD_ADC_INST`=ADC1 |
| 电机 PWM | PB3(CCP0=M1_IN2), PB4(CCP1=M1_IN1), PB0(CCP2=M2_IN2), PB2(CCP3=M2_IN1) | `MOTOR_PWM_INST`=TIMA0 |
| 电源控制 | PB1(SLEEP), PA7(FAULT) | `SLEEP_HIGH/LOW`, `FAULT()` |
| 轮编码器1(右) | PB7(CCP0=PHB1), PB9(CCP1=PHA1), J12 | `WHEEL_ENC_R_INST`=TIMG9 QEI |
| 轮编码器2(左) | PB15(CCP0=PHB2), PB16(CCP1=PHA2), J13 | `WHEEL_ENC_L_INST`=TIMG8 QEI |
| UART6 (K230) | PC11(TX), PC10(RX), J11 | `UART_K230_INST` |
| UART3 (TJC 屏) | PC6(TX), PC7(RX), J4 | `UART_TJC_INST` |
| UART0 调试 | PA10(TX), PA11(RX) | MFCLK 4MHz, 115200 |
| MPU6050 (I2C0) | PB21(SCL), PB22(SDA), PC8(INT) | `IMU_I2C_INST` |
| AD5933 (I2C1) | PA29(SCL), PA30(SDA) | `I2C_AD5933_INST` |
| 通用 ADC (J2) | PA25(VIN1), PA24(VIN3), PB24(VIN4), PB23(VIN2), PA23(VIN5) | ADC0/ADC1 |
| PGA (MCP41010) | PC25(CS), PC26(SCK), PC27(SI) | `PGA_*` 宏 |
| DDS [封存] | PC2(SCLK), PC3(SDATA), PC24(FSYNC) | `DDS_*` 宏,与 CCD 共用引脚 |

## 编译方法

```bash
# 控制题
"D:/iar/ewarm-9.60.3/common/bin/iarbuild.exe" control/iar/empty_mspm0g3519_nortos_iar.ewp -build Debug
# 信号题
"D:/iar/ewarm-9.60.3/common/bin/iarbuild.exe" signal/iar/empty_mspm0g3519_nortos_iar.ewp -build Debug
```

IAR 全局变量:`MSPM0_SDK_INSTALL_DIR` = `C:\ti\mspm0_sdk_2_10_00_04`、`SYSCONFIG_ROOT` = `C:\ti\sysconfig_1.28.0`(Global 组,勿放 TI 组)。

## common API 速查(稳定契约,改动需评审)

```c
// ===== 延时(common/NUEDC2025/tsp_isr.c) =====
delay_1ms(100);

// ===== GPIO 宏(common/TSP3519/tsp_gpio.h) =====
LED_ON(); LED_OFF(); LED_TOGGLE();
BUZZ_ON(); BUZZ_OFF();

// ===== LCD(common/TSP3519/TSP_TFT18.c),y 为行号(16px/行) =====
tsp_tft18_show_str_color(x, y, "text", fcolor, bcolor);
tsp_tft18_clear(BLACK);

// ===== 按键(common/NUEDC2025/tsp_key.c),主循环 ~10ms 调 scan =====
tsp_key_scan();
if (tsp_key_pressed(KEY_S0)) { ... }   // 上升沿,自动清除
uint8_t held = tsp_key_state(KEY_PUSH);

// ===== 菜单(common/TSP3519/tsp_menu.c) =====
tsp_menu_init(title, items, count);    // S0↑ S1↓ S2确认 PUSH返回
uint8_t back = tsp_menu_run();
tsp_menu_switch(title, items, count);

// ===== 旋钮编码器(common/NUEDC2025/tsp_encoder.c) =====
tsp_encoder_enable(); / tsp_encoder_disable();  // 进入/退出场景时
int32_t cnt = tsp_encoder_get_count();
int16_t spd = tsp_encoder_get_speed();  // 脉冲/20ms

// ===== UART0(common/NUEDC2025/tsp_uart.c,MFCLK 4MHz) =====
// ⚠️ 全部 TX 带 10ms 超时,脱机(不接 DAPLink)不再阻塞
tsp_uart_init(115200);
tsp_uart_send_string("hello\r\n");
printf("val=%d\n", x);                 // 重定向到 UART0
tsp_uart_rx_enable(); / tsp_uart_rx_disable();  // 按需开启,防浮空噪声
if (tsp_uart_available()) { uint8_t ch = tsp_uart_read_byte(); }

// ===== UART6→K230(common/NUEDC2025/tsp_uart_k230.c,BUSCLK 80MHz) =====
tsp_uart_k230_init();
tsp_uart_k230_rx_enable(); / tsp_uart_k230_rx_disable();
tsp_uart_k230_send_string("...\n");    // TX 带超时

// ===== 双轮编码器(control/app/tsp_wheel_enc.c,TIMG8/TIMG9 QEI) =====
// ⚠️ 需要 TSP_USE_WHEEL_ENC 宏(仅 control 工程定义)
tsp_wheel_enc_start(); / tsp_wheel_enc_stop();
int16_t spd = tsp_wheel_enc_speed(MOTOR1);   // 右轮(脉冲/20ms)
int32_t cnt = tsp_wheel_enc_count(MOTOR2);   // 左轮累积
tsp_wheel_enc_reset();

// ===== 电机(common/drivers/tsp_motor.c,TIMA0 双 PWM 20kHz) =====
// 双 PWM 架构:驱动侧给 duty、空闲侧 0%,两方向快衰减;死区补偿 +50
// 镜像安装补偿:MOTOR1(右)/MOTOR2(左),MOTOR_FORWARD 统一为前进
tsp_motor_init();                 // 启动 TIMA0 计数器
SLEEP_HIGH();                     // 使能 H 桥(必须)
tsp_motor_set(MOTOR1, MOTOR_FORWARD, 50);
tsp_motor_stop_all();
if (tsp_motor_fault()) { ... }

// ===== MPU6050(common/drivers/tsp_mpu6050.c,I2C0) =====
int8_t ok = tsp_mpu6050_init();   // 0=成功
tsp_mpu6050_update_yaw();         // 主循环调用(自节拍 10ms)
float yaw = tsp_mpu6050_get_yaw();
tsp_mpu6050_reset_yaw();

// ===== PID(common/drivers/tsp_pid.c) =====
pid_pos_t p; tsp_pid_pos_init(&p, kp, ki, kd, out_min, out_max, int_max);
float out = tsp_pid_pos_step(&p, setpoint, measurement);
pid_inc_t i; tsp_pid_inc_init(&i, kp, ki, kd, out_min, out_max);  // 增量式 D-on-PV
float out = tsp_pid_inc_step(&i, setpoint, measurement, dt);

// ===== AD5933(common/drivers/tsp_ad5933.c,I2C1) =====
tsp_ad5933_init();
tsp_ad5933_set_sweep(start, delta, n, cyc, mult);
tsp_ad5933_start_sweep();
int16_t re = tsp_ad5933_read_real();  // 见 common/docs/AD5933_Use.md 标定流程

// ===== 通用 ADC(common/drivers/tsp_adc.c,J2 五通道) =====
uint16_t mv = tsp_adc_read_voltage_mv(ch);   // ch: VIN1/2/3/4/5
// 频率测量与 FFT 采样见 signal/app/tsp_fft.c

// ===== 信号题特有 =====
// FFT(signal/app/tsp_fft.c): 256 点 Q15 CFFT(CMSIS-DSP)
// TJC 屏(signal/app/tsp_tjc.c): UART3/J4,9600/115200
// 命令协议(signal/app/tsp_cmd.c): VER?/ADC,<ch>/FFT,<ch>[,FAST|MED|SLOW]

// ===== 控制题特有 =====
// CCD 循迹(control/app/tsp_linefollow.c): lf_process + lf_drive
// 里程计(control/app/tsp_odometer.c): 直线/旋转两种模式
// K230 解析(control/app/tsp_k230.c): 主循环 tsp_k230_task()
```

## SysConfig 模块(control 工程为准,signal 略有增减)

全部外设由 SysConfig 配置,应用代码不手动初始化外设寄存器。control 工程模块:Board/SYSCTL/SYSTICK/GPIO×3/SPI1(LCD)/UART1(UART_0)/UART2(UART_K230)/ADC12(CCD_ADC)/I2C(IMU_I2C)/PWM(MOTOR_PWM)/QEI×2(WHEEL_ENC)。signal 工程另有 UART3(UART_TJC)与 CMSIS-DSP 链接。

> **MOTOR_PWM 约定**(改 `.syscfg` 勿动):`timerStartTimer=false`(计数器由 `tsp_motor_init()` 启动);`ccValue=3998`(0% 占空比,CC 与 duty 反相:CC=0→100%)。
> **WHEEL_ENC**:QEI 计数器居中 0x8000,由 `tsp_wheel_enc_start()` 启动。

## 项目历史(2026 重构记录)

- 2026-07-22 ~ 07-29:比赛期间,三个独立仓库(G3519 基础平台、NUEDC2026-G3519-Control、NUEDC2026-Signal)并行开发
- 2026-08-10 整合:三分支历史并入 G3519 统一仓库(merge 保历史),旧仓库 GitHub 归档
- 2026-08-10 重构:单仓库单主线,common 平台库 + control/signal 应用(本次重构);共享模块内容级合并(如 tsp_uart 超时保护、uart6→uart_k230、tsp_gpio PGA 宏)
- 原控制题/信号题开发记录见 `control/docs/`、`signal/docs/` 与 git 历史
