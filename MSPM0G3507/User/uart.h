#ifndef UART_H
#define UART_H

#include <stdbool.h>
#include <stdint.h>

#include "ti_msp_dl_config.h"

void UART_send_char(UART_Regs *uart, const uint8_t chr);
void UART_send_string(UART_Regs *uart, const char *str);
void UART_poll_rx(void);
bool UART_get_deviations(uint8_t *out_status, int8_t *out_x, int8_t *out_y);

#endif 