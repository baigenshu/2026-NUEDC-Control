#include "uart.h"
#include "track_proto.h"

void UART_send_char(UART_Regs *uart, const uint8_t chr)
{
    DL_UART_transmitDataBlocking(uart, chr);
}

void UART_send_string(UART_Regs *uart, const char *str)
{
    while (*str) {
        UART_send_char(uart, (uint8_t)*str);
        str++;
    }
}

/* PRINT = UART0：MaixCAM 链路，RX 喂跟踪协议（不要回显二进制） */
void PRINT_INST_IRQHandler(void)
{
    switch (DL_UART_getPendingInterrupt(PRINT_INST)) {
    case DL_UART_IIDX_RX: {
        uint8_t rec = DL_UART_receiveData(PRINT_INST);
        track_proto_on_byte(rec);
        break;
    }
    default:
        break;
    }
}

/* DEBUG = UART1：调试口，RX 回显 */
void DEBUG_INST_IRQHandler(void)
{
    switch (DL_UART_getPendingInterrupt(DEBUG_INST)) {
    case DL_UART_IIDX_RX: {
        uint8_t rec = DL_UART_receiveData(DEBUG_INST);
        UART_send_char(DEBUG_INST, rec);
        break;
    }
    default:
        break;
    }
}
