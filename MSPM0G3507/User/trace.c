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

extern float target_speed_A;
extern float target_speed_B;

 float target_speed_5[] = {125, 175, 200, 400, 500};

void trace_motor()
{
    trace_get_value();
    if (trace_data[0] == 0 && trace_data[1] == 0 && trace_data[2] == 0 && trace_data[3] == 0) {
        motor_set_direction(1, 1);
        motor_set_direction(2, 1);
        float min_speed = target_speed_A < target_speed_B ? target_speed_A : target_speed_B;
        target_speed_A = min_speed;
        target_speed_B = min_speed;
    } 
    else if (trace_data[0] == 1 && trace_data[1] == 1 && trace_data[2] == 1 && trace_data[3] == 1)
    {
        target_speed_A = 0;
        target_speed_B = 0;
    }
    else if (trace_data[0] == 1 && trace_data[1] == 0 && trace_data[2] == 1 && trace_data[3] == 0) {
        motor_set_direction(1, 1);
        motor_set_direction(2, 1);
        float min_speed = target_speed_A < target_speed_B ? target_speed_A : target_speed_B;
        target_speed_A = min_speed;
        target_speed_B = min_speed;
    } 
    else if (trace_data[0] == 1 && trace_data[1] == 0)
    {
        target_speed_A = target_speed_5[2];
        target_speed_B = target_speed_5[3];
    }
    else if (trace_data[0] == 1 && trace_data[1] ==1 )
    {
        target_speed_A = target_speed_5[1];
        target_speed_B = target_speed_5[3];
    }
    else if (trace_data[1] ==1 )
    {
        target_speed_A = target_speed_5[0];
        target_speed_B = target_speed_5[3];
    }
    else if(trace_data[2] == 1 && trace_data[3] == 0)
    {
        target_speed_A = target_speed_5[3];
        target_speed_B = target_speed_5[2];
    }
    else if(trace_data[2] == 1 && trace_data[3] == 1)
    {
        target_speed_A = target_speed_5[3];
        target_speed_B = target_speed_5[1];
    }
    else if(trace_data[3] == 1)
    {
        target_speed_A = target_speed_5[3];
        target_speed_B = target_speed_5[0];
    }
}