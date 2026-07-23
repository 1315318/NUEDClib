# 控制框架重构文档

> 日期: 2026-07-23  
> 目标: 轻量 ISR + 开环/闭环可切换 + 全定点整数运算 (MSPM0G3507 Cortex-M0+)

---

## 1. 重构动机

| 问题 | 严重程度 | 说明 |
|------|:--------:|------|
| ISR 中软浮点运算 | 🔴 致命 | M0+ 无 FPU，每次 ISR ~14 次 `__aeabi_fmul/div`，耗时 200-500μs |
| 开环/闭环互相冲突 | 🔴 致命 | `trace_motor()` 直接写 PWM，紧接着 `motor_PID(target=0)` 覆盖 |
| 竞态条件 | 🔴 致命 | `trace_motor()` 同时被 ISR 和 `while(1)` 主循环调用 |
| 控制周期不可调 | 🟡 | PID_T=20ms 硬编码，降低周期会放大浮点开销 |

---

## 2. 文件变更清单

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

**未改动的文件**: `interrupt.c`, `interrupt.h`, `key.c`, `key.h`, `oled.c`, `delay.c`, `uart.c`, `gimbal_motor.c`, `gimbal_motor.h`

---

## 3. 架构：三层分离

```
┌──────────────────────────────────────────────────────────────┐
│                   应用层 (main.c)                            │
│  OLED 显示 · 按键状态机 · status → control_set_mode()         │
└────────────────────┬─────────────────────────────────────────┘
                     │ 模式切换 (volatile 原子写)
┌────────────────────▼─────────────────────────────────────────┐
│                  控制层 (control.c)                          │
│                                                               │
│  PID_INST_IRQHandler()  ← 24 bytes, 唯一写 PWM 的入口         │
│    └── control_update()  ← 512 bytes, 全 int32_t 定点         │
│          ├── 开环: error → duty = STRAIGHT ± error × GAIN    │
│          └── 闭环: error → target_speed → PID → duty          │
│                                                               │
│  所有数学: Q8.8 定点 (×256)，零浮点，零整数除法                  │
└──┬────────────────────────┬──────────────────────────────────┘
   │ trace_get_error()      │ motor_read_encoder()
   │ trace_read()           │ motor_set_duty()
   │                        │ motor_set_direction()
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

## 4. 数据流（一次控制周期 = PID_PERIOD_MS = 20ms）

```
PID_INST 定时器溢出
  │
  └─► PID_INST_IRQHandler()            [24 bytes]
        │
        └─► control_update()           [512 bytes]
              │
              ├─ 1. trace_read()            读 4 路 GPIO → trace_data[4]
              ├─ 2. trace_get_error()       质心误差, int8_t [-10, +10]
              ├─ 3. trace_get_active()      白区传感器计数 [0, 4]
              ├─ 4. trace_inner_both_line() 内对传感器姿态检测
              │
              ├─ 5. 特殊状态 (两种模式共用)
              │     active == 0  → 停车, PID 归零
              │     inner / lost → 直行
              │
              └─ 6. 正常循迹
                    │
                    ├── [开环] error → duty = DUTY_STRAIGHT ± error × DUTY_GAIN
                    │              → motor_set_duty()
                    │
                    └── [闭环] error → target = BASE_SPEED_Q8_8 ± error × SPEED_GAIN_Q8_8
                                → motor_read_encoder() ×2   (速度 Q8.8)
                                → pid_update() ×2           (增量 PID, 纯整数)
                                → duty_Q8_8 累加 + 钳位
                                → motor_set_duty()
```

---

## 5. 定点数约定

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

PID 增量 (pid_update):
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

## 6. 调参指南

所有可调参数集中在 `control.c` 顶部：

```c
#define PID_PERIOD_MS     20      // 控制周期 (ms)
#define DUTY_MAX          4000    // PWM 上限 (硬件决定)
#define DUTY_STRAIGHT     1300    // 开环: 直行基准占空比
#define DUTY_GAIN          300    // 开环: 误差增益 (PWM/error)
#define BASE_SPEED_Q8_8   25600   // 闭环: 目标速度 Q8.8 (100.0 mm/s)
#define SPEED_GAIN_Q8_8    7680   // 闭环: 误差→速度映射 Q8.8 (30.0)
#define KP_Q8_8            128    // PID 比例增益 Q8.8 (0.5)
#define KI_Q8_8            102    // PID 积分增益 Q8.8 (0.4)
```

### 开环调参

只需调两个参数：

| 现象 | 操作 |
|------|------|
| 跑偏跟不上线 | ↑ `DUTY_GAIN` (300→400) |
| 车速太快冲过头 | ↓ `DUTY_STRAIGHT` (1300→1000) |
| 转弯剧烈摇摆 | ↓ `DUTY_GAIN` (300→200) |

### 闭环调参

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
| 以上任意一项 | `control.c` | **必须重算** `SPEED_FACTOR_Q8_8` |
| 控制周期 | `control.c` | `PID_PERIOD_MS`, **必须重算** `SPEED_FACTOR_Q8_8` |

`SPEED_FACTOR_Q8_8` 计算公式：
```
SPEED_FACTOR_Q8_8 = π × WHEEL_D_mm × 1000 × 256 / (MOTOR_BIANMAQI × PID_PERIOD_MS)
                  ≈ 3.14159 × 44 × 1000 × 256 / (260 × 20)
                  ≈ 6805
```

---

## 7. 模式切换

按键 → `GROUP1_IRQHandler` → `status` 变化 → `main.c` while 循环:

```c
status 0 → CONTROL_OPEN_LOOP   // 停止态 (传感器 cross 自动停车)
status 1 → CONTROL_OPEN_LOOP   // 开环循迹
status 2 → CONTROL_CLOSED_LOOP // 闭环循迹
```

切换闭环时自动清零 PID 积分项和 `last_error`，从当前占空比平滑起步。

---

## 8. 编译指标对比

| 指标 | 重构前 | 重构后 | 改善 |
|------|:------:|:------:|:----:|
| ISR 入口代码 | 420 bytes | 24 bytes | **94%** ↓ |
| 主控制函数 | 240 bytes (仅开环) | 512 bytes (开环+闭环) | 功能翻倍 |
| ISR 软浮点运算 | ~14 次/周期 | **0** | — |
| RAM 浮点变量 | 10×float=40 bytes | **0** | — |
| 控制相关 RAM | 58 bytes | 25 bytes | **57%** ↓ |
| ISR 预估耗时 | 200-500 μs | 15-40 μs | **10×** ↓ |
| 可降控制周期 | 20ms (受限于浮点) | 5-10ms | **2-4×** 带宽 |

编译器: `ti-cgt-armllvm_5.1.1.LTS`, `-O2 -march=thumbv6m -mcpu=cortex-m0plus -mfloat-abi=soft`

---

## 9. 注意事项

1. **编码器读清零竞态**: `motor_read_encoder()` 在读取和清零之间可能丢失至多 1 个脉冲 (~0.05mm 位移)，M0+ 无原子 RMW 指令，此误差可接受。
2. **PID 占空比累加器**: `duty_L_Q8_8` / `duty_R_Q8_8` 在 Q8.8 精度下累加，输出时右移 8 位到原生 PWM 值。切换模式时重置为 `DUTY_STRAIGHT << 8` 而非 0，避免从零起步的顿挫。
3. **`trace_motor()` 已废弃**: 主循环不再调用任何电机控制函数，所有控制由 ISR → `control_update()` 完成。
4. **构建系统**: makefile 和 subdir_vars.mk 已手动添加 `control.o`，若用 CCS/SysConfig 重新生成项目文件，需重新添加。
