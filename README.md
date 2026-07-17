# MSPM0G3519 NUEDC-2026 电赛基础平台

基于 TI **MSPM0G3519SPZR**（Arm Cortex-M0+, 80MHz）的电赛开发平台，集成 LCD 菜单交互系统与多外设驱动，使用 IAR EWARM + DAPLink (CMSIS-DAP) 开发工具链。

## 功能概览

启动后播放**开机动画**（色彩测试 → 启动信息 → LED 闪烁 + 蜂鸣器短响），然后进入 **TFT LCD 菜单界面**，支持 5 项交互功能：

| 菜单项 | 功能 |
|---|---|
| **LED Menu** | 子菜单：LED Toggle（翻转）/ LED Blink（~2Hz 背景闪烁） |
| **Buzzer Test** | 蜂鸣器短响 80ms |
| **Show Counter** | 计数器演示（4 键计数，PUSH 退出） |
| **UART Test** | UART0 串口测试（TX / printf / RX 回显 + LCD 显示） |
| **About** | 显示 "NUEDC-2026 SAIS@SJTU" |

按键角色：**S0**=上移、**S1**=下移、**S2**=确认、**PUSH**=返回

> **暂未启用的模块**：CCD（无外接模块）、编码器菜单项（编码器驱动已初始化但未加入菜单）。代码保留在工程中，接上硬件后可直接启用。

## 硬件连接

| 项目 | 说明 |
|---|---|
| **主控** | TI MSPM0G3519SPZR（100 引脚 LQFP） |
| **LED** | D1 接 **PB5**（物理引脚 26），**低电平点亮** |
| **蜂鸣器** | PA13，有源蜂鸣器，高电平驱动 |
| **TFT LCD** | ST7735 160×128，SPI1（PB30/PB31/PB14），10MHz |
| **按键** | S0(PA18)、S1(PC0)、S2(PA16)、PUSH(PA12) |
| **编码器** | PHA0(PA14, 双边沿中断)、PHB0(PA15) |
| **CCD** | TSL1401 双通道，SI/CLK GPIO + ADC0(PC2/PC3) |
| **UART** | UART0 MFCLK 4MHz, 115200-8N1，PA10(TX)/PA11(RX) |
| **调试器** | DAPLink (CMSIS-DAP v2, VID_0D28&PID_0204) |
| **供电** | USB-C，禁止多路同时供电 |

## 开发注意事项

> **SysConfig 先行**：在开发任何新功能/模块之前，务必先在 `empty_mspm0g3519.syscfg` 中正确配置对应外设（引脚、时钟源、电源域），重新生成 `ti_msp_dl_config.c/.h`，确认无误后再写应用代码。参考 `empty_mspm0g3519/docs/` 中的硬件文档和 SDK 官方例程。
>
> **教程参考**：[立创·泰山派 TI MSPM0 系列教程](https://wiki.lckfb.com/zh-hans/ti-series/) 是良好的 MSPM0 开发引导和示例参考。**但该教程使用的开发板（梁山派/泰山派等）与本项目 G3519 硬件平台不配套**，其引脚定义、外设配置、时钟树等不可直接套用，必须以本项目 `docs/` 中的硬件文档和 `empty_mspm0g3519.syscfg` 配置为最终依据。
>
> **K230 视觉模块**：亚博 K230 视觉识别模块的对接方案（UART6/J11 接线、CanMV 用法、协议设计）见 [`docs/K230_Vision_Module_Use.md`](empty_mspm0g3519/docs/K230_Vision_Module_Use.md)（接线就绪，软件开发中）。

## 开发环境要求

| 工具 | 版本 | 说明 |
|------|------|------|
| **IAR EWARM** | 9.60.3 | IDE + 编译器 + 调试器 |
| **TI MSPM0 SDK** | 2.10.00.04 | driverlib、启动文件、SysConfig、示例 |
| **SysConfig** | 1.28.0 | 引脚/外设图形化配置工具 |

## 环境搭建（首次使用）

### 1. 安装 IAR EWARM

确保 IAR EWARM 9.60.3 已安装。其他 9.x 版本理论上也可用，但未经验证。

### 2. 安装 TI MSPM0 SDK

从 TI 官网下载安装 [MSPM0 SDK 2.10.00.04](https://www.ti.com/tool/MSPM0-SDK)，记录安装路径（默认 `C:\ti\mspm0_sdk_2_10_00_04`）。

### 3. 安装 SysConfig

从 TI 下载 [SysConfig 1.28.0](https://www.ti.com/tool/SYSCONFIG)，记录安装路径。

> 如果你已安装 TI Code Composer Studio (CCS)，其中包含 SysConfig，路径类似 `D:\ti\ccs2100\ccs\utils\sysconfig_1.28.0`。

### 4. 配置 IAR 全局变量

在 IAR 中设置两个全局自定义参数变量：

1. 打开 IAR → `Tools` → `Configure Custom Argument Variables...`
2. 切换到 `Global` 选项卡
3. 添加以下变量（**必须在 Global 下，不要放在 TI 组**）：

| 变量名 | 值（根据你的实际路径调整） |
|--------|---------------------------|
| `MSPM0_SDK_INSTALL_DIR` | `C:\ti\mspm0_sdk_2_10_00_04` |
| `SYSCONFIG_ROOT` | `C:\ti\sysconfig_1.28.0` |

### 5. 修复 Flash Loader（仅使用 DAPLink 时需要）

TI 官方的 Flash loader 默认只支持 XDS 和 I-jet 调试器。使用 DAPLink 需要手动添加 CMSIS-DAP 分支：

1. 找到文件（根据你的 IAR 安装路径调整）：
   ```
   <IAR安装目录>\arm\config\flashloader\TexasInstruments\FlashMSPM0GX51X.mac
   ```
2. 在第 63 行附近，找到：
   ```c
   } else if(__driverType("ijet")) {
   ```
3. 修改为：
   ```c
   } else if(__driverType("ijet") || __driverType("cmsisdap")) {
   ```

> 如果不做此修复，烧录时会报 **"Device ID mismatch"** 错误。

## 打开与编译

1. 双击 `empty_mspm0g3519_nortos_iar.eww` 打开 IAR 工作区
2. 确认调试器驱动选择为 **CMSIS-DAP**：`Project` → `Options` → `Debugger` → `Driver`
3. 编译：`Project` → `Rebuild All`（`F7`）

## 烧录与调试

1. 连接 DAPLink，确保目标板上电
2. `Project` → `Download and Debug`（绿色三角 `Ctrl+D`）
3. 点击 `Go`（`F5`）运行程序
4. 开机动画（色彩测试 → 启动信息 → LED 闪烁 + 蜂鸣器），然后进入菜单

### 调试器配置

| 参数 | 值 |
|------|-----|
| 驱动 | CMSIS-DAP |
| 接口 | SWD |
| 速度 | 1000 kHz（不稳定时降为 100 kHz） |
| Reset | Hardware Reset（需接 RST 线） |

## 工程结构

```
empty_mspm0g3519/
├── iar/                             ← $PROJ_DIR$（工程根目录）
│   ├── empty_mspm0g3519_nortos_iar.eww  ← IAR 工作区（双击打开）
│   ├── empty_mspm0g3519_nortos_iar.ewp  ← IAR 工程文件
│   ├── empty_mspm0g3519_nortos_iar.ipcf ← SysConfig 构建规则
│   ├── empty_mspm0g3519.c           ← 主程序（开机动画 + 5项交互菜单）
│   ├── empty_mspm0g3519.syscfg      ← SysConfig 引脚配置
│   ├── mspm0g3519.icf               ← 链接脚本
│   ├── ti_msp_dl_config.c/.h        ← SysConfig 生成（勿手动编辑）
│   └── iar/
│       └── startup_mspm0g351x_iar.c ← 启动文件
├── TSP3519/                          ← 板级支持库
│   ├── tsp_common_headfile.h         ← 公共头文件枢纽
│   ├── tsp_gpio.h / tsp_gpio.c       ← GPIO 宏封装（LED/蜂鸣器/LCD/CCD/按键/编码器）
│   ├── TSP_TFT18.h / TSP_TFT18.c     ← TFT LCD 驱动（ST7735, 160×128, SPI1）
│   ├── tsp_ccd.h / tsp_ccd.c         ← TSL1401 线性 CCD 驱动（双通道）[未启用]
│   └── tsp_menu.h / tsp_menu.c       ← LCD 菜单系统（列表+子菜单+增量重绘）
└── NUEDC2025/                        ← 应用层驱动
    ├── tsp_isr.h / tsp_isr.c         ← SysTick 延时 + GROUP1/UART0 中断分发
    ├── tsp_key.h / tsp_key.c         ← 4键扫描（20ms 消抖，边沿检测）
    ├── tsp_encoder.h / tsp_encoder.c ← 编码器驱动（PHA0 中断正交解码）
    └── tsp_uart.h / tsp_uart.c       ← UART0 通信（MFCLK 4MHz, 环形缓冲 RX, printf 重定向）
```

## 已知问题

| 问题 | 现象 | 解决 |
|------|------|------|
| Flash loader Device ID 不匹配 | 使用 DAPLink 烧录失败 | 按上文步骤 5 修复 Flash loader |
| LED 不亮 | 编译烧录成功但灯不闪 | 确认调用了 `DL_GPIO_enableOutput()`，SysConfig 生成的代码会自动处理 |
| 全局变量被覆盖 | 手动编辑 `.custom_argvars` 后无效 | 只能通过 IAR GUI 设置全局变量，且必须放 Global 下 |
| 设备锁定警告 | 首次下载弹出 "Device is locked" | 点 Yes/OK 执行 Mass Erase 即可，不损坏芯片 |
| PHA0 编码器噪声 | 未接编码器时光标抖动 | 已在 `tsp_encoder_init` 中默认禁用 PHA0 中断，接编码器后手动使能 |

## 参考资源

- [TI MSPM0 SDK 文档](https://www.ti.com/tool/MSPM0-SDK)
- [TI MSPM0G3519 数据手册](https://www.ti.com/product/MSPM0G3519)
- [IAR EWARM 用户指南](https://www.iar.com/support/user-guides/)
