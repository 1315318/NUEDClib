#include <stdio.h>

#include "ti_msp_dl_config.h"

#include "delay.h"
#include "oled.h"
#include "motor.h"
#include "trace.h"
#include "control.h"
#include "uart.h"
#include "mpu_port.h"
#include "interrupt.h"
#include "key.h"

#include "../OpenMV/C/gimbal_motor.h"
#include "../OpenMV/C/vision.h"

volatile int status = 0;
extern volatile uint32_t sys_tick_ms;

static const char *reset_name(uint32_t cause)
{
    switch (cause)
    {
        case 0x04: return "!! BOR !!";
        case 0x02: return "POR NRST";
        case 0x09: return "Boot NRST";
        case 0x00: return "No Reset";
        default:   return "Other";
    }
}

void _system_post_cinit(void) { }
void __mpu_init(void) { }

void SysTick_Handler(void)
{
    sys_tick_ms++;
}

int main(void)
{
    SYSCFG_DL_init();

    OLED_Init();
    OLED_Clear();

    uint32_t cause = (uint32_t)DL_SYSCTL_getResetCause();
    char buf[20];
    sprintf(buf, "RST:%s", reset_name(cause));
    OLED_ShowString(0, 0, (u8 *)buf, 16);
    OLED_Refresh();

    delay_ms(500);

    motor_init(MOTOR_L);
    motor_init(MOTOR_R);
    control_init();                          /* fixed‑point PID + default open‑loop */

    DL_Timer_startCounter(PID_INST);
    NVIC_EnableIRQ(PID_INST_INT_IRQN);       /* → PID_INST_IRQHandler → control_update() */
    NVIC_EnableIRQ(GPIO_MULTIPLE_GPIOB_INT_IRQN);
    NVIC_EnableIRQ(KEY_GPIOA_INT_IRQN );

    motor_set_direction(MOTOR_L, MOTOR_FORWARD);
    motor_set_direction(MOTOR_R, MOTOR_FORWARD);

    // gimbal_motor_init(GIMBAL_MOTOR_L);
    // gimbal_motor_init(GIMBAL_MOTOR_R);

    while (1)
    {
        static uint8_t  last_status = 0xFF;
        static control_mode_t last_mode = (control_mode_t)0xFF;

        /* ── Status / mode display ── */
        if (last_status != status)
        {
            sprintf(buf, "S:%d M:%s", status,
                    control_get_mode() == CONTROL_OPEN_LOOP ? "OL" : "CL");
            OLED_ShowString(0, 0, (u8*)buf, 16);
            OLED_Refresh();
            last_status = status;
        }

        /* ── Map key‑cycled status to control mode ──
         *     status 0 → stop    (motors off)
         *     status 1 → open‑loop  line follow
         *     status 2 → closed‑loop line follow                           */
        control_mode_t desired_mode;
        switch (status) {
            case 0:  desired_mode = CONTROL_OPEN_LOOP;   break;  /* stop — duty=0 handled by ISR when sensors see cross */
            case 1:  desired_mode = CONTROL_OPEN_LOOP;   break;
            case 2:  desired_mode = CONTROL_CLOSED_LOOP; break;
            default: desired_mode = CONTROL_OPEN_LOOP;   break;
        }

        if (desired_mode != last_mode) {
            control_set_mode(desired_mode);
            last_mode = desired_mode;
        }

        /* ── All real‑time control is in PID_INST_IRQHandler → control_update() ── */

        // process_deviation();
    }
}
