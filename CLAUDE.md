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

MSPM0G3519 电赛基础平台（NUEDC-2026 SAIS@SJTU）。主控 TI `MSPM0G3519SPZR`（Cortex-M0+, 80MHz），IAR EWARM 9.60.3 + DAPLink。

工程入口：`empty_mspm0g3519/iar/empty_mspm0g3519.c`。

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
    ├── tsp_isr.h/.c                   ← SysTick 延时 + GROUP1/UART0/UART6 中断分发
    ├── tsp_key.h/.c                   ← 4 键扫描（20ms 消抖，边沿检测）
    ├── tsp_encoder.h/.c               ← 编码器（PHA0 中断正交解码，20ms 速度）
    ├── tsp_uart.h/.c                  ← UART0（MFCLK 4MHz, 115200-8N1, 环形缓冲 RX）
    ├── tsp_uart_k230.h/.c             ← UART6（K230, BUSCLK 80MHz, 115200, 环形缓冲 RX）
    └── tsp_k230.h/.c                  ← K230 YbProtocol 解析（主循环状态机, $...# 断帧）
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
| UART6 (K230) | PC11(TX), PC10(RX)，J11 排座 | `UART_K230_INST`（SysConfig 宏） |
| CCD ADC | PC2(CH12-CCD1), PC3(CH13-CCD2) | ADC0 手动配置 |

## API 速查

```c
// ===== 初始化顺序（见 main） =====
SYSCFG_DL_init();                      // SysConfig 生成（GPIO/SPI/时钟/SysTick）
tsp_tft18_init();                      // LCD
boot_animation();                      // 开机动画（色彩测试+启动信息+蜂鸣器）
tsp_encoder_init();                    // 编码器（默认禁用 PHA0 中断）
// tsp_uart_init(115200);              // UART0 [已移除：脱机 NRST=2.5V 时 TX 阻塞，见 README 已知问题]
tsp_uart_k230_init();                   // UART6→K230（SysConfig 已定 115200，RX 按需开）
tsp_k230_init();                        // K230 协议解析器复位
// tsp_ccd_init();                     // CCD [未启用，无外接模块]
tsp_key_init();                        // 按键
tsp_menu_init(title, items, count);    // 菜单（当前仅 1 项：K230 Test）

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

// ===== UART0（tsp_uart.c，时钟=MFCLK 4MHz，PD0 安全） =====
// ⚠️ 已从 main() 移除：脱机（不接 DAPLink）时 NRST=2.5V 导致 MFCLK 不稳定，
//    UART0 TX（含 printf）会永久阻塞。仅接 DAPLink 调试时可临时启用。见 README 已知问题。
tsp_uart_init(115200);                  // SysConfig 预设后再调（仅改波特率+缓冲）
tsp_uart_send_string("hello\r\n");
printf("val=%d\n", x);                 // 已重定向到 UART0（__write → DL_UART_transmitDataBlocking）
if (tsp_uart_available()) { uint8_t ch = tsp_uart_read_byte(); }
tsp_uart_rx_enable();                   // 按需开启 RX 中断（防止浮空噪声风暴）
tsp_uart_rx_disable();                  // 用完后关闭 RX 中断

// ===== K230 视觉模块（tsp_uart_k230.c + tsp_k230.c，UART6/J11，双向已验证） =====
// 坐标映射：K230 传感器 640×480 → LCD 画布 160×80 (y=16..95)
//   lcd_x = tgt.x / 4,  lcd_w = tgt.w / 4
//   lcd_y = 16 + tgt.y / 6,  lcd_h = tgt.h / 6
tsp_uart_k230_rx_enable();              // 进入使用场景时开启接收
tsp_k230_task();                        // 主循环调用：消费环形缓冲 + 解析 YbProtocol
k230_target_t t;
if (tsp_k230_get_target(&t)) { ... }    // 有新帧返回 1: t.func_id/x/y/w/h/msg
tsp_k230_frame_count();                 // 成功帧计数（错误帧见 error_count）
tsp_uart_k230_send_string("...\n");     // G3519→K230 TX（阻塞式，115200 约 0.87ms/10B）
tsp_uart_k230_rx_disable();             // 退出场景时关闭

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
