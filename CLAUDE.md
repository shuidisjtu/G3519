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

MSPM0G3519 **信号题**专用平台（NUEDC-2026 SAIS@SJTU），从 G3519 综合平台拆分。主控 TI `MSPM0G3519SPZR`（Cortex-M0+, 80MHz），IAR EWARM 9.60.3 + DAPLink。

信号题核心模块：**AD5933 阻抗分析仪** + **AD9833 DDS 波形发生器**。

工程入口：`empty_mspm0g3519/iar/empty_mspm0g3519.c`。

## 工程结构

`$PROJ_DIR$` = `empty_mspm0g3519/iar/`，库目录与工程平级。`.ewp` 中路径使用 `$MSPM0_SDK_INSTALL_DIR$`、`$SYSCONFIG_ROOT$`。

```
empty_mspm0g3519/
├── iar/                              ← $PROJ_DIR$
│   ├── empty_mspm0g3519.c            ← 主程序
│   ├── empty_mspm0g3519.syscfg       ← SysConfig 配置
│   └── ti_msp_dl_config.c/.h        ← SysConfig 生成（勿手动编辑）
├── TSP3519/                           ← 板级支持库
│   ├── tsp_gpio.h/.c                  ← GPIO 宏（LED/蜂鸣器/LCD/按键/编码器/DDS）
│   ├── TSP_TFT18.h/.c                 ← TFT LCD 驱动（ST7735, 160×128, SPI1）
│   └── tsp_menu.h/.c                  ← LCD 菜单系统（列表+子菜单+增量重绘）
├── NUEDC2025/                         ← 应用层驱动
│   ├── tsp_isr.h/.c                   ← SysTick 延时 + GROUP1/UART0 中断分发
│   ├── tsp_key.h/.c                   ← 4 键扫描（20ms 消抖，边沿检测）
│   ├── tsp_encoder.h/.c               ← 编码器（PHA0 中断正交解码，20ms 速度）
│   ├── tsp_uart.h/.c                  ← UART0（MFCLK 4MHz, 115200-8N1, 环形缓冲 RX）
│   ├── tsp_ad5933.h/.c                ← AD5933 阻抗测量（I2C1, 100kHz, 温度+扫频）
│   └── tsp_dds.h/.c                   ← AD9833 DDS 波形发生器（GPIO bit-bang, 方波/正弦/三角波）
└── docs/                              ← 硬件文档与项目进度
    ├── development_reference/         ← 开发参考文档
    └── project_schedule/              ← 项目进度跟踪
```

## 关键硬件约束

- **LED (PB5)**: **低电平点亮**，`LED_ON()` = `clearPins`
- **调试口 J1**: 红边对准 RST/1 脚。DAPLink VTref 接 `MCU_3V3`（不接 3.3V 输出）
- **供电**: 仅 USB-C，禁止多路同时供电
- **CPUCLK = 80MHz**: HFXT 40MHz → SYSPLL → 80MHz, Flash wait state = 2
- **SysTick**: 1ms (period=80000)，驱动 `delay_1ms()`
- **LFXT**: 32.768kHz 外部晶振已使能

## 全板引脚映射（信号题相关）

| 类别 | 引脚 | 宏 |
|---|---|---|
| LED | PB5 | `LED_ON/OFF/TOGGLE` |
| 蜂鸣器 | PA13 | `BUZZ_ON/OFF/TOGGLE` |
| LCD SPI | PB30(PICO), PB31(SCLK), PB14(POCI) | `LCD_INST`=SPI1 |
| LCD 控制 | PA8(RST), PA9(BL), PB28(CS), PB29(DC) | `LCD_RST/BL/CS/DC` |
| 按键 | PA18(S0), PC0(S1), PA16(S2), PA12(PUSH) | `S0()/S1()/S2()/PUSH()` |
| 编码器 | PA14(PHA0), PA15(PHB0) | `PHA0()/PHB0()` |
| AD5933 (I2C1) | PA29(SCL), PA30(SDA) | `I2C_AD5933_INST`（SysConfig 宏） |
| DDS GPIO | PC2(SCLK), PC3(SDATA), PC24(FSYNC) | `DDS_SCLK/SDATA/FSYNC` |
| UART0 | PA10(TX), PA11(RX) | IOMUX_PINCM21/22 |
| ADC0 (J2) | PA25(VIN1/CH2), PA24(VIN3/CH3), PB24(VIN4/CH5) | `ADC12_0_INST` |
| ADC1 (J2) | PB23(VIN2/CH11), PA23(VIN5/CH12) | `ADC12_1_INST` |

## SysConfig 模块列表

当前 `.syscfg` 中已配置的模块：

| 模块 | SysConfig 名称 | 说明 |
|---|---|---|
| Board | Board | VDDA 配置、调试引脚 |
| SYSCTL | SYSCTL | 时钟树：HFXT 40MHz → SYSPLL → 80MHz |
| SYSTICK | SYSTICK | 1ms 中断 (period=80000) |
| GPIO1 | PORTB | 3 引脚：LED(PB5), LCD_CS(PB28), LCD_DC(PB29) |
| GPIO2 | PORTA | 8 引脚：S0(PA18), PHA0(PA14, 双边沿中断), PHB0(PA15), PUSH(PA12), BUZZ(PA13), S2(PA16), LCD_RST(PA8), LCD_BL(PA9) |
| GPIO3 | PORTC | 4 引脚：S1(PC0), DDS_SCLK(PC2), DDS_SDATA(PC3), DDS_FSYNC(PC24) |
| SPI1 | LCD | ST7735 LCD, BUSCLK, 10MHz, MOTO3 |
| UART1 | UART_0 | UART0, MFCLK 4MHz, PA10(TX)/PA11(RX) |
| I2C1 | I2C_AD5933 | AD5933, 100kHz Controller, PA29(SCL)/PA30(SDA) |
| ADC12 | ADC12_0 | ADC0, ULPCLK 40MHz, 2.5μs 采样, PA25(CH2), 轮询模式 |
| ADC12 | ADC12_1 | ADC1, ULPCLK 40MHz, 2.5μs 采样, PB23(CH11), 轮询模式 |

## API 速查

```c
// ===== 初始化顺序（见 main） =====
SYSCFG_DL_init();                      // SysConfig 生成（GPIO/SPI/时钟/SysTick）
tsp_tft18_init();                      // LCD
boot_animation();                      // 开机动画（色彩测试+启动信息+蜂鸣器）
tsp_encoder_init();                    // 编码器（默认禁用 PHA0 中断）
// tsp_uart_init(115200);              // UART0 [已移除：脱机 NRST=2.5V 时 TX 阻塞，见 README 已知问题]
tsp_key_init();                        // 按键
tsp_menu_init(title, items, count);    // 菜单（AD5933 Test, DDS Test, ADC Test）

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
tsp_menu_init(title, items, count);    // S0↑ S1↓ S2确认 PUSH返回
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
printf("val=%d\n", x);                 // 已重定向到 UART0（__write → DL_UART_transmitDataBlocking）
if (tsp_uart_available()) { uint8_t ch = tsp_uart_read_byte(); }
tsp_uart_rx_enable();                   // 按需开启 RX 中断（防止浮空噪声风暴）
tsp_uart_rx_disable();                  // 用完后关闭 RX 中断

// ===== AD5933 Impedance Analyzer（I2C1，详见 docs/AD5933_Use.md）=====
tsp_ad5933_init();                           // 复位 + 外部时钟 + 待机
float temp = tsp_ad5933_read_temperature();  // 读温度（°C），500ms 超时返回 NAN
if (temp != temp) { /* NAN → 超时，显示错误 */ }  // NaN 自检（不依赖 math.h）
tsp_ad5933_set_sweep(start, delta, n, cyc, mult); // 配置扫频参数
tsp_ad5933_start_sweep();                    // 启动扫频
int16_t re = tsp_ad5933_read_real();         // 读实部
int16_t im = tsp_ad5933_read_imag();         // 读虚部
// GainFactor 标定（应用层逻辑，已在 action_ad5933_test 中实现）：
//   1. 接已知 R_cal → start_sweep → 等 DATA_VALID → 读 Real/Imag
//   2. GF = 1 / (R_cal × sqrt(R² + I²))
//   3. 测未知：Z = 1 / (GF × sqrt(R² + I²))
//   4. 每次读完发 REPEAT_FREQ 触发重新测量（单频点模式）

// ===== AD9833 DDS Waveform Generator（GPIO bit-bang，详见 docs/AD9833_DDS_Use.md）=====
tsp_dds_set_output(1000, AD9833_SINE);       // 设置 1kHz 正弦波并开始输出
tsp_dds_set_output(5000, AD9833_SQUARE);     // 切换到 5kHz 方波
tsp_dds_stop();                               // 停止 DDS 输出 (RESET)
// 波形常量: AD9833_SINE, AD9833_TRIANGLE, AD9833_SQUARE
// 无需 init: 首次 tsp_dds_set_output() 即完成初始化
// DDS Test 交互: S0/S1 切换波形, 编码器调频率, PUSH 退出

// ===== 通用 ADC（J2 三路，tsp_adc.c/.h）=====
tsp_adc_init();                              // 配置 PA24/PB24 为模拟输入（PA25 由 SysConfig 配置）
tsp_adc_select_channel(ADC_CH_VIN1);         // 切换通道: ADC_CH_VIN1(CH2), ADC_CH_VIN2(CH11), ADC_CH_VIN3(CH3), ADC_CH_VIN4(CH5), ADC_CH_VIN5(CH12)
uint16_t raw = tsp_adc_read_raw();           // 单次 12-bit 采样（轮询模式）
uint16_t mv  = tsp_adc_read_mv();            // 返回 mV（0~3300）
uint16_t avg = tsp_adc_read_avg_mv(8);       // 8 次平均，返回 mV
uint32_t hz  = tsp_adc_measure_freq();       // burst 采样 + 过零检测，返回 Hz（0=DC）
// ADC Test 交互: S0/S1 切换通道, PUSH 退出
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
