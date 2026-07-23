#include <stdio.h>

#include "ti_msp_dl_config.h"

#include "delay.h"
#include "oled.h"
#include "motor.h"
#include "trace.h"
#include "uart.h"
#include "mpu_port.h"
#include "interrupt.h"
#include "key.h"

#include "../OpenMV/C/gimbal_motor.h"
#include "../OpenMV/C/vision.h"

volatile int status = 0;
extern float target_speed_1;
extern float target_speed_2;
extern uint8_t trace_data[4];
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

    DL_Timer_startCounter(PID_INST);
    NVIC_EnableIRQ(PID_INST_INT_IRQN);
    NVIC_EnableIRQ(GPIO_MULTIPLE_GPIOB_INT_IRQN);
    NVIC_EnableIRQ(KEY_GPIOA_INT_IRQN );
   
    motor_set_direction(MOTOR_L, MOTOR_FORWARD);
    motor_set_direction(MOTOR_R, MOTOR_FORWARD);

    // gimbal_motor_init(GIMBAL_MOTOR_L);
    // gimbal_motor_init(GIMBAL_MOTOR_R);
    
    while (1)
    {  
        //static变量只在编译时赋值一次
        static uint8_t last_status = 0xFF; 
        
        if (last_status != status)
        {
            sprintf(buf, "status:%d", status); 
            OLED_ShowString(0, 0, (u8*)buf, 16); 
            OLED_Refresh(); 
            last_status = status;
        }        
        if(status == STATUS_TRACE)
        {
            trace_motor(); 
        }

        //process_deviation();
    }
    
}
