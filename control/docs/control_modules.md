# 控制题（小车题）模块实现进度

> 最后更新：2026-07-26
> 覆盖场景：循迹、色块追踪、电机驱动、速度闭环、视觉导航等

## 进度总览

| 模块 | 代码 | 硬件验证 | 状态 |
|------|------|---------|------|
| DRV8874 电机驱动 | 完整（双PWM 4通道，SysConfig） | 已验证：两轮同步起转、方向正确 | 可用 |
| K230 视觉模块 | 完整 | 双向通信已验证 | 可用 |
| CCD 线阵（循迹） | 完整 | DC注入已验证 | 可用 |
| 旋钮编码器 | 完整 | 在用（菜单/参数微调） | 可用 |
| 轮子编码器(QEI) | 完整（TIMG8/TIMG9 硬件QEI） | 已验证：双轮脉冲/速度正常 | 待标定 |
| MPU6050 陀螺仪/IMU | 完整 | 已验证 | 可用 |
| PID 控制器 | 完整 | 闭环菜单可用 | 可用（已重调增益） |
| CCD 循迹算法 | 完整 | 待实物验证 | 增益待重调（反馈量级变化） |
| 里程计 | 完整 | 轮子编码器已接入 | COUNTS_PER_CM 待标定 |

## 各模块详情

### DRV8874 电机驱动

- **代码文件**：`NUEDC2025/tsp_motor.c/.h`
- **应用入口**：`action_motor_openloop()` — 双电机独立控制、正反转、占空比调节；`action_motor_closeloop()` — PID 闭环速度控制
- **硬件配置**（SysConfig 实例 `MOTOR_PWM`）：
  - TIMA0 PWM 20kHz (80MHz BUSCLK, period=3999, LOAD=3998)
  - 4 通道双 PWM 架构（无 GPIO 方向引脚）：
    - M1(右轮): PB3(CCP0=IN2) + PB4(CCP1=IN1)
    - M2(左轮): PB0(CCP2=IN2) + PB2(CCP3=IN1)
  - nSLEEP: PB1，nFAULT: PA7 (10kΩ 上拉)
  - 死区补偿: MOTOR_DEAD_ZONE=50, MOTOR_DC_LIMIT=99
- **验证状态**（2026-07-26）：双轮同步起转、方向正确、开环速度基本对称
- **使用注意**：调用 `tsp_motor_set()` 前必须先 `SLEEP_HIGH()` 使能 H 桥
- **参考文档**：`common/docs/DRV8874_Motor_Use.md`
- **参考工程**：[HSPv2](https://github.com/yyx1248722477/hsp.git) — 同底盘/扩展板

#### 双 PWM 迁移（2026-07-26）

原 2 通道（PB3/PB0 PWM + PB4/PB2 GPIO）改为 4 通道全 PWM。根因：GPIO 方向引脚导致正/反向衰减模式不对称（fast vs slow decay），两轮起转阈值差距 5% vs 55%。参照 HSPv2 参考工程采用双 PWM 架构。

### K230 视觉模块

- **代码文件**：`NUEDC2025/tsp_uart_k230.c/.h`（UART6 驱动）、`tsp_k230.c/.h`（YbProtocol 解析）
- **应用入口**：`action_k230_test()` — 色块追踪画框 + 双向通信
- **已验证**：
  - UART6 双向通信（G3519 → K230 TX, K230 → G3519 RX）
  - YbProtocol 帧解析（`$...#` 断帧，func_id/x/y/w/h/msg 提取）
  - 色块识别坐标映射（K230 640×480 → LCD 160×80）
  - 指令协议（`$SWITCH#` 切换识别模式）
- **坐标映射**：`lcd_x = tgt.x/4, lcd_y = 16 + tgt.y/6`
- **参考文档**：`common/docs/K230_Vision_Module_Use.md`
- **K230 固件**：`k230_firmware_ref/` 目录（亚博 SD 卡固件归档）

### CCD 线阵（循迹）

- **代码文件**：`TSP3519/tsp_ccd.c/.h`
- **应用入口**：`action_ccd_test()` — 原始波形查看器（128 像素采集 + LCD 显示）
- **4 通道 2 组**：
  - Group1: CCD1(PB18,CH5) + CCD2(PB19,CH6)，共用 SI1(PC9)/CLK1(PB20)
  - Group2: CCD3(PB17,CH4) + CCD4(PA17,CH2)，共用 SI2(PC4)/CLK2(PC5)
- **ADC**: ADC1 via SysConfig（实例 CCD_ADC），序列模式 MEM0-3→CH5/6/4/2
- **循迹相关**：当前代码仅做原始波形展示（Max/Min/Avg），**未实现**循迹算法（二值化、中线提取、偏差计算）。这些算法需根据赛题在应用层实现
- **已修复 bug（2026-07-25）**：
  - 原代码手动初始化用了 ADC1 实例名但实际应通过 SysConfig 配置
  - ENC 位未在每次转换前重新使能（硬件自动清除）
  - CCD2 引脚映射错误（原 PB17/CH4，实为 PB19/CH6）
  - freqRange 配置不匹配（24-32MHz vs 实际 4MHz ADC 时钟）
- **AD2 验证（2026-07-25）**：DC 1.65V 注入 CCD1(PB18)，Avg=2266（理论 2048，偏差 ~10%，正常）
- **LCD 波形显示**：采用批量清空+连线绘制（`set_region` 流式写入清屏，再画相邻列垂直线段）。动态波形（AD2 正弦/三角）显示效果一般，但实际 CCD 空间信号（黑白线条）相邻像素平滑，预计效果良好
- **引脚互斥**：与 AD9833 DDS 共用 PC2/PC3，小车题中一般不需要 DDS，无冲突
- **AD2 测试方案**：`common/docs/CCD_AD2_Test.md`

### 旋钮编码器（参数微调）

- **代码文件**：`NUEDC2025/tsp_encoder.c/.h`
- **硬件**：PHA0(PA14) GPIO 中断正交解码 + PHB0(PA15) 方向判别
- **用途**：Motor OpenLoop 中切换 M1/M2、Motor ClsLoop 中微调目标速度 ±1
- **注意**：这是板上的旋钮编码器（J5 pin37/38），**不是**轮子编码器

### 轮子编码器（QEI 硬件正交解码）

- **代码文件**：`NUEDC2025/tsp_wheel_enc.c/.h`
- **硬件配置**（SysConfig QEI 模块）：
  - 右轮(J12): TIMG9, PB7(CCP0=PHB1) + PB9(CCP1=PHA1), 实例 `WHEEL_ENC_R`
  - 左轮(J13): TIMG8, PB15(CCP0=PHB2) + PB16(CCP1=PHA2), 实例 `WHEEL_ENC_L`
  - 5V 编码器 → SN74LVC2T45 电平转换 → 3.3V MCU
  - BUSCLK/ULPCLK, LOAD=0xFFFF, 计数器居中 0x8000
- **功能**：双轮独立脉冲计数和速度测量（20ms 间隔，SysTick 驱动）
- **已接入场景**：Motor ClsLoop（速度反馈）、Line Follow（速度反馈）、Odometer（距离/角度）
- **验证状态**（2026-07-26）：手转轮子有脉冲响应，双轮速度读数正常
- **待完成**：
  - `ODOM_COUNTS_PER_CM` 标定（推车 100cm 记录脉冲数）
  - `ODOM_COUNTS_PER_DEGREE` 标定
  - LineFollow PD 增益重调

### MPU6050 陀螺仪/IMU（盲走航向）

- **代码文件**：`NUEDC2025/tsp_mpu6050.c/.h`
- **应用入口**：`action_mpu6050_test()` — 六轴原始数据显示 + Yaw 角度页面
- **芯片**：MPU6050（U3），六轴 IMU（三轴陀螺仪 + 三轴加速度计）
- **总线**：I2C0 via SysConfig（实例 IMU_I2C），400kHz，SCL=PB21(pin83), SDA=PB22(pin84)，上拉 R1/R2=2.2kΩ
- **I2C 地址**：0x68（AD0 经 10K 下拉接 GND）
- **中断**：IMU_INT = PC8(pin80)，当前未使用（轮询模式）
- **配置**：采样率 125Hz, DLPF 44Hz, 陀螺仪 ±500°/s, 加速度计 ±4g
- **Yaw 积分**：`tsp_mpu6050_update_yaw()` 自节拍 10ms 读取 gyro Z 并积分
- **小车用途**：读取 Z 轴角速度积分得到航向角（Yaw），实现盲走（定角度转弯、直线修正）
- **已验证（2026-07-25）**：
  - WHO_AM_I = 0x68 通信正常
  - 六轴原始数据：静置 AZ≈7749-7784（接近 1g=8192），GZ≈-2（优秀零飘）
  - Gyro Z 校准偏移正常工作（200 采样平均 + 死区滤波）
  - Yaw 积分显示正常（整数度，无字段重叠）
  - I2C 总线空闲保护后错误计数稳定为 0
- **参考**：`common/docs/G3519_main_board.md` §8.1

## 引脚互斥关系 (小车题相关)

| 冲突组 | 引脚 | 涉及模块 | 说明 |
|--------|------|---------|------|
| ~~PB9/PB7~~ | ~~SPI1 vs TIMG9~~ | ~~LCD ↔ 编码器1~~ | **已解决**：LCD SPI1 实际用 PB30/PB31/PB14，不占 PB7/PB9。两路 QEI 均可同时使用 |

> 小车题中 AD9833 DDS 一般不需要，因此 CCD 与 DDS 的 PC2/PC3 冲突不影响。

## 小车题典型功能 vs 当前覆盖

| 功能 | 所需模块 | 覆盖 |
|------|---------|------|
| 电机驱动 | DRV8874 | ✓ 代码完整 |
| 视觉导航/色块追踪 | K230 | ✓ 已验证 |
| CCD 循迹 | CCD + 循迹算法 | ✓ 已实现（tsp_linefollow.c） |
| 速度闭环 | 轮子编码器(QEI) + PID | ✓ 已验证（增益已重调） |
| 盲走/航向控制 | MPU6050 + Yaw 积分 | ✓ 已验证 |
| 里程测量 | 轮子编码器(QEI) + 里程计 | ✓ 已实现（待标定 COUNTS_PER_CM） |
| PID 参数调整 | 编码器旋钮 + LCD | ✓ 已实现（Speed Setting 菜单） |

## 新增模块详情

### PID 控制器

- **代码文件**：`NUEDC2025/tsp_pid.c/.h`
- **两种形式**：
  - **位置式 PID**：`tsp_pid_pos_step()` — 用于循迹转向 PD、速度 PD（Ki=0 时退化为 PD）
  - **增量式 PID (D-on-PV)**：`tsp_pid_inc_step()` — 用于电机闭环速度控制，防止设定值突变时的微分冲击
- **应用入口**：`action_motor_closeloop()` — 增量式 PID 闭环速度控制演示
- **特性**：输出钳位、积分抗饱和、D-on-PV 防微分冲击
- **移植来源**：HSP (HuashanPi) Ex6_pwm.c GD32F4xx 平台

### CCD 循迹算法

- **代码文件**：`NUEDC2025/tsp_linefollow.c/.h`
- **应用入口**：`action_linefollow()` → `tsp_linefollow_demo()` — 交互式循迹（LCD 实时显示 + S2 启停）
- **算法流程**：
  1. CCD 128px 采集 → 扫描黑线左右边沿（亮暗阈值 + 宽度验证）
  2. 中线误差 → PD 转向控制（IIR 平滑）
  3. 编码器速度反馈 → PD 速度控制（帧间速率限制）
  4. 差速驱动：`left_dc = dc + steer`, `right_dc = dc - steer`
- **丢线保护**：≤3 帧保持上次位置、>3 帧渐减速度（线性衰减）、>13 帧完全停车
- **参数调整**：`action_speed_setting()` 菜单（编码器旋钮微调 Kp/Kd/基础速度）
- **移植来源**：HSP Project1_LineFollower.c

### 里程计

- **代码文件**：`NUEDC2025/tsp_odometer.c/.h`
- **应用入口**：`action_odometer()` → `tsp_odometer_demo()` — 交互式里程计
- **模式**：
  - STRAIGHT：直线距离（cm），`pulse / COUNTS_PER_CM`
  - ROTATE：旋转角度（deg），`pulse / COUNTS_PER_DEGREE`
- **功能**：编码器脉冲积分、LCD 增量刷新（距离/角度/原始脉冲/速度）
- **操作**：S0=直线模式、S1=旋转模式、S2=清零、PUSH=退出
- **移植来源**：HSP Odometer.c GD32F4xx 平台

## 代码质量审查记录（2026-07-25）

两轮全量代码审查，共修复 56 项问题（6 CRITICAL + 10 HIGH + 16 MEDIUM + 24 LOW）：

**关键修复**：
- PID 微分/增量索引错误（CRITICAL）
- UART 环形缓冲 volatile 竞态（HIGH）— `available()`/`read_byte()` 对 volatile 变量做单次快照
- I2C 无 NACK 检测 + init 忽略 write_reg 返回值（HIGH）
- INT_MIN 取反 UB（HIGH）— 用 unsigned 中间变量安全取反
- sprintf 缓冲区溢出（MEDIUM）— 全部替换为 snprintf
- 按键 debounce 边沿丢失（MEDIUM）— 仅在状态实际变化时更新 state_prev
- 编码器 disable/enable 速度尖峰（MEDIUM）— static 局部变量移至文件作用域
- 循迹 g_lost_frames uint8 回绕（MEDIUM）— 饱和递增
- LCD draw_frame 右下角缺失（MEDIUM）— 使用 dx-1/dy-1 定位边框
- 所有头文件 include guard 从保留标识符 `_X` 改为 `X_`

## 代码质量审查记录（2026-07-26）

双 PWM 迁移 + QEI 编码器集成后复审，共修复 4 文件 7 项问题：

**关键修复**：
- tsp_wheel_enc.c: g_speed/g_last_raw/g_last_tick/g_first_run 缺少 volatile（CRITICAL）— ISR 写入、主循环读取，优化器可能缓存过期值
- tsp_wheel_enc.c: reset() 无临界区保护（HIGH）— SysTick 可能在复位中途触发 update()，导致 g_count 被脏 delta 污染
- tsp_odometer.c: S2 复位仍调用旋钮 tsp_encoder_reset() 而非轮子 tsp_wheel_enc_reset()（HIGH）— 迁移遗漏
- tsp_wheel_enc.c: 新增 g_running 守卫 — update() 在 QEI 计数器未启动时直接返回，防止垃圾累积
- tsp_motor.h: 文件头从旧 GPIO+PWM 描述更新为当前双 PWM 4 通道架构
- empty_mspm0g3519.c: Motor ClsLoop PID 增益重调（Kp 0.2→0.05, Ki 0.02→0.008, Kd 0.06→0.02），目标速度范围适配 QEI 反馈量级
- 所有修复已通过完整 8 步硬件调试流程验证
