#include "vision.h"

#include <stdbool.h>
#include <stdint.h>

#include "gimbal_motor.h"
#include "../MSPM0G3507/User/uart.h"

// 水平轴 (L 电机) 控制参数
#define VISION_KP_X         1     // 比例增益
#define VISION_DEAD_ZONE_X  3     // 死区 (像素)
#define VISION_SPEED_MIN_X  5     // 最小跟踪速度 (°/s)
#define VISION_SPEED_MAX_X  80    // 最大跟踪速度 (°/s)

// 垂直轴 (R 电机) 控制参数
#define VISION_KP_Y         1     // 比例增益
#define VISION_DEAD_ZONE_Y  3     // 死区 (像素)
#define VISION_SPEED_MIN_Y  5     // 最小跟踪速度 (°/s)
#define VISION_SPEED_MAX_Y  80    // 最大跟踪速度 (°/s)

// 共用超时
#define VISION_TIMEOUT_MS   200   // 失联超时 (ms)

extern volatile uint32_t sys_tick_ms;

static int8_t   last_dev_x     = 0;
static int8_t   last_dev_y     = 0;
static uint32_t last_frame_ms  = 0;
static bool     frame_received  = false;
static bool     continuous_set  = false;

// 单轴 P 控制
static void control_axis(uint8_t motor_id, int8_t deviation,
                         int kp, int dead_zone,
                         int speed_min, int speed_max)
{
    int abs_dev = (deviation < 0) ? -deviation : deviation;

    if (abs_dev <= dead_zone)
    {
        gimbal_motor_stop(motor_id);
        return;
    }

    int speed = kp * abs_dev;
    if (speed < speed_min) speed = speed_min;
    if (speed > speed_max) speed = speed_max;

    if (deviation > 0)
    {
        gimbal_motor_set_dir(motor_id, GIMBAL_MOTOR_DIRECTION_FORWARD);
    }
    else
    {
        gimbal_motor_set_dir(motor_id, GIMBAL_MOTOR_DIRECTION_REVERSE);
    }

    gimbal_motor_set_speed(motor_id, (uint8_t)speed);
    gimbal_motor_start(motor_id);
}

void process_deviation(void)
{
    // 首次调用时, 两个电机都设为连续转动模式
    if (!continuous_set)
    {
        gimbal_motor_set_continuous(GIMBAL_MOTOR_L, 1);
        gimbal_motor_set_continuous(GIMBAL_MOTOR_R, 1);
        continuous_set = true;
    }

    // 1. 读取最新偏差 (有新帧才更新缓存)
    int8_t dev_x, dev_y;
    if (UART_get_deviations(&dev_x, &dev_y))
    {
        last_dev_x     = dev_x;
        last_dev_y     = dev_y;
        last_frame_ms  = sys_tick_ms;
        frame_received = true;
    }

    // 2. 失联保护: 从未收到帧, 或超过 TIMEOUT_MS 无帧 → 两轴停机
    if (!frame_received || (sys_tick_ms - last_frame_ms) > VISION_TIMEOUT_MS)
    {
        gimbal_motor_stop(GIMBAL_MOTOR_L);
        gimbal_motor_stop(GIMBAL_MOTOR_R);
        return;
    }

    // 3. 两轴独立 P 控制
    control_axis(GIMBAL_MOTOR_L, last_dev_x,
                 VISION_KP_X, VISION_DEAD_ZONE_X,
                 VISION_SPEED_MIN_X, VISION_SPEED_MAX_X);

    control_axis(GIMBAL_MOTOR_R, last_dev_y,
                 VISION_KP_Y, VISION_DEAD_ZONE_Y,
                 VISION_SPEED_MIN_Y, VISION_SPEED_MAX_Y);
}
