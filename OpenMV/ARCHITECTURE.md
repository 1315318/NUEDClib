# NUEDClib 系统架构与接口规范

## 系统概览

```
┌─────────────────────────────────────────────────────────────────┐
│                        OpenMV Cam                              │
│  main.py                                                        │
│  ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌─────────────┐ │
│  │ 传感器    │ → │ 检测算法  │ → │ 偏移计算  │ → │ 帧打包      │ │
│  │ snapshot  │   │ find_blobs│   │ cx,cy→off │   │ struct.pack │ │
│  └──────────┘   └──────────┘   └──────────┘   └──────┬──────┘ │
│                                                       │ UART TX│
└───────────────────────────────────────────────────────┼────────┘
                                                        │
                                           5-byte frame │ 115200bps
                                                        │
┌───────────────────────────────────────────────────────┼────────┐
│                        MSPM0G3507                      │ RX     │
│  ┌──────────┐   ┌──────────────┐   ┌──────────────────┴──────┐ │
│  │ 电机驱动  │ ← │  P 控制器     │ ← │  UART 解析              │ │
│  │ gimbal   │   │  vision.c    │   │  uart.c                 │ │
│  └──────────┘   └──────────────┘   └─────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

---

## 一、UART 协议（OpenMV ↔ MSPM0 之间的契约）

### 1.1 物理层

| 参数 | 值 |
|------|-----|
| 波特率 | 115200 |
| 数据位 | 8 |
| 校验位 | None |
| 停止位 | 1 |
| 方向 | OpenMV TX(P1) → MSPM0 RX(P0)，单向 |
| 帧间隔 | 无要求（每帧独立，状态机自动同步） |

### 1.2 帧格式（5 字节）

```
 Byte 0   Byte 1    Byte 2     Byte 3     Byte 4
┌────────┬─────────┬──────────┬──────────┬────────┐
│  0xAA  │ status  │ offset_x │ offset_y │  0xBB  │
│ 帧头   │ 状态字节 │ X偏移    │ Y偏移    │ 帧尾   │
│ uint8  │ uint8   │ int8     │ int8     │ uint8  │
└────────┴─────────┴──────────┴──────────┴────────┘
```

### 1.3 status 字节位定义

```
 Bit  7   6   5   4   3     2     1     0
┌────┬────┬────┬────┬────┬─────┬─────┬─────┐
│  0 │  0 │  0 │  0 │  0 │ conf│ conf│valid│
│  保留                                  │
└────┴────┴────┴────┴────┴─────┴─────┴─────┘
```

| 位 | 名称 | 说明 |
|----|------|------|
| bit 0 | `data_valid` | 0=未检测到目标, 此帧的 offset 无效; 1=检测到目标, offset 有效 |
| bit 1-2 | `confidence` | 0~3, 检测置信度。3=高度可信, 2=基本确定, 1=勉强检测 |
| bit 3-7 | reserved | 保留, 填 0 |

### 1.4 偏移值范围

QQVGA = 160×120 像素，画面中心 = (80, 60)：

| 字段 | 类型 | 范围 | 含义 |
|------|------|------|------|
| `offset_x` | int8 | -80 ~ +79 | 正值=目标在中心右侧, 负值=左侧 |
| `offset_y` | int8 | -60 ~ +59 | 正值=目标在中心下方, 负值=上方 |

### 1.5 data_valid=0 时的约定

- `offset_x` 和 `offset_y` 必须填 `0`（安全默认值）
- MSPM0 检查 `data_valid==0` 后**忽略** offset 值，使用上一帧有效数据保持位置
- 持续 `data_valid=0` 超过 `VISION_TIMEOUT_MS`(200ms) 后电机停机

---

## 二、OpenMV 端接口规范 (main.py)

### 2.1 任务脚本必须遵守的约定

每个追踪任务的 `main.py` 必须：

1. **输出 5 字节帧**：`struct.pack("<BBbbB", 0xAA, status, offset_x, offset_y, 0xBB)`
2. **正确设置 status 字节**：
   - 检测到目标 → `data_valid=1`, `confidence` 按任务自定义规则填入 1~3
   - 未检测到目标 → `data_valid=0`, `confidence=0`, `offset_x=offset_y=0`
3. **每帧都发送**：MSPM0 依赖持续收帧来判断连接正常（200ms 超时）

### 2.2 传感器配置（可任务自定义）

| 参数 | 循迹任务 | 颜色追踪任务 | 说明 |
|------|---------|-------------|------|
| `pixformat` | GRAYSCALE | RGB565 | 像素格式 |
| `framesize` | QQVGA | QQVGA | **固定**，协议偏移范围依赖此分辨率 |
| `auto_gain` | False | False | **必须关闭**，否则阈值失效 |
| `auto_whitebal` | False | False | **必须关闭** |
| `auto_exposure` | False | False | **必须关闭**，曝光时间按场景设定 |
| `exposure_us` | 25000 | 15000 | 按光照调参 |

### 2.3 检测算法（可任务自定义）

OpenMV 负责：
- 选择合适的 `find_blobs` / `find_template` / `find_apriltags` 等
- 从检测结果中选出**唯一**要追踪的目标（通常取最大 blob）
- 做基本的有效性过滤（密度、尺寸等）

OpenMV **不负责**：
- 控制算法（那是 MSPM0 的事）
- 平滑滤波（那是 MSPM0 的事）
- 电机逻辑

### 2.4 confidence 计算指南

confidence 的**语义**是"这个偏移值有多可信"，不是"目标有多大"。各任务自行定义阈值，但应遵循：

| conf | 语义 | 典型条件 |
|------|------|---------|
| 3 | 高度可信 | 目标清晰、无遮挡、特征完整 |
| 2 | 基本确定 | 目标部分可见或有轻微干扰 |
| 1 | 勉强检测 | 目标在视野边缘、或被部分遮挡 |

MSPM0 会根据 confidence 缩放电机速度：conf=3 → 全速, conf=2 → 75%, conf=1 → 50%。

### 2.5 当前已实现的任务

| 任务 | 文件 | 像素格式 | 阈值 |
|------|------|---------|------|
| 循迹（已移走） | `main.py`(旧) | GRAYSCALE | `[(0, 64)]` |
| 红色颜色追踪（当前） | `main.py` | RGB565 | `[(30, 80, 20, 127, -20, 40)]` |

---

## 三、MSPM0 端接口规范

### 3.1 文件与职责

```
MSPM0G3507/
├── main.c                    # 入口, 调用 process_deviation()
├── User/
│   ├── uart.h / uart.c       # UART 收帧 + 协议解析
│   ├── trace.h / trace.c     # 红外循迹（独立子系统，当前未使用）
│   └── motor.h / motor.c     # 驱动轮电机（独立子系统）
│
OpenMV/
├── vision.h / vision.c       # 云台 P 控制器
├── gimbal_motor.h / gimbal_motor.c  # 云台步进电机驱动
└── main.py                   # OpenMV 端任务脚本
```

### 3.2 UART 层 (uart.h / uart.c)

**公共接口：**

```c
// 轮询 UART FIFO 并驱动状态机，通常在 UART_get_deviations 内部调用
void UART_poll_rx(void);

// 尝试获取一帧数据（非阻塞）
// 返回 true 表示收到完整且校验通过的帧
// out_status: 帧的 status 字节（原始值，调用方自行解析位）
// out_x:      offset_x (int8)
// out_y:      offset_y (int8)
bool UART_get_deviations(uint8_t *out_status, int8_t *out_x, int8_t *out_y);
```

**实现逻辑：**

1. 调用 `UART_poll_rx()` 从 FIFO 读取字节
2. 状态机：`IDLE → STATUS → DATA_X → DATA_Y → TAIL → IDLE`
3. 帧校验：
   - 帧头 `rx_buffer[0] == 0xAA`
   - 帧尾 `rx_buffer[4] == 0xBB`
   - 长度 `rx_index == 5`
4. 校验失败 → 丢弃帧，状态机回到 IDLE 等待下一帧头
5. **不关心** status/x/y 的语义，纯粹传递

**内部常量：**

```c
#define RX_BUF_SIZE 6   // 接收缓冲区大小（5 字节帧 + 1 字节冗余）
```

### 3.3 控制层 (vision.h / vision.c)

**公共接口：**

```c
// 主循环每轮调用一次。内部完成：
//   1. 读取 UART 帧
//   2. 解析 data_valid / confidence
//   3. EMA 低通滤波
//   4. 迟滞死区判断
//   5. P 控制 + 置信度增益缩放
//   6. 输出到云台电机
void process_deviation(void);
```

**配置宏（可调参数）：**

```c
// ── X 轴 (L 电机, 水平旋转) ──
#define VISION_KP_X              1     // 比例增益 (speed = KP * |offset|)
#define VISION_DEAD_ZONE_ENTER_X 3     // 进入死区阈值 (像素)
#define VISION_DEAD_ZONE_EXIT_X  5     // 退出死区阈值 (像素)
#define VISION_SPEED_MIN_X       5     // 最小速度 (°/s)
#define VISION_SPEED_MAX_X       80    // 最大速度 (°/s)

// ── Y 轴 (R 电机, 垂直旋转) ──
#define VISION_KP_Y              1
#define VISION_DEAD_ZONE_ENTER_Y 3
#define VISION_DEAD_ZONE_EXIT_Y  5
#define VISION_SPEED_MIN_Y       5
#define VISION_SPEED_MAX_Y       80

// ── 共用 ──
#define VISION_TIMEOUT_MS        200   // 失联超时 (ms)
#define VISION_EMA_ALPHA_Q8      64    // EMA 系数 Q8: 64/256=0.25
```

**置信度增益表：**

```c
static const uint16_t CONF_GAIN_Q8[] = {0, 128, 192, 256};
//   conf=0:   0/256 = 0.00x (不应出现，data_valid=0 时不进入控制)
//   conf=1: 128/256 = 0.50x (勉强检测 → 半速谨慎)
//   conf=2: 192/256 = 0.75x (基本确定 → 中速)
//   conf=3: 256/256 = 1.00x (高度可信 → 全速)
```

**控制流程图：**

```
process_deviation()
  │
  ├─ UART_get_deviations(&status, &x, &y)
  │    │
  │    ├─ 有帧 → 更新 last_frame_ms
  │    │         │
  │    │         ├─ data_valid=1 → 更新 last_dev, last_confidence, EMA
  │    │         └─ data_valid=0 → 保持旧值（等超时）
  │    │
  │    └─ 无帧 → 不更新
  │
  ├─ 超时检查 (sys_tick_ms - last_frame_ms > 200ms)
  │    └─ 超时 → 两轴停机 + EMA 复位 + return
  │
  └─ control_axis(L, ctrl_x, ..., last_confidence)
     control_axis(R, ctrl_y, ..., last_confidence)
       │
       ├─ 迟滞死区判断
       │    ├─ 在死区内, |dev| ≤ exit  → 停机, return
       │    ├─ 在死区内, |dev| >  exit → 退出死区, 继续
       │    ├─ 跟踪中,   |dev| ≤ enter → 进入死区, 停机, return
       │    └─ 跟踪中,   |dev| >  enter → 继续
       │
       ├─ speed = KP * |dev|
       ├─ clamp(speed, speed_min, speed_max)
       ├─ speed = speed * CONF_GAIN_Q8[confidence] >> 8
       ├─ speed = max(speed, 1)   // 保证能动
       └─ 设置方向 + 写入 PWM
```

### 3.4 云台电机层 (gimbal_motor.h / gimbal_motor.c)

**公共接口：**

```c
#define GIMBAL_MOTOR_L 0   // 水平轴
#define GIMBAL_MOTOR_R 1   // 垂直轴
#define GIMBAL_MOTOR_DIRECTION_FORWARD  0
#define GIMBAL_MOTOR_DIRECTION_REVERSE  1

void gimbal_motor_init(uint8_t motor_id);             // 初始化 GPIO + 定时器
void gimbal_motor_set_dir(uint8_t motor_id, uint8_t direction);  // 设置方向
void gimbal_motor_set_speed(uint8_t motor_id, uint8_t speed);    // 设置速度 (°/s)
void gimbal_motor_set_continuous(uint8_t motor_id, uint8_t continuous); // 连续模式
void gimbal_motor_start(uint8_t motor_id);             // 启动
void gimbal_motor_stop(uint8_t motor_id);              // 停止
void gimbal_motor_set_angle(uint8_t motor_id, uint8_t angle);  // 定角度转动
```

**调用约定：**

- 必须先 `_init()` 再使用
- `process_deviation()` 首次调用时会自动 `_set_continuous(L, 1)` 和 `_set_continuous(R, 1)`
- 速度换算：`frequency = speed / 0.05625`（每脉冲 0.05625°），PWM 占空比固定 50%

### 3.5 红外循迹子系统 (trace.h / trace.c)（独立，当前停用）

```c
void trace_get_value(void);   // 读取 4 路红外传感器 → trace_data[4]
void trace_motor(void);       // 基于质心误差的线跟随控制
```

与视觉子系统**互斥使用**——`main.c` 的 `while(1)` 中只调用其中一个。

---

## 四、调用约定与时序契约

### 4.1 帧率与实时性

| 项目 | 约束 | 说明 |
|------|------|------|
| OpenMV 帧率 | 无硬性要求 | 30fps 左右正常，掉到 10fps 仍可工作 |
| MSPM0 主循环频率 | 越快越好 | `while(1)` 无延时，`process_deviation()` 非阻塞 |
| 超时阈值 | 200ms | 超过此时间无有效帧 → 停机 |
| EMA 平滑窗口 | α=0.25 | 约 4 帧收敛到新值 63%，约 12 帧收敛到 95% |

### 4.2 启动时序

```
T=0     OpenMV 上电 → sensor.reset() → skip_frames(2000ms)
        MSPM0 上电  → SYSCFG_DL_init() → motor_init() → gimbal_motor_init()
T=2s    OpenMV 开始发送帧
        MSPM0 process_deviation() 首次调用 → set_continuous()
T=2s+   正常跟踪
```

MSPM0 在 OpenMV 发帧之前（前 2 秒）处于超时状态，电机会保持停机。这是安全的。

### 4.3 异常恢复

| 异常 | OpenMV 行为 | MSPM0 行为 | 恢复 |
|------|-----------|-----------|------|
| 目标消失 | data_valid=0, offset=0 | 保持 last_dev, 等超时 | 目标重现 → 立即恢复 |
| 短暂遮挡 | data_valid=0 | 同上 | 同上 |
| UART 断线 | 无帧发出 | 200ms 后超时停机 | 重连后自动恢复 |
| OpenMV 重启 | 2s 后恢复发帧 | 200ms 超时 → 停机 → 恢复后自动跟踪 | 自动 |
| MSPM0 重启 | 不受影响 | 重新初始化 | 自动 |
| 全黑/过曝 | data_valid=0 | 保持位置 | 光照恢复 → 自动恢复 |

---

## 五、新增追踪任务的 checklist

在 OpenMV 端实现新的追踪任务时，确保：

- [ ] 像素格式与阈值匹配（GRAYSCALE 用单通道，RGB565 用 LAB 六通道）
- [ ] `framesize` 保持 QQVGA（偏移范围硬依赖 160×120）
- [ ] auto_gain / auto_whitebal / auto_exposure 全部 False
- [ ] 检测到目标时：`data_valid=1`, `confidence` 填入 1/2/3
- [ ] 未检测到目标时：`data_valid=0`, `confidence=0`, `offset_x=offset_y=0`
- [ ] 帧打包格式：`struct.pack("<BBbbB", 0xAA, status, offset_x, offset_y, 0xBB)`
- [ ] 每帧都发送（包括无效帧），保持 MSPM0 侧连接存活
- [ ] MSPM0 端 **不需要任何修改**
