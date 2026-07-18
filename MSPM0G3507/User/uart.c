#include "uart.h"

#include <stdbool.h>
#include <stdint.h>

#include "ti_msp_dl_config.h"

void UART_send_char(UART_Regs *uart, const uint8_t chr)
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

#define RX_BUF_SIZE 5
volatile uint8_t rx_buffer[RX_BUF_SIZE];
volatile uint8_t rx_index = 0;
volatile uint8_t frame_ready = 0;

typedef enum
{
    STATE_IDLE,   // 等待帧头 0xAA
    STATE_DATA_X, // 接收 offset_x
    STATE_DATA_Y, // 接收 offset_y
    STATE_TAIL    // 等待帧尾 0xBB
} rx_state_t;

volatile rx_state_t rx_state = STATE_IDLE;

void UART_poll_rx(void)
{
    uint8_t max_read = 64;
    while (max_read-- > 0 && DL_UART_Main_isRXFIFOEmpty(UART0_INST) == false)
    {
        uint8_t rx_byte = DL_UART_Main_receiveData(UART0_INST);

        switch (rx_state)
        {
            case STATE_IDLE:
                if (rx_byte == 0xAA)
                {
                    rx_index = 0;
                    rx_buffer[rx_index] = rx_byte;
                    rx_index++;
                }
                
                rx_state = STATE_DATA_X;
                break;

            case STATE_DATA_X:
                rx_buffer[rx_index] = rx_byte;
                rx_index++;
                
                rx_state = STATE_DATA_Y;
                break;

            case STATE_DATA_Y:
                rx_buffer[rx_index] = rx_byte;
                rx_index++
                
                rx_state = STATE_TAIL;
                break;

            case STATE_TAIL:
                if (rx_byte == 0xBB)
                {
                    rx_buffer[rx_index] = rx_byte;
                    rx_index++
                    frame_ready = 1;
                }
                
                rx_state = STATE_IDLE;
                break;
        }
    }
}

bool UART_get_deviations(int8_t *out_x, int8_t *out_y)
{
    UART_poll_rx();

    if (frame_ready)
    {
        frame_ready = 0;

        bool valid = false;

        // 校验帧格式: 0xAA | int8_t offset_x | int8_t offset_y | 0xBB, 共4字节
        if (rx_index == 4 && rx_buffer[0] == 0xAA && rx_buffer[3] == 0xBB)
        {
            *out_x = (int8_t)rx_buffer[1];
            *out_y = (int8_t)rx_buffer[2];
            valid = true;
        }

        // 重置缓冲区，准备接收下一帧
        rx_index = 0;
        rx_state = STATE_IDLE;

        return valid;
    }

    return false;
}