# MSPM0G3519 LED Blink 示例工程

基于 TI **MSPM0G3519SPZR**（Arm Cortex-M0+）的 LED 闪烁入门工程，使用 IAR EWARM + DAPLink (CMSIS-DAP) 开发工具链。

## 硬件连接

| 项目 | 说明 |
|---|---|
| **主控** | TI MSPM0G3519SPZR（100 引脚 LQFP） |
| **LED** | D1 接 **PB5**（物理引脚 26），**低电平点亮** |
| **调试器** | DAPLink (CMSIS-DAP v2, VID_0D28&PID_0204) |
| **供电** | 主板既可通过 USB-C 供电，也可通过转接的调试器接口供电，但禁止多路同时供电 |

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
3. 在 `TI` 组中添加以下变量：

| 变量名 | 值（根据你的实际路径调整） |
|--------|---------------------------|
| `MSPM0_SDK_INSTALL_DIR` | `C:\ti\mspm0_sdk_2_10_00_04` |
| `SYSCONFIG_ROOT` | `D:\ti\ccs2100\ccs\utils\sysconfig_1.28.0` |

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
4. 观察 D1 (PB5) LED 以约 2Hz 频率闪烁

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
├── empty_mspm0g3519_nortos_iar.eww  ← IAR 工作区（双击打开）
├── empty_mspm0g3519_nortos_iar.ewp  ← IAR 工程文件
├── empty_mspm0g3519_nortos_iar.ipcf ← SysConfig 构建规则
├── empty_mspm0g3519.c               ← 主程序（LED 闪烁）
├── empty_mspm0g3519.syscfg          ← SysConfig 引脚配置
├── mspm0g3519.icf                   ← 链接脚本
├── ti_msp_dl_config.c/.h            ← SysConfig 生成的驱动配置（勿手动编辑）
└── iar/
    └── startup_mspm0g351x_iar.c     ← 启动文件
```

## 程序逻辑

```c
int main(void)
{
    SYSCFG_DL_init();                          // SysConfig 自动生成的初始化
    DL_GPIO_initDigitalOutput(IOMUX_PINCM18);  // PB5 设为 GPIO 功能
    DL_GPIO_enableOutput(GPIOB, DL_GPIO_PIN_5);// 开启输出使能（容易漏！）
    DL_GPIO_setPins(GPIOB, DL_GPIO_PIN_5);     // 初始高电平（LED 灭，低电平有效）

    while (1) {
        delay_cycles(8000000);                 // ~250ms @ 32MHz
        DL_GPIO_togglePins(GPIOB, DL_GPIO_PIN_5);
    }
}
```

## 已知问题

| 问题 | 现象 | 解决 |
|------|------|------|
| Flash loader Device ID 不匹配 | 使用 DAPLink 烧录失败 | 按上文步骤 5 修复 Flash loader |
| LED 不亮 | 编译烧录成功但灯不闪 | 确认调用了 `DL_GPIO_enableOutput()`，`DL_GPIO_initDigitalOutput` 只配功能不设方向 |
| 全局变量被覆盖 | 手动编辑文件后无效 | 只能通过 IAR GUI 设置全局变量，IAR 关闭时会覆盖文件 |
| Stop Debugging 后 LED 不闪 | 退出调试后芯片停止运行 | 正常现象。重新上电即恢复（程序已烧入 Flash） |

## 参考资源

- [TI MSPM0 SDK 文档](https://www.ti.com/tool/MSPM0-SDK)
- [TI MSPM0G3519 数据手册](https://www.ti.com/product/MSPM0G3519)
- [IAR EWARM 用户指南](https://www.iar.com/support/user-guides/)
