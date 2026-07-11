#include "trace.h"

extern float target_speed_A;
extern float target_speed_B;

uint8_t trace_data[4] = {0, 0, 0, 0};

uint8_t get_gpio_state(GPIO_Regs *gpio_port, uint32_t gpio)
{
    uint32_t high_bits = DL_GPIO_readPins(gpio_port, gpio);
    if((gpio & high_bits) != 0)
    { 
        return 1;
    }
    else
    {
        return 0;
    }
}

void trace_get_value()
{
    trace_data[0] = get_gpio_state(TRACE_X1_PORT, TRACE_X1_PIN);
    trace_data[1] = get_gpio_state(TRACE_X2_PORT, TRACE_X2_PIN);
    trace_data[2] = get_gpio_state(TRACE_X3_PORT, TRACE_X3_PIN);
    trace_data[3] = get_gpio_state(TRACE_X4_PORT, TRACE_X4_PIN);
}

float base_speed = 150.0f;
float trace_kp = 15.0f;
float trace_ki = 0.5f;
float trace_kd = 8.0f;

int8_t last_position = 0;
float trace_integral = 0.0f;

void trace_motor()
{
    int8_t position = 0;
    
    if (trace_data[0] == 0) position -= 3;
    if (trace_data[1] == 0) position -= 1;
    if (trace_data[2] == 0) position += 1;
    if (trace_data[3] == 0) position += 3;
    
    trace_integral += (float)position;
    if (trace_integral > 50.0f) trace_integral = 50.0f;
    if (trace_integral < -50.0f) trace_integral = -50.0f;
    
    int8_t position_diff = position - last_position;
    last_position = position;
    
    float speed_diff = trace_kp * (float)position + trace_ki * trace_integral + trace_kd * (float)position_diff;
    
    target_speed_A = base_speed - speed_diff;
    target_speed_B = base_speed + speed_diff;
    
    if (target_speed_A < 0) target_speed_A = 0;
    if (target_speed_B < 0) target_speed_B = 0;
    
    motor_set_direction(1, target_speed_A > 0 ? 1 : 0);
    motor_set_direction(2, target_speed_B > 0 ? 1 : 0);
}