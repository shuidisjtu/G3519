# MSPM0G3519 使用 IAR + DAPLink 验证开发板操作手册

**目标**：利用 IAR Embedded Workbench for Arm + DAPLink (CMSIS-DAP) 完成 MSPM0G3519 的连接、下载、调试及 PB5 LED 闪烁验证。

> 主控为 `MSPM0G3519SPZR`（100 引脚）；D1 接在 `PB5`，**低电平点亮**。

## 1. 所需物品

| 物品 | 要求 |
|---|---|
| 开发板 | MSPM0G3519 主板；如连接扩展板，必须断电插接且排针对齐 |
| 电脑 | Windows 10/11 |
| IAR | IAR Embedded Workbench for Arm **9.32+**（本项目使用 9.60.3） |
| SDK | TI MSPM0 SDK（本项目使用 2.10.00.04） |
| SysConfig | CCS 内嵌版本可用（本项目路径: `D:\ti\ccs2100\ccs\utils\sysconfig_1.28.0`），独立安装版也可 |
| 调试器 | DAPLink，设备管理器中应显示为 `CMSIS-DAP v2` |
| 线材 | J1 2×5 转接线或杜邦线；至少连接 SWDIO、SWCLK、GND、VTref |
| 电源 | **仅用主板 USB-C 供电** |

## 2. 接线（必须在断电时完成）

开发板 J1 为 2×5 调试口。转接线红边对准 J1 的 `RST` / 1 脚侧。

| 开发板 J1 | 网络名 | DAPLink 端 | 必须 |
|---:|---|---|---|
| 1 | `RST` | `nRESET` / `RESET` | 建议 |
| 5 | `SWCLK` | `SWCLK` | 必须 |
| 7 | `SWDIO` | `SWDIO` | 必须 |
| 6、8 或 9 | `GND` | `GND` | 必须 |
| 10 | `MCU_3V3` | `VTref` / `Vref` | 必须 |
| 2 | `UART0-RX` | — | 不需要 |
| 4 | `UART0-TX` | — | 不需要 |

### 供电规则

- J1 接线、扩展板插接均在**主板断电**时进行
- DAPLink 的 `VTref` 接 J1-10 `MCU_3V3`（仅参考电压），不接 DAPLink 的电源输出
- 主板通过 USB-C 供电；**禁止多路同时供电**
- 先给 DAPLink 上电，再给主板上电

## 3. 上电前检查

- [ ] DAPLink 未连接电脑，主板 USB-C 未上电
- [ ] J1 红边对准 `RST` / 1 脚
- [ ] 已接 `SWDIO`、`SWCLK`、`GND`、`VTref`；建议接 `RST`
- [ ] 确认无第二路电源接入开发板

## 4. 确认 DAPLink 枚举

1. USB 连接 DAPLink 到电脑
2. 设备管理器确认出现 `CMSIS-DAP v2`（不是 XDS110）
3. 给主板 USB-C 上电

## 5. 安装并确认 IAR / SDK / SysConfig

### IAR EWARM
安装路径示例: `D:\iar\ewarm-9.60.3`。在 IAR 器件选择界面搜索 `MSPM0G3519`，能找到则继续。

### MSPM0 SDK
安装路径示例: `C:\ti\mspm0_sdk_2_10_00_04`。

### SysConfig
CCS 内嵌版本路径示例: `D:\ti\ccs2100\ccs\utils\sysconfig_1.28.0\`。如无 CCS，需单独下载 [TI SysConfig Standalone](https://www.ti.com/tool/SYSCONFIG)。

### 设置 IAR 全局变量

**必须通过 IAR GUI 设置**（直接编辑 `global.custom_argvars` 文件会在 IAR 关闭时被覆盖）：

> **Tools → Configure Custom Argument Variables → Global → New Group → New Variable**

| 变量 | 值 |
|---|---|
| `MSPM0_SDK_INSTALL_DIR` | `C:\ti\mspm0_sdk_2_10_00_04` |
| `SYSCONFIG_ROOT` | `D:\ti\ccs2100\ccs\utils\sysconfig_1.28.0` |

设置后**关闭并重新打开 IAR** 生效。

## 6. 建立验证工程

### 6.1 复制 SDK 示例

将 SDK 示例 `C:\ti\mspm0_sdk_2_10_00_04\examples\nortos\CUSTOM_BOARD\driverlib\empty_mspm0g3519\` 复制到工作目录。

### 6.2 重组工程文件结构

SDK 示例将 `.ewp`/`.eww`/`.ipcf` 放在 `iar/` 子目录中，导致 `$PROJ_DIR$` 指向 `iar/` 而非工程根目录，所有路径错位。

**将以下文件从 `iar/` 移到工程根目录**：
- `empty_mspm0g3519_nortos_iar.ewp`
- `empty_mspm0g3519_nortos_iar.eww`
- `empty_mspm0g3519_nortos_iar.ewd`
- `empty_mspm0g3519_nortos_iar.ipcf`
- `mspm0g3519.icf`

并修正 `.ewp` 中的路径引用：
- 启动文件: `$PROJ_DIR$\iar\startup_mspm0g351x_iar.c`
- 源文件: `$PROJ_DIR$\empty_mspm0g3519.c`
- SysConfig: `$PROJ_DIR$\empty_mspm0g3519.syscfg`

最终结构参考源码仓库 `empty_mspm0g3519/`。

### 6.3 配置 CMSIS-DAP 调试器

`Project → Options`：

| 位置 | 设置 |
|---|---|
| `General Options → Target` | `MSPM0G3519` |
| `Debugger → Setup` | Driver = `CMSIS-DAP` |
| `Debugger → CMSIS-DAP → Interface` | Interface = `SWD`，速度 = `1000 kHz` |
| `Debugger → CMSIS-DAP → Setup` | Reset = Hardware（已接 RST 时） |

不稳定时降到 `100 kHz`。

### 6.4 Flash loader CMSIS-DAP 修复

TI 的 `FlashMSPM0GX51X.mac` 只处理了 XDS 和 I-Jet 调试器，缺失 CMSIS-DAP 分支，导致启动时误报 "Device ID doesn't match"。修复文件：

`D:\iar\ewarm-9.60.3\arm\config\flashloader\TexasInstruments\FlashMSPM0GX51X.mac`

将第 63 行：
```c
} else if(__driverType("ijet")) {
```
改为：
```c
} else if(__driverType("ijet") || __driverType("cmsisdap")) {
```

> 本项目已完成此修复。重装 IAR 后需重新修改。

## 7. 编写 PB5 LED 闪烁代码

### 引脚映射

| 物理引脚 | IOMUX 索引 | GPIO 宏 |
|---|---|---|
| PB5 (Pin 26) | `IOMUX_PINCM18` | `GPIOB`, `DL_GPIO_PIN_5` |

> 注意：物理引脚编号（26）≠ IOMUX 索引（18）。IOMUX_PINCM26 对应的实际是 GPIOB_DIO9，不是 PB5。

### main.c

```c
#include "ti_msp_dl_config.h"

int main(void)
{
    SYSCFG_DL_init();

    /* PB5 as output, initial high (LED off, active low) */
    DL_GPIO_initDigitalOutput(IOMUX_PINCM18);   // 设引脚功能为 GPIO
    DL_GPIO_enableOutput(GPIOB, DL_GPIO_PIN_5); // 开输出使能（容易漏！）
    DL_GPIO_setPins(GPIOB, DL_GPIO_PIN_5);      // 初始高电平

    while (1) {
        delay_cycles(8000000);                   // ~250ms @ 32MHz
        DL_GPIO_togglePins(GPIOB, DL_GPIO_PIN_5);
    }
}
```

> 关键踩坑：`DL_GPIO_initDigitalOutput` 只配引脚功能为 GPIO，不设方向。必须额外调用 `DL_GPIO_enableOutput`。

## 8. 编译、下载和验证

1. `Project → Rebuild All`（F7），确保无 error（2 个来自 SDK 的 Warning 可忽略）
2. `Project → Download and Debug`
3. 如弹出 "Device ID doesn't match" 警告 → 检查步骤 6.4
4. 停在 `main()` 后，按 `F5`（或 ▶）运行
5. 观察 D1 闪烁（低电平亮，高电平灭）
6. 停止调试，主板重新上电 → D1 仍闪烁 → 确认程序已写入 Flash

## 9. 验证判定

| 现象 | 结论 |
|---|---|
| 能下载并停在 `main()` | SWD 通信、供电、Flash 下载正常 |
| 断点可命中 | CPU 执行正常 |
| D1 持续闪烁 | PB5 GPIO 和 LED 电路正常 |
| 重新上电后仍闪烁 | Flash 编程与启动流程正常 |

## 10. 常见故障

| 现象 | 排查 |
|---|---|
| 变量展开失败 (`$MSPM0_SDK_INSTALL_DIR$`) | 通过 IAR GUI 设置全局变量（不要直接编辑文件），重启 IAR |
| "Device ID doesn't match" | 检查步骤 6.4 MAC 文件是否已添加 `cmsisdap` 分支 |
| 下载成功但 LED 不亮 | 确认调用了 `DL_GPIO_enableOutput`；确认 IOMUX 用的是 PINCM18 不是 PINCM26 |
| 无法连接目标 | 降速到 `100 kHz`；检查 RST 线是否接好；主板是否上电 |
| 找不到 `MSPM0G3519` 器件 | 升级 IAR 至 9.32+ 或确认安装的是 Arm 版本 |

## 参考

- [MSPM0 SDK User Guide](https://software-dl.ti.com/msp430/esd/MSPM0-SDK/2_10_00_04/docs/english/sdk_users_guide/doc_guide/doc_guide-srcs/sdk_users_guide.html)
- [IAR C-SPY Debugging Guide for Arm](https://wwwfiles.iar.com/arm/webic/doc/EWARM_DebuggingGuide.ENU.pdf)
- 本项目 CLAUDE.md — 环境路径、工程结构、踩坑记录
