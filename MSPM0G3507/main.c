/*
 * Copyright (c) 2021, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ti_msp_dl_config.h"
#include "User/delay.h"
#include "oled.h"
#include "uart.h"
#include "motor.h"
#include <stdio.h>
#include "key.h"
#include "trace.h"




int status = 0;
extern float target_speed_A;
extern float target_speed_B;
extern uint8_t trace_data[4];  

int main(void)
{
    SYSCFG_DL_init();
    NVIC_EnableIRQ(GPIO_MULTIPLE_GPIOB_INT_IRQN);
    DL_Timer_startCounter(PWMA_INST);
    DL_Timer_startCounter(PWMB_INST);
    DL_Timer_setCaptureCompareValue(PWMA_INST, 50, GPIO_PWMA_C0_IDX);
    DL_Timer_setCaptureCompareValue(PWMB_INST, 50, GPIO_PWMB_C1_IDX);
    motor_init(1);
    motor_init(2);
    target_speed_A = 0;
    target_speed_B = 0;

    while (1) {
        DL_GPIO_setPins(Motor_2_PORT, Motor_2_BIN2_PIN);
        DL_GPIO_clearPins(Motor_2_PORT, Motor_2_BIN1_PIN);
        DL_GPIO_setPins(Motor_l_AIN2_PORT, Motor_l_AIN2_PIN);
        DL_GPIO_clearPins(Motor_l_AIN1_PORT, Motor_l_AIN1_PIN);
        motor_set_duty(1,2000);
        motor_set_duty(2,2000);
    }
}
