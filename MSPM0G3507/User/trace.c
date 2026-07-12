#include "trace.h"
#include "motor.h"

extern float target_speed_A;
extern float target_speed_B;

uint8_t trace_data[4] = {0, 0, 0, 0};

/* ── Duty cycle constants (PWM period = 4000) ── */
#define DUTY_MAX       4000   /* absolute upper limit */
#define DUTY_STRAIGHT  1000   /* base straight-line speed */
#define DUTY_GAIN       150   /* correction gain per error step */

/**
 * @brief  Read a single GPIO pin and return 1=line (black), 0=white.
 * @note   Most IR line-following sensor modules pull the output LOW
 *         when a black line is detected and HIGH on a white surface.
 *         Adjust the return polarity here if your module differs.
 */
uint8_t get_gpio_state(GPIO_Regs *gpio_port, uint32_t gpio)
{
    uint32_t high_bits = DL_GPIO_readPins(gpio_port, gpio);
    if ((gpio & high_bits) != 0)
    {
        return 0;   /* HIGH → white background */
    }
    else
    {
        return 1;   /* LOW  → black line */
    }
}

void trace_get_value()
{
    trace_data[0] = get_gpio_state(TRACE_X1_PORT, TRACE_X1_PIN);  /* leftmost  */
    trace_data[1] = get_gpio_state(TRACE_X2_PORT, TRACE_X2_PIN);
    trace_data[2] = get_gpio_state(TRACE_X3_PORT, TRACE_X3_PIN);
    trace_data[3] = get_gpio_state(TRACE_X4_PORT, TRACE_X4_PIN);  /* rightmost */
}

/**
 * @brief  Line-following control using centre-of-mass error.
 *
 * Each sensor is assigned a position weight (X1=-3, X2=-1, X3=+1, X4=+3).
 * The weighted sum of active sensors gives the line-position error:
 *   error < 0 → line is LEFT  → need RIGHT turn (motor 1 faster)
 *   error > 0 → line is RIGHT → need LEFT  turn (motor 2 faster)
 *   error = 0 → line is CENTERED
 *
 * This approach handles all 16 sensor bit-patterns without a long if-else chain.
 */
void trace_motor()
{
    trace_get_value();

    /* ── Count active sensors and compute position error ── */
    uint8_t  active = trace_data[0] + trace_data[1] + trace_data[2] + trace_data[3];
    int8_t   error  = 0;
    if (trace_data[0]) error += -3;
    if (trace_data[1]) error += -1;
    if (trace_data[2]) error += +1;
    if (trace_data[3]) error += +3;

    /* ── Lost line (0,0,0,0): go straight to find the line again ── */
    if (active == 0)
    {
        motor_set_direction(1, 1);
        motor_set_direction(2, 1);
        motor_set_duty(1, DUTY_STRAIGHT);
        motor_set_duty(2, DUTY_STRAIGHT);
        return;
    }

    /* ── All sensors on line (1,1,1,1): cross / end marker → stop ── */
    if (active == 4)
    {
        motor_set_direction(1, 0);
        motor_set_direction(2, 0);
        motor_set_duty(1, 0);
        motor_set_duty(2, 0);
        target_speed_A = 0;
        target_speed_B = 0;
        return;
    }

    /* ── Normal line-following ── */
    motor_set_direction(1, 1);   /* motor 1 (left)  forward */
    motor_set_direction(2, 1);   /* motor 2 (right) forward */

    /* duty = straight_base ± error * gain
     *
     *   error < 0 (line LEFT)    → L_faster, R_slower → right turn
     *   error > 0 (line RIGHT)   → L_slower, R_faster → left  turn
     */
    int32_t duty_L = (int32_t)DUTY_STRAIGHT - (int32_t)error * DUTY_GAIN;
    int32_t duty_R = (int32_t)DUTY_STRAIGHT + (int32_t)error * DUTY_GAIN;

    /* Clamp to valid PWM range */
    if (duty_L < 0)   duty_L = 0;
    if (duty_L > (int32_t)DUTY_MAX) duty_L = DUTY_MAX;
    if (duty_R < 0)   duty_R = 0;
    if (duty_R > (int32_t)DUTY_MAX) duty_R = DUTY_MAX;

    motor_set_duty(1, (uint32_t)duty_L);
    motor_set_duty(2, (uint32_t)duty_R);
}
