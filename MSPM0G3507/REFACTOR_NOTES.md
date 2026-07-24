# 控制框架重构文档

> 第一次重构: 2026-07-23 — 轻量 ISR + 开环/闭环可切换 + 全定点整数运算  
> 第二次重构: 2026-07-24 — 删除运行时模式切换 + 开环/闭环独立文件 + 编译时按需引入

---

## 1. 重构动机

### 第一次重构 (2026-07-23)

| 问题 | 严重程度 | 说明 |
|------|:--------:|------|
| ISR 中软浮点运算 | 🔴 致命 | M0+ 无 FPU，每次 ISR ~14 次 `__aeabi_fmul/div`，耗时 200-500μs |
| 开环/闭环互相冲突 | 🔴 致命 | `trace_motor()` 直接写 PWM，紧接着 `motor_PID(target=0)` 覆盖 |
| 竞态条件 | 🔴 致命 | `trace_motor()` 同时被 ISR 和 `while(1)` 主循环调用 |
| 控制周期不可调 | 🟡 | PID_T=20ms 硬编码，降低周期会放大浮点开销 |

### 第二次重构 (2026-07-24)

| 问题 | 说明 |
|------|------|
| 运行时模式切换冗余 | 开环和闭环共用 `control.c`，通过 `control_mode_t` 枚举在 ISR 中分支判断，增加 cycle 开销 |
| 耦合度高 | 开环常量（`DUTY_STRAIGHT`）和闭环 PID 状态混在同一文件，修改任一模式需触碰整个文件 |
| 按键逻辑复杂 | KEY_1/KEY_2 在 3 个 status 间循环（0→1→2），用户需记住当前处于哪种模式 |
| 构建不灵活 | 无法在编译时剥离不需要的模式，两个模式的代码始终占用 flash |

---

## 2. 文件变更清单

### 第二次重构 (2026-07-24)

| 文件 | 操作 | 说明 |
|------|:--:|------|
| `User/control.h` | **重写** | 删除 `control_mode_t`、`control_set_mode/get_mode`；新增 4 个 variant hook 声明 |
| `User/control.c` | **重写** | 删除 PID 代码、模式枚举、模式切换；保留 ISR 入口 + 传感器读取 + 特殊状态 + 调用 variant hooks（~70 行） |
| `User/control_open.h` | **新建** | 开环常量：`DUTY_STRAIGHT`(1300)、`DUTY_GAIN`(300)、`DUTY_BIAS`(20) |
| `User/control_open.c` | **新建** | 开环 variant hooks：`control_variant_init/reset`（空）、`control_straight_duty`、`control_track_duty`（~30 行） |
| `User/control_closed.h` | **新建** | 闭环常量：`BASE_SPEED_Q8_8`(25600)、`SPEED_GAIN_Q8_8`(7680)、`KP_Q8_8`(128)、`KI_Q8_8`(102) |
| `User/control_closed.c` | **新建** | 闭环 variant hooks + 完整 PID 实现（从旧 `control.c` 迁入，~100 行） |
| `User/interrupt.c` | 修改 | KEY_1/KEY_2 改为 `status = !status`（启停切换）；KEY_3 改为 `status = 1`（强制运行） |
| `User/key.h` | 修改 | 删除未使用的 `STATUS_TRACE` 宏 |
| `main.c` | 修改 | 删除 `last_mode`/`desired_mode`/switch-case；OLED 简化为 `S:%d`；注释更新 |
| `Debug/makefile` | 修改 | 新增 `CONTROL_MODE := open`；条件链接 `control_open.o` 或 `control_closed.o`；clean 列表补充 |
| `Debug/MSPM0G3507/User/subdir_vars.mk` | 修改 | 条件编译：根据 `CONTROL_MODE` 添加对应的 `.c`/`.o`/`.d` |

### 第一次重构 (2026-07-23)

| 文件 | 操作 | 行数 | 说明 |
|------|:--:|:--:|------|
| `User/control.h` | **新建** | 20 | 控制模式枚举 + API 声明 |
| `User/control.c` | **新建** | 225 | 定点 PID、模式管理、ISR 入口、control_update() |
| `User/trace.h` | 重写 | 23 | 移除 `target_speed_A/B`、`trace_motor()`；新增传感器 API |
| `User/trace.c` | 重写 | 69 | 移除控制逻辑和 `DUTY_*` 宏；保留纯传感器读取 |
| `User/motor.h` | 修改 | 28 | 移除 `calculate_speed()`、`motor_PID()`；新增 `motor_read_encoder()` |
| `User/motor.c` | 修改 | 137 | 移除全部 PID 代码和 `PID_INST_IRQHandler`；新增定点编码器读取 |
| `main.c` | 修改 | 94 | 移除 `trace_motor()` 调用；status 自动映射控制模式 |
| `Debug/makefile` | 修改 | — | 添加 `control.o` 到 ORDERED_OBJS 和 clean |
| `Debug/MSPM0G3507/User/subdir_vars.mk` | 修改 | — | 添加 `control.c` / `control.o` / `control.d` |

**未改动的文件**: `interrupt.h`, `oled.c`, `delay.c`, `uart.c`, `gimbal_motor.c`, `gimbal_motor.h`, OpenMV 下所有文件

---

## 3. 架构：三层分离 + Variant 模式

```
┌──────────────────────────────────────────────────────────────┐
│                   应用层 (main.c)                            │
│  OLED 显示 · 按键启停 (status 0/1)                            │
│  编译时决定模式: CONTROL_MODE=open | closed                    │
└────────────────────┬─────────────────────────────────────────┘
                     │ 调用 control_init()
┌────────────────────▼─────────────────────────────────────────┐
│                  控制层 (control.c)                           │
│                                                               │
│  PID_INST_IRQHandler()  ← 24 bytes, 唯一写 PWM 的入口         │
│    └── control_update()  ← 传感器读取 + 特殊状态 (共享)         │
│          ├── active==0        → STOP + control_variant_reset() │
│          ├── inner || lost    → control_straight_duty()        │
│          └── normal            → control_track_duty(error)     │
│                                                               │
│  四个 variant hook 在链接时解析到具体实现                         │
└──┬────────────────────────────────────────────────────────────┘
   │                          │
   │  ┌───────────────────────┴───────────────────────────┐
   │  │         Variant 层 (二选一，编译时决定)              │
   │  │                                                    │
   │  │  control_open.c          control_closed.c          │
   │  │  ─────────────           ────────────────           │
   │  │  duty = STRAIGHT         PID 增量式                 │
   │  │    ± error × GAIN        target = BASE              │
   │  │                            ± error × SPEED_GAIN     │
   │  │  无状态                   PID → duty 累加 + 钳位    │
   │  │                          有状态 (pid_L/R, duty_Q8)  │
   │  └────────────────────────────────────────────────────┘
   │                          │
   │ trace_get_error()        │ motor_read_encoder()
   │ trace_read()             │ motor_set_duty()
   │                          │ motor_set_direction()
┌──▼──────────────────┐  ┌──▼──────────────────────────────────┐
│  传感器层 (trace.c)   │  │       驱动层 (motor.c)               │
│                      │  │                                      │
│  trace_read()        │  │  motor_set_duty()      写 PWM 比较器  │
│  trace_get_error()   │  │  motor_set_direction()  写 GPIO 方向  │
│  trace_get_active()  │  │  motor_read_encoder()   读+清零编码器  │
│  trace_inner_both_   │  │  limit_duty()           PWM 钳位     │
│    line()            │  │                                      │
│                      │  │  [编码器脉冲由 interrupt.c 的          │
│  4 路红外 GPIO 读取   │  │   GROUP1_IRQHandler 累加]            │
└──────────────────────┘  └──────────────────────────────────────┘
```

---

## 4. Variant Hook 接口

在 `control.h` 中声明，由 `control_open.c` 或 `control_closed.c` 实现。
编译时只链接一个 variant 文件 —— 未实现的符号会导致链接错误，确保不会同时引入两个。

```c
void control_variant_init(void);           // 模式特定初始化
void control_variant_reset(void);          // 停车时重置内部状态
void control_straight_duty(void);          // inner/lost 直行占空比
void control_track_duty(int8_t error);     // 正常循迹占空比
```

### 数据流（一次控制周期 = 20ms）

```
PID_INST 定时器溢出
  │
  └─► PID_INST_IRQHandler()            [24 bytes]
        │
        └─► control_update()           [共享骨架]
              │
              ├─ 1. trace_read()            读 4 路 GPIO → trace_data[4]
              ├─ 2. trace_get_error()       质心误差, int8_t [-10, +10]
              ├─ 3. trace_get_active()      白区传感器计数 [0, 4]
              ├─ 4. trace_inner_both_line() 内对传感器姿态检测
              │
              ├─ 5. active == 0  → STOP + duty=0 + variant_reset()
              │
              ├─ 6. inner || active==4 → motor FORWARD
              │       └── control_straight_duty()   [variant]
              │
              └─ 7. normal → motor FORWARD
                      └── control_track_duty(error) [variant]
                            │
                            ├── [开环] duty = STRAIGHT ± error × GAIN
                            │              → motor_set_duty()
                            │
                            └── [闭环] target = BASE_SPEED ± error × SPEED_GAIN
                                        → motor_read_encoder() ×2
                                        → pid_update() ×2
                                        → duty_Q8_8 累加 + 钳位
                                        → motor_set_duty()
```

---

## 5. 编译时模式切换

在 `Debug/makefile` 第 8 行修改：

```make
CONTROL_MODE := open      # 开环循迹
# CONTROL_MODE := closed  # 闭环循迹（注释掉上面，启用这行）
```

切换后执行 `make clean && make` 重新编译。

按键行为适配：
```
status 0 → 停车（ISR cross 检测 + 按键启停）
status 1 → 运行（模式由编译决定）

KEY_1 / KEY_2 → 启停切换 (status = !status)
KEY_3         → 强制运行 (status = 1)
KEY_4         → 紧急停车 (status = 0)
```

---

## 6. 定点数约定

Cortex-M0+ 无硬件 FPU 和除法器。所有计算用 `int32_t`，通过缩放实现小数精度。

| 物理量 | 内部表示 | 缩放因子 | 范围 |
|--------|:------:|:------:|------|
| 速度 | Q8.8 | ×256 | ±32767 mm/s |
| PID 增益 | ×256 | kp=0.5 → 128 | ±128 |
| PWM 占空比 (累加器) | Q8.8 | ×256 | 0 ~ 4000×256 |
| 传感器误差 | 原生 | ×1 | -10 ~ +10 |

### 核心换算

```
速度换算 (motor_read_encoder):
  speed_Q8_8 = counter × 6805

  推导: counter / 260 × π × 44 × 1000 / 20 × 256
       = counter × (3.14159 × 44 × 50 × 256) / 260
       ≈ counter × 6805

PID 增量 (pid_update, 仅闭环):
  kp, ki 为 ×256 缩放; error 为 Q8.8

  delta = kp × (error - last_error)   → Q16.16
        + ki × error                   → Q16.16
  return delta >> 8                    → Q8.8

  等价浮点公式: Δout = 0.5 × Δe + 0.4 × e
```

### Q8.8 换算速查

| 浮点 | Q8.8 | 浮点 | Q8.8 |
|:----:|:----:|:----:|:----:|
| 0.1 | 26 | 1.0 | 256 |
| 0.4 | 102 | 1.5 | 384 |
| 0.5 | 128 | 30.0 | 7680 |
| 0.7 | 179 | 100.0 | 25600 |

---

## 7. 调参指南

### 开环参数 (`control_open.h`)

所有可调参数：

```c
#define DUTY_STRAIGHT  1300    // 直行基准占空比
#define DUTY_GAIN       300    // 误差增益 (PWM per error unit)
#define DUTY_BIAS        20    // 左轮偏置 (补偿硬件不对称)
```

| 现象 | 操作 |
|------|------|
| 跑偏跟不上线 | ↑ `DUTY_GAIN` (300→400) |
| 车速太快冲过头 | ↓ `DUTY_STRAIGHT` (1300→1000) |
| 转弯剧烈摇摆 | ↓ `DUTY_GAIN` (300→200) |
| 直行偏右 | ↑ `DUTY_BIAS` (20→40) |
| 直行偏左 | ↓ `DUTY_BIAS` (20→0) |

### 闭环参数 (`control_closed.h`)

所有可调参数：

```c
#define BASE_SPEED_Q8_8   25600   // 目标速度 Q8.8 (100.0 mm/s)
#define SPEED_GAIN_Q8_8    7680   // 误差→速度映射 Q8.8 (30.0)
#define KP_Q8_8            128    // PID 比例增益 Q8.8 (0.5)
#define KI_Q8_8            102    // PID 积分增益 Q8.8 (0.4)
```

按顺序调：

| 步骤 | 参数 | 方法 |
|:--:|------|------|
| 1 | `KP_Q8_8` | `KI_Q8_8=0`，逐步加大 KP 至直线上轻微震荡，回退到 70% |
| 2 | `KI_Q8_8` | 从 0 逐步加大，至稳态误差消失且不震荡 |
| 3 | `SPEED_GAIN_Q8_8` | 弯道差速不够则加大 |
| 4 | `BASE_SPEED_Q8_8` | 确定巡航速度 |

### 硬件改动时需重算

| 改动 | 文件 | 参数 |
|------|------|------|
| 编码器 PPR | `motor.h` | `MOTOR_BIANMAQI` |
| 轮径 | `motor.h` | `MOTOR_WHEEL_D` |
| 以上任意一项 | `motor.c` | **必须重算** `SPEED_FACTOR_Q8_8` |
| 控制周期 | `control_closed.c` | `PID_PERIOD_MS`, **必须重算** `SPEED_FACTOR_Q8_8` |

`SPEED_FACTOR_Q8_8` 计算公式：
```
SPEED_FACTOR_Q8_8 = π × WHEEL_D_mm × 1000 × 256 / (MOTOR_BIANMAQI × PID_PERIOD_MS)
                  ≈ 3.14159 × 44 × 1000 × 256 / (260 × 20)
                  ≈ 6805
```

---

## 8. 编译指标对比

| 指标 | 原始代码 | 第一次重构 | 第二次重构 |
|------|:------:|:------:|:----:|
| ISR 入口代码 | 420 bytes | 24 bytes | 24 bytes |
| 主控制函数 | 240 bytes (仅开环) | 512 bytes (开环+闭环) | ~70 bytes (共享) + variant |
| ISR 软浮点运算 | ~14 次/周期 | **0** | **0** |
| RAM 浮点变量 | 10×float=40 bytes | **0** | **0** |
| 控制相关 RAM | 58 bytes | 25 bytes | 共享 0 + variant 按需 |
| ISR 预估耗时 | 200-500 μs | 15-40 μs | 15-40 μs |
| 可降控制周期 | 20ms (受限于浮点) | 5-10ms | 5-10ms |
| 运行时模式切换 | ❌ | ✅ (按键) | ❌ (编译时) |
| Flash 占用 | 基准 | 基准 | **开环模式下省去 PID 代码** |

编译器: `ti-cgt-armllvm_5.1.1.LTS`, `-O2 -march=thumbv6m -mcpu=cortex-m0plus -mfloat-abi=soft`

---

## 9. 注意事项

1. **编码器读清零竞态**: `motor_read_encoder()` 在读取和清零之间可能丢失至多 1 个脉冲 (~0.05mm 位移)，M0+ 无原子 RMW 指令，此误差可接受。
2. **PID 占空比累加器** (仅闭环): `duty_L_Q8_8` / `duty_R_Q8_8` 在 Q8.8 精度下累加，输出时右移 8 位到原生 PWM 值。`control_closed_init()` 时初始化为 `INIT_DUTY_Q8_8`（1300×256）而非 0，避免冷启动顿挫。
3. **Variant 链接安全**: 四个 hook 函数 (`control_variant_init/reset/straight_duty/track_duty`) 由 `control_open.c` 或 `control_closed.c` 提供。如果两个同时编译会产生重复符号链接错误；如果都不编译则产生未定义符号错误。`CONTROL_MODE` 变量确保只引入一个。
4. **构建系统**: `makefile` 和 `subdir_vars.mk` 已手动修改。若用 CCS/SysConfig 重新生成项目文件，需重新添加 variant 相关的条件编译逻辑和 clean 条目。
5. **`trace_motor()` 已废弃**: 主循环不再调用任何电机控制函数，所有控制由 ISR → `control_update()` 完成。
6. **`SPEED_FACTOR_Q8_8`**: 仅在 `motor.c` 中定义（原先在 `control.c` 中有重复定义，已于第二次重构中移除）。
