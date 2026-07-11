
#include "delay.h"


void delay_ms(uint32_t ms)   //延时函数   ms
{
    uint32_t cycles = ms * (CPUCLK_FREQ / 1000);
    delay_cycles(cycles);
}