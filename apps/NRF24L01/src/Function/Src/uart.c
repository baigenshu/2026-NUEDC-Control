#include "uart.h"
#include <stdio.h>

void UART_send_char(UART_Regs *uart, uint8_t chr)
{
    DL_UART_Main_transmitDataBlocking(uart, chr);
}

void UART_send_string(UART_Regs *uart, const char *str)
{
    while (*str) {
        UART_send_char(uart, (uint8_t)*str);
        str++;
    }
}

int fputc(int ch, FILE *f)
{
    (void)f;
    UART_send_char(PRINT_INST, (uint8_t)ch);
    return ch;
}

int _write(int fd, char *ptr, int len)
{
    int i;
    (void)fd;
    for (i = 0; i < len; i++) {
        UART_send_char(PRINT_INST, (uint8_t)ptr[i]);
    }
    return len;
}
