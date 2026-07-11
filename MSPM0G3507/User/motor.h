#ifndef MOTOR_H
#define MOTOR_H

#define PI 3.14

#include "ti_msp_dl_config.h"
#include "trace.h"

void motor_init(uint8_t motor_id);
void motor_set_duty(uint8_t motor_id, uint32_t duty);
void motor_set_direction(uint8_t motor_id, uint8_t direction);

//编码器线数
#define MOTOR_BIANMAQI 260
//轮胎直径
#define MOTOR_WHEEL_D 44

#endif /* MOTOR_H */