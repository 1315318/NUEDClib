#include "vision.h"

#include <stdbool.h>
#include <stdint.h>

#include "gimbal_motor.h"
#include "../MSPM0G3507/User/uart.h"

// 视觉跟踪控制参数
#define VISION_KP         1     // 比例增益 (speed = KP * |deviation|)
#define VISION_DEAD_ZONE  3     // 死区 (像素), 偏差绝对值小于此不动作
#define VISION_SPEED_MIN  5     // 最小跟踪速度 (°/s), 克服静摩擦
#define VISION_SPEED_MAX  80    // 最大跟踪速度 (°/s)
#define VISION_TIMEOUT_MS 200   // 失联超时 (ms), 超时后停机

extern volatile uint32_t sys_tick_ms;

static int8_t   last_deviation = 0;
static uint32_t last_frame_ms  = 0;
static bool     frame_received  = false;
static bool     continuous_set  = false;

void process_deviation(void)
{
    // 首次调用时, 将 L 电机设为连续转动模式 (避免 step_remain ISR 干扰跟踪)
    if (!continuous_set)
    {
        gimbal_motor_set_continuous(GIMBAL_MOTOR_L, 1);
        continuous_set = true;
    }

    // 1. 读取最新偏差 (有新帧才更新缓存)
    int8_t deviation;
    if (UART_get_deviation(&deviation))
    {
        last_deviation = deviation;
        last_frame_ms  = sys_tick_ms;
        frame_received = true;
    }

    // 2. 失联保护: 从未收到帧, 或超过 TIMEOUT_MS 无帧 → 停机
    if (!frame_received || (sys_tick_ms - last_frame_ms) > VISION_TIMEOUT_MS)
    {
        gimbal_motor_stop(GIMBAL_MOTOR_L);
        return;
    }

    // 3. P 控制: 速度正比于偏差, 带死区和限幅
    int abs_dev = (last_deviation < 0) ? -(int)last_deviation : (int)last_deviation;

    if (abs_dev <= VISION_DEAD_ZONE)
    {
        gimbal_motor_stop(GIMBAL_MOTOR_L);
        return;
    }

    int speed = VISION_KP * abs_dev;
    if (speed < VISION_SPEED_MIN) speed = VISION_SPEED_MIN;
    if (speed > VISION_SPEED_MAX) speed = VISION_SPEED_MAX;

    if (last_deviation > 0)
    {
        gimbal_motor_set_dir(GIMBAL_MOTOR_L, GIMBAL_MOTOR_DIRECTION_FORWARD);
    }
    else
    {
        gimbal_motor_set_dir(GIMBAL_MOTOR_L, GIMBAL_MOTOR_DIRECTION_REVERSE);
    }

    gimbal_motor_set_speed(GIMBAL_MOTOR_L, (uint8_t)speed);
    gimbal_motor_start(GIMBAL_MOTOR_L);
}
