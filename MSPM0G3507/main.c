#include <stdio.h>

#include "ti_msp_dl_config.h"

#include "delay.h"
#include "oled.h"
#include "motor.h"
#include "trace.h"

int status = 0;
extern float target_speed_A;
extern float target_speed_B;
extern uint8_t trace_data[4];

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

    motor_init(1);
    motor_init(2);

    target_speed_A = 0;
    target_speed_B = 0;

    while (1) {
        trace_motor();
    }
}
