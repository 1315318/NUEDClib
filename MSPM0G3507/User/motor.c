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
    // DL_Timer_startCounter(Motor_PID_INST);
    // NVIC_EnableIRQ(Motor_PID_INST_INT_IRQN);
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

/* ── Speed tracking (for future PID use) ──
 * Encoder reading currently disabled.
 * When QEI timers are configured in pin.syscfg (TIMG5/TIMG6),
 * replace the dummy implementation below with hardware QEI reads.
 */

float speed_A = 0.0f;
float speed_B = 0.0f;

void calculate_speed(uint8_t motor_id)
{
    /* TODO: read QEI timer when Encoder_1_INST / Encoder_2_INST are
     * configured in pin.syscfg as QEI-mode TIMG instances */
    (void)motor_id;
}

/* ── PID speed-control variables (used when PID is re-enabled) ── */
float kp = 7.0f;              /* proportional gain */
float ki = 0.6f;              /* integral gain */
float PWM_A_duty   = 0.0f;
float PWM_B_duty   = 0.0f;
float target_speed_A = 0.0f;  /* target speed, motor 1 (left)  */
float target_speed_B = 0.0f;  /* target speed, motor 2 (right) */
float last_error_A = 0.0f;
float last_error_B = 0.0f;
float current_error_A = 0.0f;
float current_error_B = 0.0f;

/*
 * DC_Motor_PID() and Motor_PID_INST_IRQHandler() are currently
 * disabled (open-loop control).  To restore PID closed-loop speed
 * control, un-comment the two functions below and re-enable the
 * timer + IRQ in motor_init().
 */

#if 0   /* ── PID closed-loop control (disabled) ── */

void DC_Motor_PID(uint8_t motor_id)
{
    float error = 0;
    if (motor_id == 1)
    {
        error = target_speed_A - speed_A;
        current_error_A = error;
        PWM_A_duty += kp * (current_error_A - last_error_A)
                   +  ki *  current_error_A;

        if (PWM_A_duty > (float)PWM_DUTY_MAX) PWM_A_duty = (float)PWM_DUTY_MAX;
        if (PWM_A_duty < 0.0f)                PWM_A_duty = 0.0f;

        last_error_A = current_error_A;
        motor_set_duty(motor_id, (uint32_t)PWM_A_duty);
    }
    else if (motor_id == 2)
    {
        error = target_speed_B - speed_B;
        current_error_B = error;
        PWM_B_duty += kp * (current_error_B - last_error_B)
                   +  ki *  current_error_B;

        if (PWM_B_duty > (float)PWM_DUTY_MAX) PWM_B_duty = (float)PWM_DUTY_MAX;
        if (PWM_B_duty < 0.0f)                PWM_B_duty = 0.0f;

        last_error_B = current_error_B;
        motor_set_duty(motor_id, (uint32_t)PWM_B_duty);
    }
}

void Motor_PID_INST_IRQHandler(void)
{
    uint32_t pending = DL_Timer_getPendingInterrupt(Motor_PID_INST);

    switch (pending)
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

    DL_Timer_clearInterruptStatus(Motor_PID_INST, pending);
}

#endif  /* PID closed-loop control */
