# CLAUDE.md

给 Claude Code 的编程参考。环境搭建、构建步骤、已知问题等见 `empty_mspm0g3519/iar/README.md`。

## 项目概述

MSPM0G3519 电赛基础平台（NUEDC-2026 SAIS@SJTU）。主控 TI `MSPM0G3519SPZR`（Cortex-M0+, 80MHz），IAR EWARM 9.60.3 + DAPLink。

工程入口：`empty_mspm0g3519/iar/empty_mspm0g3519.c`（开机动画 + 4 项交互菜单）。

## 工程结构

`$PROJ_DIR$` = `empty_mspm0g3519/iar/`，库目录与工程平级。`.ewp` 中路径使用 `$MSPM0_SDK_INSTALL_DIR$`、`$SYSCONFIG_ROOT$`。

```
empty_mspm0g3519/
├── iar/                              ← $PROJ_DIR$
│   ├── empty_mspm0g3519.c            ← 主程序
│   ├── empty_mspm0g3519.syscfg       ← SysConfig 配置
│   └── ti_msp_dl_config.c/.h         ← SysConfig 生成（勿手动编辑）
├── TSP3519/                           ← 板级支持库
│   ├── tsp_gpio.h/.c                  ← GPIO 宏（LED/蜂鸣器/LCD/CCD/按键/编码器）
│   ├── TSP_TFT18.h/.c                 ← TFT LCD 驱动（ST7735, 160×128, SPI1）
│   ├── tsp_ccd.h/.c                   ← TSL1401 线性 CCD（双通道，ADC0 软触发）[未启用]
│   └── tsp_menu.h/.c                  ← LCD 菜单系统（列表+子菜单+增量重绘）
└── NUEDC2025/                         ← 应用层驱动
    ├── tsp_isr.h/.c                   ← SysTick 延时 + GROUP1 中断分发
    ├── tsp_key.h/.c                   ← 4 键扫描（20ms 消抖，边沿检测）
    ├── tsp_encoder.h/.c               ← 编码器（PHA0 中断正交解码，20ms 速度）
    └── tsp_uart.h/.c                  ← UART0（MFCLK 4MHz, 115200-8N1, 环形缓冲 RX）
```

## 关键硬件约束

- **LED (PB5)**: **低电平点亮**，`LED_ON()` = `clearPins`
- **调试口 J1**: 红边对准 RST/1 脚。DAPLink VTref 接 `MCU_3V3`（不接 3.3V 输出）
- **供电**: 仅 USB-C，禁止多路同时供电
- **CPUCLK = 80MHz**: HFXT 40MHz → SYSPLL → 80MHz, Flash wait state = 2
- **SysTick**: 1ms (period=80000)，驱动 `delay_1ms()`
- **LFXT**: 32.768kHz 外部晶振已使能

## 全板引脚映射

| 类别 | 引脚 | 宏 |
|---|---|---|
| LED | PB5 | `LED_ON/OFF/TOGGLE` |
| 蜂鸣器 | PA13 | `BUZZ_ON/OFF/TOGGLE` |
| LCD SPI | PB30(PICO), PB31(SCLK), PB14(POCI) | `LCD_INST`=SPI1 |
| LCD 控制 | PA8(RST), PA9(BL), PB28(CS), PB29(DC) | `LCD_RST/BL/CS/DC` |
| 按键 | PA18(S0), PC0(S1), PA16(S2), PA12(PUSH) | `S0()/S1()/S2()/PUSH()` |
| 编码器 | PA14(PHA0), PA15(PHB0) | `PHA0()/PHB0()` |
| CCD | PC9(SI1), PB20(CLK1), PC4(SI2), PC5(CLK2) | `CCD_SI1/CLK1/SI2/CLK2` |
| 电源控制 | PB1(SLEEP), PA7(FAULT) | `SLEEP_HIGH/LOW`, `FAULT()` |
| UART0 | PA10(TX), PA11(RX) | IOMUX_PINCM21/22 |
| CCD ADC | PC2(CH12-CCD1), PC3(CH13-CCD2) | ADC0 手动配置 |

## API 速查

```c
// ===== 初始化顺序（见 main） =====
SYSCFG_DL_init();                      // SysConfig 生成（GPIO/SPI/时钟/SysTick）
tsp_tft18_init();                      // LCD
boot_animation();                      // 开机动画（色彩测试+启动信息+蜂鸣器）
tsp_encoder_init();                    // 编码器（默认禁用 PHA0 中断）
tsp_uart_init(115200);                  // UART0（SysConfig 预设 MFCLK 4MHz 时钟）
// tsp_ccd_init();                     // CCD [未启用，无外接模块]
tsp_key_init();                        // 按键
tsp_menu_init(title, items, count);    // 菜单

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
int32_t cnt = tsp_encoder_get_count(); // 原子读取
int16_t spd = tsp_encoder_get_speed(); // 脉冲/20ms
tsp_encoder_reset();

// ===== UART（tsp_uart.c，时钟=MFCLK 4MHz，PD0 安全） =====
tsp_uart_init(115200);                  // SysConfig 预设后再调（仅改波特率+缓冲）
tsp_uart_send_string("hello\r\n");
printf("val=%d\n", x);                 // 已重定向到 UART0
if (tsp_uart_available()) { uint8_t ch = tsp_uart_read_byte(); }
tsp_uart_rx_enable();                   // 按需开启 RX 中断（防止浮空噪声风暴）
tsp_uart_rx_disable();                  // 用完后关闭 RX 中断

// ===== CCD（tsp_ccd.c） =====
ccd_data_t pixels;                     // uint16_t[128]
tsp_ccd_snapshot(CCD1, pixels);        // flush → 曝光 → 读 128 像素
tsp_ccd_set_exposure(15);              // 曝光时间 ms
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
