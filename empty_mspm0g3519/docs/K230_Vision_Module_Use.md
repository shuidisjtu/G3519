# K230 视觉识别模块对接说明（调研版）

版本：V1.0（纯调研文档，方案未实施）
适用对象：MSPM0G3519 学校开发主板 + 亚博智能 AIMOTION2 K230 视觉识别模块（豪华版）

> ⚠️ **状态说明**：本文档为对接方案调研，`.syscfg` 和代码均**尚未改动**。实施前必须按根目录 `CLAUDE.md` 的「SysConfig 先行」规则执行，并完成 §8 的待实物核对项。

## 1. 结论速览

| 项目 | 结论 |
|---|---|
| 对接方式 | UART 串口，115200-8N1，3.3V TTL，双方直连兼容 |
| G3519 侧接口 | **J11 排座 / UART6**（TX=PC11，RX=PC10），PD1 电源域，可用 BUSCLK 80MHz |
| K230 侧接口 | 亚博固件固定使用 **UART1**（FPIOA pin9=TXD、pin10=RXD），封装在 `ybUtils/YbUart.py`，默认 115200 |
| K230 开发方式 | CanMV IDE + MicroPython（亚博官方固件基于嘉楠 CanMV） |
| 供电 | K230 模块用**自身 USB 独立供电**，与主板仅共地（详见 §4.3） |
| 数据方向 | K230 → G3519：GUI 固件的识别案例**开箱即发 YbProtocol ASCII 帧**（§5.1）；G3519 → K230：原厂固件无接收处理，需自写脚本 |

## 2. K230 模块与开发环境

### 2.1 模块概况

亚博智能 AIMOTION2 K230 视觉识别模块（豪华版）基于嘉楠科技 **Kendryte K230** 芯片（RISC-V，6 TOPS 等效算力），板载 GC2093 摄像头、2.4 寸触摸屏、TF 卡槽、WiFi。固件为亚博定制的 **CanMV MicroPython** 镜像（`CanMV_K230_YAHBOOM_micropython_Vx.x.x.img`），与嘉楠官方 CanMV 文档体系兼容。

> 亚博官方建议：不要给该模块烧录第三方 K230 固件，可能出现外设不兼容。固件与例程从亚博资料页获取（见 §9）。

### 2.2 CanMV IDE 使用流程（嘉楠官方 v1.7 文档）

1. **安装**：CanMV IDE 要求 ≥ 4.0.5（亚博资料包提供 `canmv-ide-for-k230_v4.0.7.exe`），安装路径不能含中文。也可用 OpenMV IDE ≥ 4.0（仅支持 K230）。
2. **连接**：模块 USB 接 PC，IDE 左下角点「连接」。
3. **运行**：打开 `.py` 文件，点左下角「运行」；`print()` 输出显示在 IDE 串口终端。
4. **部署自启**：菜单 `工具 → 保存为 main.py`，脚本随 CanMV 开机自动运行——**联调完成后必须做这一步**，脱离 PC 后 K230 才能独立向 G3519 发数据。
5. **保存文件**：`工具 → 保存文件到 CanMV Cam`，文件落在 `/sdcard/<路径>`（板上仅 SD 卡可读写）。

## 3. K230 侧 UART API（CanMV MicroPython）

### 3.1 UART 资源

K230 片上共 5 路 UART：

| 实例 | 占用情况 |
|---|---|
| UART0 | 被小核 SH 占用（调试） |
| UART3 | 被大核 SH 占用（调试） |
| **UART1 / UART2 / UART4** | **用户可用** |

### 3.2 FPIOA 引脚映射 + UART 初始化

K230 使用 FPIOA（现场可编程 IO 阵列）自由映射引脚功能，**使用 UART 前必须先映射引脚**。本模块固件的串口封装在 `ybUtils/YbUart.py`（已存档于 `k230_firmware_ref/`），实际配置为 **UART1 @ FPIOA pin9/pin10**：

```python
from machine import UART, FPIOA

fpioa = FPIOA()
fpioa.set_function(9,  fpioa.UART1_TXD, ie=0, oe=1, pu=1)   # 载板串口 TX
fpioa.set_function(10, fpioa.UART1_RXD, ie=1, oe=0, pu=1)   # 载板串口 RX
uart = UART(UART.UART1, 115200)
```

> 自写脚本时直接 `from ybUtils.YbUart import YbUart` 复用该封装即可。社区流传的 `pin11/12 = UART2` 配置**不适用于本模块**。

### 3.3 收发 API

| 方法 | 说明 |
|---|---|
| `uart.write(buf)` | 发送字节/字符串，返回写入字节数 |
| `uart.read([n])` | 读取最多 n 字节，无数据返回 `None`，返回 `bytes` |
| `uart.readline()` | 读一行（以换行结束） |
| `uart.readinto(buf[, n])` | 读入现有缓冲区 |
| `uart.deinit()` | 释放资源 |

K230 侧发送识别结果的典型模式（原厂 GUI 固件即如此实现，见 `apps/ai_face/face_det_core/face_detection.py:77-79`）：

```python
pto_data = pto.get_face_detect_data(x, y, w, h)   # YbProtocol 组帧，见 §5.1
uart.send(pto_data)
```

## 4. 硬件接线方案（G3519 J11 ↔ K230）

### 4.1 G3519 侧 J11 针脚（UART6）

主板 J11 为 4 针排座，针脚定义**已按主板电路图实查核对（2026-07-17）**：

| J11 针脚 | 信号 | MCU GPIO | 说明 |
|---:|---|---|---|
| 1 | ~5V | - | 二极管隔离后约 5V，仅供小功率模块取电 |
| 2 | GND | - | 与 K230 共地 |
| 3 | UART6-RX | PC10（物理脚 87） | MCU 接收 |
| 4 | UART6-TX | PC11（物理脚 88） | MCU 发送 |

> ⚠️ `G3519_main_board.md` 旧版曾误记为 2=RX/3=TX/4=GND，已依据电路图勘正。若参考旧资料接线会把 GND 接到对方 TX 上。

UART6 属 **PD1 电源域**（主 UART，支持 FIFO/硬件流控），输入时钟上限 80 MHz，可直接使用 BUSCLK 80MHz——无需像 UART0 (PD0) 那样切 MFCLK。

### 4.2 交叉连接

| G3519 J11 | 方向 | K230 模块串口 |
|---|:-:|---|
| 4 脚 TX (PC11) | → | RX |
| 3 脚 RX (PC10) | ← | TX |
| 2 脚 GND | — | GND（**必须连接**） |

双方均为 3.3V TTL，**直连即可，不需要电平转换**。TX/RX 必须交叉；只通不收时优先检查是否接反（参考 `M0G3519_UART_Use.md` §7）。

### 4.3 供电方案 ⚠️

**推荐：K230 模块用自身 USB Type-C 独立供电，与主板之间只连 TX/RX/GND 三根线。**

理由：

- K230 模块满载（摄像头 + NPU 推理 + 2.4 寸屏 + WiFi）功耗远超普通传感器，J11 的 1 脚经二极管隔离、驱动能力有限，带不动时会导致模块反复重启或识别掉帧。
- 主板规则「仅 USB-C、禁止多路同时供电」针对主板自身的电源输入；K230 作为独立设备各自供电不违反该规则，但**严禁把 J11 的 5V 和模块 USB 供电同时接入模块**。

### 4.4 与拓展板共存

依据 `M0G3519_UART_Use.md` §2.3：`Car_2Motor` 拓展板 J14 未占用任何 UART 通道，J11 (PC10/PC11) 也不在拓展板信号中，插拓展板不影响本方案。

## 5. 通信协议

### 5.1 方案 A（推荐）：直接解析亚博 YbProtocol，零 K230 代码

原厂 GUI 固件的识别案例已通过 UART1 主动上报结果，协议实现在 `libs/YbProtocol.py`（已存档于 `k230_firmware_ref/`）。**ASCII 文本帧**格式：

```text
$LL,FF,xxx,yyy,www,hhh[,MSG[,VVV]]#\n
```

| 字段 | 说明 |
|---|---|
| `$` / `#` + `\n` | 帧起始 / 帧结束（`#` 后跟换行符） |
| `LL` | 2 位十进制帧长（含 `$` 和 `#`，不含 `\n`；车牌识别类型计算有 +2 偏差） |
| `FF` | 2 位功能 ID（见下表） |
| `xxx,yyy,www,hhh` | 3 位十进制目标框左上角坐标与宽高（已缩放到屏幕坐标系，前导补零） |
| `MSG` / `VVV` | 可选：识别文本（二维码内容、人名、类别名等）/ 3 位数值（置信度×100、角度等） |

示例——颜色识别到 (120,160) 处 50×40 色块：`$23,01,120,160,050,040#\n`

常用功能 ID（完整 23 项见 `k230_firmware_ref/YbProtocol.py`）：

| ID | 功能 | ID | 功能 |
|---:|---|---:|---|
| 01 | 颜色识别 | 09 | 人体检测 |
| 03 | 二维码（附内容） | 11 | 手掌检测 |
| 04 | AprilTag（附 tag_id + 角度） | 14 | 物体检测（附类别名） |
| 06 | 人脸检测 | 15 | 目标跟踪 |

G3519 侧解析建议：按 `$`…`#\n` 定界断帧（**勿依赖 LL 字段**，个别类型计算不一致），逗号分割后转整数；该协议无校验和，靠格式合法性检查（字段数、数字位数）过滤坏帧。

### 5.2 方案 B（可选）：自定义二进制协议

自写 K230 脚本（复用 `YbUart` 封装）时，可改用定长帧头 + 校验的二进制帧，减少解析开销：

```text
[0xAA] [0x55] [TYPE] [LEN] [PAYLOAD × LEN] [CHECKSUM]
```

| 字段 | 长度 | 说明 |
|---|---:|---|
| 帧头 | 2 | 固定 0xAA 0x55，用于断帧同步 |
| TYPE | 1 | 数据类型：0x01=色块坐标、0x02=标签码、0x10=命令… |
| LEN | 1 | PAYLOAD 字节数 |
| PAYLOAD | LEN | 小端多字节数据（如 int16 x/y/w/h） |
| CHECKSUM | 1 | TYPE+LEN+PAYLOAD 逐字节求和取低 8 位 |

- **K230 → G3519**：周期上报识别结果（建议 ≤50Hz，帧长 13 字节时占用波特率约 6%，裕量充足）。
- **G3519 → K230**（可选）：命令帧切换识别模式、设置阈值等。
- G3519 侧解析用状态机逐字节处理环形缓冲数据，校验失败整帧丢弃并重新找帧头。

## 6. G3519 侧后续实现路线（未实施）

严格按「SysConfig 先行」流程：

1. **`.syscfg` 新增 UART 实例**：UART6，时钟源 BUSCLK，TX=PC11 / RX=PC10，115200-8N1；参考 SDK 例程 `uart_echo_interrupts_standby` 的 `.syscfg` 写法（`C:\ti\mspm0_sdk_2_10_00_04\examples\`）。
2. **向用户确认配置** 后重新生成 `ti_msp_dl_config.c/.h`。
3. **驱动层**：现有 `NUEDC2025/tsp_uart.c` 为 UART0 单例（实例宏集中在文件顶部），不可直接复用。两个方案：
   - 新建 `tsp_uart_k230.c/.h`（复制环形缓冲结构，实例指向 UART6）——改动最小；
   - 或将 `tsp_uart` 泛化为多实例（结构体持有 `UART_Regs*` + 独立缓冲）——更干净，改动大。
4. **中断分发**：`NUEDC2025/tsp_isr.c` 新增 `UART6_IRQHandler`（UART 各有独立向量，不走 GROUP1）。
5. **协议解析层**：新建 `tsp_k230.c/.h`，实现 §5 帧格式的状态机解析，对外提供 `tsp_k230_get_target()` 之类接口。
6. **菜单集成**：`tsp_menu` 新增「K230 Test」项，LCD 实时显示收到的坐标，便于联调。
7. printf 重定向保持指向 UART0（调试口），与 K230 通道互不干扰。

## 7. 调试排查清单

联调时按顺序排查：

1. **K230 独立自检**：无需写代码——在 GUI 中打开任一识别案例（如颜色识别），用 USB-TTL 模块接 K230 载板串口 TX/GND，PC 端 115200 应能看到 `$..,..#` 格式的 YbProtocol 帧；也可在 CanMV IDE 中运行 §3.2 脚本自测。
2. **G3519 独立自检**：UART6 配好后先做 PC10↔PC11 短接回环（参考 `M0G3519_UART_Use.md` §9.2）。
3. **互连无数据**：检查交叉接线、共地、K230 脚本是否已「保存为 main.py」（脱机运行时）。
4. **数据乱码**：核对双方波特率；确认 G3519 UART6 时钟源为 BUSCLK 且 SysConfig 生成的实例频率宏（如 `UART_6_INST_FREQUENCY`，以实际实例名为准）与实际一致。
5. **丢帧/错帧**：确认 G3519 已调用 RX 使能（对应 `tsp_uart_rx_enable` 的新实例版本）；检查环形缓冲是否溢出（上报频率 × 帧长 vs 主循环消费速度）。

## 8. 核对状态

**已核对 ✅（2026-07-17，依据模块 SD 卡完整固件源码）**：

| 事项 | 结论 | 证据 |
|---|---|---|
| 串口实例与 FPIOA 引脚 | **UART1，pin9=TXD / pin10=RXD**（带上拉）；社区流传的 pin11/12=UART2 不适用本模块 | `ybUtils/YbUart.py:7-10` |
| 上报协议 | **YbProtocol ASCII 帧**（§5.1），23 种功能 ID | `libs/YbProtocol.py` |
| 识别案例是否开箱即发 | 是：GUI 主程序创建 `YbUart(baudrate=115200)` 注入各识别 app，检测循环逐帧 `uart.send()` | `ybMain/main.py:249`、`apps/ai_face/face_det_core/face_detection.py:77-79` |
| GUI「UART测试」页 | 仅为界面壳（收发是模拟实现），不代表实际驱动 | `k230_firmware_ref/uart.py:325-371` |
| J11 针脚定义 | **1=~5V、2=GND、3=RX(PC10)、4=TX(PC11)**；主板文档旧记载（2=RX/3=TX/4=GND）有误，两处文档均已勘正 | 主板电路图实查（2026-07-17） |

**仍需实物核对** ❗：

| 事项 | 说明 |
|---|---|
| 载板串口物理针脚 | UART1 (pin9/10) 对应载板上的物理接口形式（排针/PH2.0）与丝印顺序（5V/GND/RX/TX） |
| G3519 → K230 命令下发 | 原厂 GUI 固件未见 UART 接收处理逻辑（`YbUart.read()` 存在但 GUI 未调用），命令通道需自写 K230 脚本才有效 |

## 9. 参考链接

- [CanMV K230 连接 IDE（嘉楠官方 v1.7，用户指定入口）](https://www.kendryte.com/k230_canmv/zh/v1.7/userguide/how_to_use_ide.html)
- [K230 CanMV UART 模块 API 手册（v1.7 中文）](https://www.kendryte.com/k230_canmv/zh/v1.7/api/machine/K230_CanMV_UART%E6%A8%A1%E5%9D%97API%E6%89%8B%E5%86%8C.html)
- [K230 CanMV UART 例程（v1.2 英文）](https://www.kendryte.com/k230_canmv/en/v1.2/example/peripheral/uart.html)
- [亚博智能 K230 资料页（固件/IDE/例程下载）](https://www.yahboom.com/study/K230)
- 本仓库：`docs/M0G3519_UART_Use.md`（UART 资源/时钟约束/排查方法）、`docs/G3519_main_board.md` §4.1（J4/J11/J20 针脚）
- 本目录 `k230_firmware_ref/`：`YbUart.py`（串口封装，UART1@pin9/10）、`YbProtocol.py`（上报协议全量定义）、`uart.py`（GUI 测试页，仅界面壳）——均取自模块 SD 卡固件

> 本文档基于 2026-07 的嘉楠 v1.7 文档与亚博公开资料整理；UART API 在 CanMV v0.4~main 各版本间保持一致。K230 载板级信息以亚博官方原理图和实物为最终依据。
