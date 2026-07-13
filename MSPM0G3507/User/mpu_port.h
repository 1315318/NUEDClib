#ifndef _MPU_PORT_H_
#define _MPU_PORT_H_

#include "ti_msp_dl_config.h"
#include <stdint.h>

//接线
//MPU6050_SCL接到MSPM0的PA1
//MPU6050_SDA接到MSPM0的PA0
//VCC接到MSPM0的3.3V
//GND接到MSPM0的GND
//ADDR接到MSPM0的GND(MPU6050的I2C地址为0x68)二选一（默认情况）
//ADDR接到MSPM0的3.3V(MPU6050的I2C地址为0x69)二选一

// 官方库需要的 4 个底层函数声明
int MPU_Write_Len(unsigned char addr, unsigned char reg, unsigned char len, unsigned char *buf);
int MPU_Read_Len(unsigned char addr, unsigned char reg, unsigned char len, unsigned char *buf);
void mget_ms(unsigned long *time);

// 我们自己封装的 DMP 初始化和读取函数
int DMP_Init(void);
int DMP_Read_Data(float *pitch, float *roll, float *yaw);

#endif

