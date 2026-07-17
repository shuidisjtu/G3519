# K230 视觉模块对接指南

版本：V1.2（轻量化）。接线与环境已就绪，G3519 侧软件实施须按「SysConfig 先行」规则（见 §4）。

## 1. 结论速览

| 项目 | 结论 |
|---|---|
| 对接方式 | UART，115200-8N1，3.3V TTL 直连 |
| G3519 侧 | **J11 排座 / UART6**（TX=PC11，RX=PC10），PD1 电源域，时钟用 BUSCLK 80MHz |
| K230 侧 | 亚博固件固定 **UART1**（FPIOA pin9=TXD、pin10=RXD），封装见 `k230_firmware_ref/YbUart.py` |
| 数据方向 | K230 → G3519：GUI 识别案例**开箱即发 YbProtocol ASCII 帧**（§3.1）；G3519 → K230：原厂固件无接收处理，需自写脚本 |
| 供电 | K230 用自身 USB 独立供电，与主板**只连 TX/RX/GND 三根线**；严禁 J11 的 5V 与模块 USB 同时供电 |

## 2. 硬件接线（已完成，供核对）

J11 针脚定义已按主板电路图实查核对（2026-07-17）：

| J11 针脚 | 信号 | MCU GPIO | 接 K230 |
|---:|---|---|---|
| 1 | ~5V（二极管隔离） | - | 不接 |
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

## 4. G3519 侧实现路线

严格按「SysConfig 先行」流程：

1. **`.syscfg` 新增 UART 实例**：UART6，时钟源 BUSCLK，TX=PC11 / RX=PC10，115200-8N1；参考 SDK 例程 `uart_echo_interrupts_standby` 的 `.syscfg` 写法（`C:\ti\mspm0_sdk_2_10_00_04\examples\`）。
2. **向用户确认配置** 后重新生成 `ti_msp_dl_config.c/.h`。
3. **驱动层**：现有 `NUEDC2025/tsp_uart.c` 为 UART0 单例（实例宏集中在文件顶部），不可直接复用。两个方案：
   - 新建 `tsp_uart_k230.c/.h`（复制环形缓冲结构，实例指向 UART6）——改动最小；
   - 或将 `tsp_uart` 泛化为多实例（结构体持有 `UART_Regs*` + 独立缓冲）——更干净，改动大。
4. **中断分发**：`NUEDC2025/tsp_isr.c` 新增 `UART6_IRQHandler`（UART 各有独立向量，不走 GROUP1）。
5. **协议解析层**：新建 `tsp_k230.c/.h`，实现 §3.1 帧格式的状态机解析，对外提供 `tsp_k230_get_target()` 之类接口。
6. **菜单集成**：`tsp_menu` 新增「K230 Test」项，LCD 实时显示收到的坐标，便于联调。
7. printf 重定向保持指向 UART0（调试口），与 K230 通道互不干扰。

### 4.1 亚博官方 MCU 例程参考（本地 `01_k230_color_detect/`，不入库）

亚博为 MSPM0**G3507** 提供的 CCS 例程，存放于 `empty_mspm0g3519/01_k230_color_detect/`（已 `.gitignore`）。

**可移植**：`APP/yb_protocol.c/.h` 的解析状态机（帧头 `0x24`/帧尾 `0x23` 断帧 → 逗号分割 `atoi` → LL 校验 → 功能 ID 过滤），可改造为 `tsp_k230.c`；其 `pto_len == num` 校验实证 LL 可用（`#` 即成帧，`\n` 不入缓冲）。

**不可照抄**：

| 项目 | 例程 (G3507) | 本项目 (G3519) |
|---|---|---|
| `.syscfg` | SDK 2.02、LQFP-64 | SDK 2.10、LQFP-100，必须自建 |
| K230 串口 | UART2 @ PA21/PA22（**G3519 没有 UART2**） | UART6 @ PC10/PC11 (J11) |
| 工程/IDE | CCS | IAR EWARM |
| printf 重定向 | Keil 风格 `fputc` | IAR `__write`（`tsp_uart.c` 已有） |

**⚠️ 例程坏实践，移植时必须修正**：`usart.c:146-166` 在 RX 中断里直接整帧解析 + `sprintf` + 阻塞回显。本项目必须保持「**ISR 只入环形缓冲，主循环消费解析**」架构（同 `tsp_uart.c`），解析结果存最新目标供查询，不在中断里 printf。

## 5. K230 侧要点

- 方案 A 无需写任何 K230 代码，GUI 打开识别案例即发数据。
- 自写脚本时：`from ybUtils.YbUart import YbUart` 复用固件封装（UART1@pin9/10 已配好）；写完在 CanMV IDE 中 `工具 → 保存为 main.py`，脚本随开机自启，**脱离 PC 前必须做这一步**。
- 勿烧录第三方 K230 固件（亚博官方提示外设可能不兼容）。

## 6. 调试排查清单

1. **K230 独立自检**：GUI 打开任一识别案例，USB-TTL 接 K230 载板串口 TX/GND，PC 端 115200 应看到 `$..,..#` 帧。
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
