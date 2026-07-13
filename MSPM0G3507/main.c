#include <stdio.h>

#include "ti_msp_dl_config.h"

#include "delay.h"
#include "oled.h"
#include "motor.h"
#include "trace.h"
#include "mpu_port.h"

int status = 0;
extern float target_speed_A;
extern float target_speed_B;
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


void SysTick_Handler(void) {
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

    int dmp_ret = DMP_Init();
    sprintf(buf, "DMP:%d", dmp_ret);
    OLED_ShowString(0, 16, (u8 *)buf, 16);
    OLED_Refresh();
    if (dmp_ret != 0) {
        while (1);
    }

    float pitch, roll, yaw;
    char oled_str[20];

    motor_init(1);
    motor_init(2);

    target_speed_A = 0;
    target_speed_B = 0;

    while (1)
    {
        trace_motor();
        if (DMP_Read_Data(&pitch, &roll, &yaw) == 0) {
            sprintf(oled_str,"p:%.2f",pitch);
            OLED_ShowString(0, 16, (u8 *)oled_str, 16);
            sprintf(oled_str,"r:%.2f",roll);
            OLED_ShowString(0, 32, (u8 *)oled_str, 16);
            sprintf(oled_str,"y:%.2f",yaw);
            OLED_ShowString(0, 48, (u8 *)oled_str, 16);
            OLED_Refresh();
        }
    }
}
