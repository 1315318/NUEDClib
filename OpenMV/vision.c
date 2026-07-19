#include "vision.h"

#include <stdbool.h>
#include <stdint.h>

#include "gimbal_motor.h"
#include "../MSPM0G3507/User/uart.h"

// 水平轴 (L 电机) 控制参数
#define VISION_KP_X             1     // 比例增益
#define VISION_DEAD_ZONE_ENTER_X 3    // 进入死区阈值 (像素)
#define VISION_DEAD_ZONE_EXIT_X  5    // 退出死区阈值 (像素)
#define VISION_SPEED_MIN_X      5     // 最小跟踪速度 (°/s)
#define VISION_SPEED_MAX_X      80    // 最大跟踪速度 (°/s)

// 垂直轴 (R 电机) 控制参数
#define VISION_KP_Y             1     // 比例增益
#define VISION_DEAD_ZONE_ENTER_Y 3    // 进入死区阈值 (像素)
#define VISION_DEAD_ZONE_EXIT_Y  5    // 退出死区阈值 (像素)
#define VISION_SPEED_MIN_Y      5     // 最小跟踪速度 (°/s)
#define VISION_SPEED_MAX_Y      80    // 最大跟踪速度 (°/s)

// 共用超时与滤波
#define VISION_TIMEOUT_MS       200   // 失联超时 (ms)
#define VISION_EMA_ALPHA_Q8     64    // EMA 系数 Q8: 64/256 = 0.25

extern volatile uint32_t sys_tick_ms;

static int8_t   last_dev_x      = 0;
static int8_t   last_dev_y      = 0;
static uint32_t last_frame_ms   = 0;
static bool     frame_received  = false;
static bool     continuous_set  = false;

// EMA 滤波状态 (Q8.8 定点)
static int16_t  ema_x_q8        = 0;
static int16_t  ema_y_q8        = 0;
static bool     ema_init        = false;

// 迟滞死区状态 (per-axis)
static bool     in_dead_zone_l  = false;
static bool     in_dead_zone_r  = false;

// 单轴 P 控制（带迟滞死区）
static void control_axis(uint8_t motor_id, int8_t deviation,
                         int kp, int dead_zone_enter, int dead_zone_exit,
                         int speed_min, int speed_max)
{
    int abs_dev = (deviation < 0) ? -deviation : deviation;

    bool *in_dz = (motor_id == GIMBAL_MOTOR_L) ? &in_dead_zone_l : &in_dead_zone_r;

    // 迟滞判断
    if (*in_dz)
    {
        // 当前在死区内：偏差必须超过 exit 阈值才退出
        if (abs_dev <= dead_zone_exit)
        {
            gimbal_motor_stop(motor_id);
            return;
        }
        *in_dz = false;
    }
    else
    {
        // 当前在跟踪中：偏差降到 enter 阈值以下才进入死区
        if (abs_dev <= dead_zone_enter)
        {
            *in_dz = true;
            gimbal_motor_stop(motor_id);
            return;
        }
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

    // 1. 读取最新帧 (含 status 字节)
    uint8_t frame_status;
    int8_t dev_x, dev_y;
    if (UART_get_deviations(&frame_status, &dev_x, &dev_y))
    {
        last_frame_ms  = sys_tick_ms;
        frame_received = true;

        // 检查 data_valid 标志 (bit 0)
        bool data_valid = (frame_status & 0x01) != 0;

        if (data_valid)
        {
            last_dev_x = dev_x;
            last_dev_y = dev_y;

            // EMA 低通滤波 (Q8.8 定点, alpha = 64/256 = 0.25)
            if (!ema_init)
            {
                ema_x_q8 = (int16_t)dev_x * 256;
                ema_y_q8 = (int16_t)dev_y * 256;
                ema_init = true;
            }
            else
            {
                // ema += alpha * (raw - ema)
                ema_x_q8 += (int16_t)(((int32_t)VISION_EMA_ALPHA_Q8
                            * ((int32_t)dev_x * 256 - (int32_t)ema_x_q8)) >> 8);
                ema_y_q8 += (int16_t)(((int32_t)VISION_EMA_ALPHA_Q8
                            * ((int32_t)dev_y * 256 - (int32_t)ema_y_q8)) >> 8);
            }
        }
        // data_valid=0 → 不更新 last_dev 和 EMA，保持当前状态
        // 由超时机制最终停机（如果持续无效）
    }

    // 2. 失联保护: 从未收到帧, 或超过 TIMEOUT_MS 无帧 → 两轴停机
    if (!frame_received || (sys_tick_ms - last_frame_ms) > VISION_TIMEOUT_MS)
    {
        gimbal_motor_stop(GIMBAL_MOTOR_L);
        gimbal_motor_stop(GIMBAL_MOTOR_R);
        ema_init = false;  // 重置滤波器
        return;
    }

    // 3. 用 EMA 滤波后的值进行两轴独立 P 控制
    int8_t ctrl_x = (int8_t)(ema_x_q8 >> 8);
    int8_t ctrl_y = (int8_t)(ema_y_q8 >> 8);

    control_axis(GIMBAL_MOTOR_L, ctrl_x,
                 VISION_KP_X, VISION_DEAD_ZONE_ENTER_X, VISION_DEAD_ZONE_EXIT_X,
                 VISION_SPEED_MIN_X, VISION_SPEED_MAX_X);

    control_axis(GIMBAL_MOTOR_R, ctrl_y,
                 VISION_KP_Y, VISION_DEAD_ZONE_ENTER_Y, VISION_DEAD_ZONE_EXIT_Y,
                 VISION_SPEED_MIN_Y, VISION_SPEED_MAX_Y);
}
