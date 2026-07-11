#include "motor.h"

void motor_init(uint8_t motor_id)
{
    if (motor_id == 1)
    {
        DL_Timer_startCounter(PWMA_INST);
        DL_GPIO_setPins(Motor_l_AIN1_PORT, Motor_l_AIN1_PIN);
        DL_GPIO_setPins(Motor_l_AIN2_PORT, Motor_l_AIN2_PIN);
        DL_Timer_setCaptureCompareValue(PWMA_INST, 0U, GPIO_PWMA_C0_IDX);
    }
    else if (motor_id == 2)
    {
        DL_Timer_startCounter(PWMB_INST);
        DL_GPIO_setPins(Motor_2_PORT, Motor_2_BIN1_PIN);
        DL_GPIO_setPins(Motor_2_PORT, Motor_2_BIN2_PIN);
        DL_Timer_setCaptureCompareValue(PWMB_INST, 0U, GPIO_PWMB_C1_IDX);
    }
    DL_Timer_startCounter(Motor_PID_INST);
    NVIC_EnableIRQ(Motor_PID_INST_INT_IRQN);
}

int32_t limit_duty(int32_t duty)
{
    if (duty > 4000)
    {
        duty = 4000;
    }
    else if (duty < 0)
    {
        duty = 0;
    }
    return duty;
}



void motor_set_duty(uint8_t motor_id, uint32_t duty)
{
    duty = limit_duty(duty);

    if (motor_id == 1)
    {
        DL_Timer_setCaptureCompareValue(PWMA_INST, duty, GPIO_PWMA_C0_IDX);
    }
    else if (motor_id == 2)
    {
        DL_Timer_setCaptureCompareValue(PWMB_INST, duty, GPIO_PWMB_C1_IDX);
    }
}

// direction: 0 stop, 1 forward, 2 backward
void motor_set_direction(uint8_t motor_id, uint8_t direction)
{
    if (motor_id == 1)
    {
        switch (direction)
        {
            case 1U:
                DL_GPIO_setPins(Motor_l_AIN2_PORT, Motor_l_AIN2_PIN);
                DL_GPIO_clearPins(Motor_l_AIN1_PORT, Motor_l_AIN1_PIN);
                break;

            case 2U:
                DL_GPIO_setPins(Motor_l_AIN1_PORT, Motor_l_AIN1_PIN);
                DL_GPIO_clearPins(Motor_l_AIN2_PORT, Motor_l_AIN2_PIN);
                break;

            case 0U:
            default:
                DL_GPIO_clearPins(Motor_l_AIN1_PORT, Motor_l_AIN1_PIN);
                DL_GPIO_clearPins(Motor_l_AIN2_PORT, Motor_l_AIN2_PIN);
                break;
        }
    }
    else if (motor_id == 2)
    {
        switch (direction)
        {
            case 1U:
                DL_GPIO_setPins(Motor_2_PORT, Motor_2_BIN2_PIN);
                DL_GPIO_clearPins(Motor_2_PORT, Motor_2_BIN1_PIN);
                break;

            case 2U:
                DL_GPIO_setPins(Motor_2_PORT, Motor_2_BIN1_PIN);
                DL_GPIO_clearPins(Motor_2_PORT, Motor_2_BIN2_PIN);
                break;

            case 0U:
            default:
                DL_GPIO_clearPins(Motor_2_PORT, Motor_2_BIN1_PIN);
                DL_GPIO_clearPins(Motor_2_PORT, Motor_2_BIN2_PIN);
                break;
        }
    }
}
extern uint32_t counter_B;
extern uint32_t counter_A;
float speed_A = 0;
float speed_B = 0;

void calculate_speed(uint8_t motor_id)
{
    if (motor_id == 1)
    {
        speed_A = (float)counter_A / MOTOR_BIANMAQI * PI * MOTOR_WHEEL_D * 100; // mm/s 
        counter_A = 0;
    }
    else if (motor_id == 2)
    {
        speed_B = (float)counter_B / MOTOR_BIANMAQI * PI * MOTOR_WHEEL_D * 100; // mm/s
        counter_B = 0;
    }
}


float kp = 0.4; //比例系数
float ki = 0.3; //积分系数

int32_t PWM_A_duty = 0;
int32_t PWM_B_duty = 0;

float target_speed_A = 0;
float target_speed_B = 0;    
float last_error_A = 0;
float last_error_B = 0;    
float current_error_A = 0;
float current_error_B = 0;


void DC_Motor_PID(uint8_t motor_id)
{
    float error = 0;
    if (motor_id == 1)
    {
        error = target_speed_A - speed_A;
        current_error_A = error;
        PWM_A_duty += (int32_t)(kp * (current_error_A - last_error_A) + ki * current_error_A);
        last_error_A = current_error_A;
        PWM_A_duty = limit_duty(PWM_A_duty);
        motor_set_duty(motor_id, (uint32_t)PWM_A_duty);
    }
    else if (motor_id == 2)
    {
        error = target_speed_B - speed_B;
        current_error_B = error;
        PWM_B_duty += (int32_t)(kp * (current_error_B - last_error_B) + ki * current_error_B);
        last_error_B = current_error_B;
        PWM_B_duty = limit_duty(PWM_B_duty);
        motor_set_duty(motor_id, (uint32_t)PWM_B_duty);
    }
}
    


void Motor_PID_INST_IRQHandler(void)
{
    switch (DL_Timer_getPendingInterrupt(Motor_PID_INST))
    {
    case DL_TIMER_IIDX_LOAD:
        calculate_speed(1);
        DC_Motor_PID(1);
        calculate_speed(2);
        DC_Motor_PID(2);
        break; 


    default:
        break;
    }
}
