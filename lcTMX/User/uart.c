#include  "uart.h"

void UART_send_char(UART_Regs *uart,const uint8_t chr)
{
    DL_UART_transmitData(uart, chr);
}

void UART_send_string(UART_Regs *uart, const char *str)
{
    while (*str) {
        UART_send_char(uart, (uint8_t)(*str));
        str++;
    }
}

void UART_0_INST_IRQHandler()
{
    switch (DL_UART_getPendingInterrupt(UART_0_INST)) 
    {
        case DL_UART_IIDX_RX:
        {
            uint8_t receivedData = DL_UART_receiveData(UART_0_INST);
            UART_send_char(UART_0_INST, receivedData); // 回显接收到的数据
            break;
        }
            
            
        case DL_UART_IIDX_TX:
            
            break;
        default:
           
            break;
    }
}   