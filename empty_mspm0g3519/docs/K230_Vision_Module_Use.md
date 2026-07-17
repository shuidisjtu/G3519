# K230 视觉模块对接指南

版本：V1.3。**链路已实测打通**（2026-07-17：`k230_link_test.py` 模拟帧 20Hz，K230 Test 页面 Frm 递增、Err=0）。G3519 侧驱动/协议解析/菜单已实装（见 §4）。

## 1. 结论速览

| 项目 | 结论 |
|---|---|
| 对接方式 | UART，115200-8N1，3.3V TTL 直连 |
| G3519 侧 | **J11 排座 / UART6**（TX=PC11，RX=PC10），PD1 电源域，时钟用 BUSCLK 80MHz |
| K230 侧 | 亚博固件固定 **UART1**（FPIOA pin9=TXD、pin10=RXD），封装见 `k230_firmware_ref/YbUart.py` |
| 数据方向 | K230 → G3519：GUI 识别案例**开箱即发 YbProtocol ASCII 帧**（§3.1）；G3519 → K230：原厂固件无接收处理，需自写脚本 |
| 供电 | 两种方式均可且**可同时接**（J11 5V 经肖特基隔离，USB 端电压略高时二极管截止防倒灌）：① 联调时 K230 接 USB（IDE 通信必须）；② 脱机运行时可由 J11 1 脚 5V 直接供电（略低于 5V，可支撑 K230 正常工作，老师确认的设计意图） |

## 2. 硬件接线（已完成，供核对）

J11 针脚定义已按主板电路图实查核对（2026-07-17）：

| J11 针脚 | 信号 | MCU GPIO | 接 K230 |
|---:|---|---|---|
| 1 | ~5V（肖特基隔离） | - | K230 5V（可选：脱机时由主板供电；与 K230 USB 同接安全） |
| 2 | GND | - | GND |
| 3 | UART6-RX | PC10（物理脚 87） | ← K230 TX |
| 4 | UART6-TX | PC11（物理脚 88） | → K230 RX |

> ⚠️ `G3519_main_board.md` 旧版曾误记为 2=RX/3=TX/4=GND，已勘正；勿按旧资料重新接线。
> 拓展板 J14 不占用 PC10/PC11，插拓展板不影响本方案。

## 3. 通信协议

### 3.1 方案 A（推荐）：直接解析亚博 YbProtocol，零 K230 代码

原厂 GUI 固件的识别案例通过 UART1 主动上报，协议定义见 `k230_firmware_ref/YbProtocol.py`。**ASCII 文本帧**：

```text
$LL,FF,xxx,yyy,www,hhh[,MSG[,VVV]]#\n
```

| 字段 | 说明 |
|---|---|
| `$` / `#` + `\n` | 帧起始 / 帧结束（`#` 后跟换行符） |
| `LL` | 2 位十进制帧长（含 `$` 和 `#`，不含 `\n`；车牌识别类型有 +2 偏差） |
| `FF` | 2 位功能 ID（见下表） |
| `xxx,yyy,www,hhh` | 3 位十进制目标框左上角坐标与宽高（屏幕坐标系，前导补零） |
| `MSG` / `VVV` | 可选：识别文本（二维码内容、类别名等）/ 3 位数值（置信度×100、角度等） |

示例——颜色识别到 (120,160) 处 50×40 色块：`$23,01,120,160,050,040#\n`

常用功能 ID（完整 23 项见 `k230_firmware_ref/YbProtocol.py`）：

| ID | 功能 | ID | 功能 |
|---:|---|---:|---|
| 01 | 颜色识别 | 09 | 人体检测 |
| 03 | 二维码（附内容） | 11 | 手掌检测 |
| 04 | AprilTag（附 tag_id + 角度） | 14 | 物体检测（附类别名） |
| 06 | 人脸检测 | 15 | 目标跟踪 |

**G3519 侧解析要点**：按 `$`…`#\n` 定界断帧（**勿依赖 LL 字段**，个别类型计算不一致），逗号分割后转整数；无校验和，靠格式合法性检查（字段数、数字位数）过滤坏帧；状态机逐字节消费环形缓冲，坏帧整帧丢弃重新找帧头。

### 3.2 方案 B（可选）：自定义二进制协议

自写 K230 脚本（复用 `YbUart` 封装）时可改用二进制帧减少解析开销：

```text
[0xAA] [0x55] [TYPE] [LEN] [PAYLOAD × LEN] [CHECKSUM]
```

CHECKSUM = TYPE+LEN+PAYLOAD 逐字节求和取低 8 位；PAYLOAD 小端（如 int16 x/y/w/h）。115200 下 13 字节帧 @50Hz 仅占用波特率 ~6%。

## 4. G3519 侧实现（已完成）

| 层 | 文件 | 说明 |
|---|---|---|
| SysConfig | `iar/empty_mspm0g3519.syscfg` | `UART_K230` 实例：UART6、BUSCLK、PC11(TX)/PC10(RX)、115200-8N1（IBRD=43/FBRD=26，偏差 0.008%） |
| 驱动 | `NUEDC2025/tsp_uart_k230.c/.h` | 256B 环形缓冲 RX；`rx_enable/disable` 按需开关（同 UART0 策略） |
| 中断 | `NUEDC2025/tsp_isr.c` | `UART6_IRQHandler` → 只入队，不解析 |
| 协议 | `NUEDC2025/tsp_k230.c/.h` | `$...#` 状态机断帧（主循环 `tsp_k230_task()`），`tsp_k230_get_target()` 查询最新目标，帧/错帧计数 |
| 菜单 | `iar/empty_mspm0g3519.c` | 「K230 Test」页：实时显示 ID/X/Y/W/H/MSG/Frm/Err，PUSH 退出自动关 RX |

架构原则：**ISR 只入环形缓冲，解析在主循环**；printf 仍走 UART0 调试口，与 K230 通道互不干扰。API 用法见项目 `CLAUDE.md` 速查。

> 本地 `01_k230_color_detect/`（亚博 G3507 CCS 例程，已 .gitignore）仅作协议参考，其 ISR 内解析 + printf 的写法勿模仿；G3507 syscfg/引脚不适用本板。

## 5. K230 侧要点

- **链路测试脚本**：`k230_scripts/k230_link_test.py`（自包含，不依赖亚博库）。两种跑法：① CanMV IDE 临时「运行」（需接 USB，改脚本方便）；② `工具 → 保存为 main.py` 后拔 USB，J11 四线（5V/GND/RX/TX）供电脱机自启。
- **方案 A（用原厂识别功能）的前提是亚博 GUI 固件在位**：GUI 打开识别案例即自动发数据。当前模块的 `main.py` 已被覆盖，需要时恢复原厂 `main.py`（本地 H:\ 有 SD 卡备份）或重烧亚博固件。
- 自写脚本：亚博固件下可 `from ybUtils.YbUart import YbUart` 复用封装（UART1@pin9/10 已配好）；改完须「保存为 main.py」才能脱机自启。
- 勿烧录第三方 K230 固件（亚博官方提示外设可能不兼容）。

## 6. 调试排查清单

1. **K230 独立自检**：GUI 打开任一识别案例，USB-TTL 接 K230 载板串口 TX/GND，PC 端 115200 应看到 `$..,..#` 帧；若 GUI 已被覆盖/刷机，用 `k230_scripts/k230_link_test.py`（CanMV IDE 临时运行，模拟颜色帧，不依赖亚博库）。
2. **G3519 独立自检**：UART6 配好后先做 PC10↔PC11 短接回环（参考 `M0G3519_UART_Use.md` §9.2）。
3. **互连无数据**：检查交叉接线、共地、K230 脚本是否已「保存为 main.py」（脱机运行时）。
4. **数据乱码**：核对双方波特率；确认 UART6 时钟源为 BUSCLK 且 SysConfig 生成的实例频率宏与实际一致。
5. **丢帧/错帧**：确认已使能 RX 中断；检查环形缓冲是否溢出（上报频率 × 帧长 vs 主循环消费速度）。

## 7. 参考

- [K230 CanMV UART API 手册（v1.7 中文）](https://www.kendryte.com/k230_canmv/zh/v1.7/api/machine/K230_CanMV_UART%E6%A8%A1%E5%9D%97API%E6%89%8B%E5%86%8C.html)
- [CanMV K230 连接 IDE（v1.7）](https://www.kendryte.com/k230_canmv/zh/v1.7/userguide/how_to_use_ide.html)
- [亚博 K230 资料页](https://www.yahboom.com/study/K230)
- 本仓库：`docs/M0G3519_UART_Use.md`、`docs/G3519_main_board.md` §4.1
- 本目录 `k230_firmware_ref/`：`YbUart.py`（串口封装）、`YbProtocol.py`（协议全量定义）、`uart.py`（GUI 测试页，仅界面壳）——取自模块 SD 卡固件
