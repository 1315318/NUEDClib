#include "trace.h"
#include "motor.h"

extern float target_speed_1;
extern float target_speed_2;

uint8_t trace_data[4] = {0, 0, 0, 0};

/* ── Duty cycle constants (PWM period = 4000) ── */
//#define DUTY_MAX       4000   /* absolute upper limit */
//#define DUTY_STRAIGHT  1000   /* base straight-line speed */
//#define DUTY_GAIN       150   /* correction gain per error step */

#define BASE_SPEED  200.0f
#define SPEED_GAIN  40.0f
/**
 * @brief  Read a single GPIO pin and return 1=white, 0=line (black).
 * @note   Most IR line-following sensor modules pull the output LOW
 *         when a black line is detected and HIGH on a white surface.
 */
uint8_t get_gpio_state(GPIO_Regs *gpio_port, uint32_t gpio)
{
    uint32_t high_bits = DL_GPIO_readPins(gpio_port, gpio);
    if ((gpio & high_bits) != 0)
    {
        return 1;   /* HIGH → white background */
    }
    else
    {
        return 0;   /* LOW  → black line */
    }
}

void trace_get_value()
{
    trace_data[0] = get_gpio_state(TRACE_X1_PORT, TRACE_X1_PIN);  
    trace_data[1] = get_gpio_state(TRACE_X2_PORT, TRACE_X2_PIN); /* leftmost  */
    trace_data[2] = get_gpio_state(TRACE_X3_PORT, TRACE_X3_PIN);
    trace_data[3] = get_gpio_state(TRACE_X4_PORT, TRACE_X4_PIN);  /* rightmost */
}

/**
 * @brief  Line-following control using centre-of-mass error.
 *
 * Each sensor is assigned a position weight (X2=+5, X1=+1, X3=-1, X4=-5).
 * Error is computed from sensors that see the black line (active low):
 *   error > 0 → line is LEFT  → need LEFT  turn (motor 2 faster)
 *   error < 0 → line is RIGHT → need RIGHT turn (motor 1 faster)
 *   error = 0 → line is CENTERED
 */
void trace_motor()
{
    trace_get_value();

    /* ── Count active sensors and compute position error ── */
    uint8_t  active = trace_data[0] + trace_data[1] + trace_data[2] + trace_data[3];
    int8_t   error  = 0;
    if (trace_data[0]) error += -2;
    if (trace_data[1]) error += -3.5;
    if (trace_data[2]) error += +2;
    if (trace_data[3]) error += +3.5;

    /* ── Lost line (1, 1, 1, 1): go straight to find the line again ── */
    if (active == 4)
    {
        motor_set_direction(1,1);
        motor_set_direction(2,1);
        target_speed_1 = BASE_SPEED;
        target_speed_2 = BASE_SPEED;
        return;
    }

    /* ── All sensors on line (0, 0, 0, 0): cross / end marker → stop ── */
    if (active == 0)
    {
        motor_set_direction(1, 0);
        motor_set_direction(2, 0);
        target_speed_1 = 0;
        target_speed_2 = 0;
        return;
    }

    /* ── Normal line-following ── */
    motor_set_direction(1, 1);   /* motor 1 (left)  forward */
    motor_set_direction(2, 1);   /* motor 2 (right) forward */

    /* duty = straight_base ± error * gain
     *
     *   error > 0 (line LEFT)     → L_slower, R_faster → left  turn
     *   error < 0 (line RIGHT)    → L_faster, R_slower → right turn
     */
    target_speed_1 = BASE_SPEED - error * SPEED_GAIN;
    target_speed_2 = BASE_SPEED + error * SPEED_GAIN;

    /* 防止负数或超速 */
    if (target_speed_1 < 0) target_speed_1 = 0;
    if (target_speed_1 > 500) target_speed_1 = 500;
    if (target_speed_2 < 0) target_speed_2 = 0;
    if (target_speed_2 > 500) target_speed_2 = 500;

    
    
}
