#include "motor.h"

void motor_init(uint8_t motor_id)
{
    if (motor_id == 1)
    {
        /* PWM timer already started by SYSCFG_DL_init();
         * only set initial direction (brake) and zero duty */
        DL_GPIO_setPins(Motor_l_AIN1_PORT, Motor_l_AIN1_PIN);
        DL_GPIO_setPins(Motor_l_AIN2_PORT, Motor_l_AIN2_PIN);
        DL_TimerG_setCaptureCompareValue(PWMA_INST, 0U, DL_TIMERG_CAPTURE_COMPARE_0_INDEX);
    }
    else if (motor_id == 2)
    {
        /* PWM timer already started by SYSCFG_DL_init() */
        DL_GPIO_setPins(Motor_2_PORT, Motor_2_BIN1_PIN);
        DL_GPIO_setPins(Motor_2_PORT, Motor_2_BIN2_PIN);
        DL_TimerA_setCaptureCompareValue(PWMB_INST, 0U, DL_TIMERA_CAPTURE_COMPARE_1_INDEX);
    }
    /* PID timer and ISR disabled while using open-loop control.
     * Restore DL_Timer_startCounter + NVIC_EnableIRQ when
     * DC_Motor_PID() and Motor_PID_INST_IRQHandler() are re-enabled.
     */
    
}

/*
 * QEI encoder mode uses TIMG5/TIMG6 hardware with built-in
 * digital glitch filtering.  No GPIO interrupts needed — the
 * timer hardware counts encoder pulses and filters noise
 * autonomously in the background.
 */

/**
 * @brief  Clamp PWM duty to valid range [0, PWM_DUTY_MAX].
 *         Accepts int32_t so negative-out-of-range is caught at compile time.
 */
uint32_t limit_duty(int32_t duty)
{
    if (duty > (int32_t)PWM_DUTY_MAX)
    {
        return PWM_DUTY_MAX;
    }
    if (duty < 0)
    {
        return 0;
    }
    return (uint32_t)duty;
}

void motor_set_duty(uint8_t motor_id, uint32_t duty)
{
    duty = limit_duty((int32_t)duty);

    if (motor_id == 1)
    {
        /* PWMA = TIMG7 (general-purpose timer) */
        DL_TimerG_setCaptureCompareValue(PWMA_INST, duty, DL_TIMERG_CAPTURE_COMPARE_0_INDEX);
    }
    else if (motor_id == 2)
    {
        /* PWMB = TIMA0 (advanced timer — different register layout!) */
        DL_TimerA_setCaptureCompareValue(PWMB_INST, duty, DL_TIMERA_CAPTURE_COMPARE_1_INDEX);
    }
}

/* direction: 0 = stop, 1 = forward, 2 = backward */
void motor_set_direction(uint8_t motor_id, uint8_t direction)
{
    if (motor_id == 1)
    {
        switch (direction)
        {
            case 1U:   /* forward  */
                DL_GPIO_setPins(Motor_l_AIN1_PORT, Motor_l_AIN1_PIN);
                DL_GPIO_clearPins(Motor_l_AIN2_PORT, Motor_l_AIN2_PIN);
                break;

            case 2U:   /* backward */
                DL_GPIO_clearPins(Motor_l_AIN1_PORT, Motor_l_AIN1_PIN);
                DL_GPIO_setPins(Motor_l_AIN2_PORT, Motor_l_AIN2_PIN);
                break;

            case 0U:
            default:    /* stop     */
                DL_GPIO_clearPins(Motor_l_AIN1_PORT, Motor_l_AIN1_PIN);
                DL_GPIO_clearPins(Motor_l_AIN2_PORT, Motor_l_AIN2_PIN);
                break;
        }
    }
    else if (motor_id == 2)
    {
        switch (direction)
        {
            case 1U:   /* forward  */
                DL_GPIO_setPins(Motor_2_PORT, Motor_2_BIN1_PIN);
                DL_GPIO_clearPins(Motor_2_PORT, Motor_2_BIN2_PIN);
                break;

            case 2U:   /* backward */
                DL_GPIO_clearPins(Motor_2_PORT, Motor_2_BIN1_PIN);
                DL_GPIO_setPins(Motor_2_PORT, Motor_2_BIN2_PIN);
                break;

            case 0U:
            default:    /* stop     */
                DL_GPIO_clearPins(Motor_2_PORT, Motor_2_BIN1_PIN);
                DL_GPIO_clearPins(Motor_2_PORT, Motor_2_BIN2_PIN);
                break;
        }
    }
}

uint16_t PID_T = 10; //ms

extern uint32_t counter_1_A;
extern uint32_t counter_2_A;

float speed_1 = 0;
float speed_2 = 0;

void calculate_speed(uint8_t motor_id)
{
    
    if (motor_id == 1)
    {
        speed_1 = (float)counter_1_A / MOTOR_BIANMAQI * PI * MOTOR_WHEEL_D * 1000/PID_T;
        counter_1_A = 0;
    }
    if (motor_id == 2)
    {
        speed_2 = (float)counter_2_A / MOTOR_BIANMAQI * PI * MOTOR_WHEEL_D * 1000/PID_T;
        counter_2_A = 0;
    }
}

float kp = 0.5;
float ki = 0.4;

#define INTEGRAL_MAX  2000.0f
#define INTEGRAL_MIN -2000.0f

uint16_t PWM_1_duty = 0;
float target_speed_1 = 0;
float last_error_1 = 0;
float current_error_1 = 0;

uint16_t PWM_2_duty = 0;
float target_speed_2 = 0;
float last_error_2 = 0;
float current_error_2 = 0;

void motor_PID(uint8_t motor_id)
{
    float error;
    if (motor_id == 1) {
        error = target_speed_1 - speed_1;
        current_error_1= error;
        PWM_1_duty += (uint16_t)(kp * (current_error_1 - last_error_1) + ki * (current_error_1));
        last_error_1 = current_error_1;
        PWM_1_duty = (int32_t)limit_duty(PWM_1_duty);
        motor_set_duty(motor_id, (uint32_t)PWM_1_duty);
    }
    if (motor_id == 2) {
        error = target_speed_2 - speed_2;
        current_error_2= error;
        PWM_2_duty += (uint16_t)(kp * (current_error_2 - last_error_2) + ki * (current_error_2));
        last_error_2 = current_error_2;
        PWM_2_duty = (int32_t)limit_duty(PWM_2_duty);
        motor_set_duty(motor_id, (uint32_t)PWM_2_duty);
    }
}

void PID_INST_IRQHandler()
{
    switch (DL_Timer_getPendingInterrupt(PID_INST))
    {
    case DL_TIMER_IIDX_LOAD:
    {
        trace_motor();
        calculate_speed(1);
        motor_PID(1);
        calculate_speed(2);
        motor_PID(2);
        break;
    }
    default:
        break;
    }
}