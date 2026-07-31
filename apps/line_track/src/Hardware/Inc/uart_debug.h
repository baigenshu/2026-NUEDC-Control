/**
 * @file uart_debug.h
 * @brief DEBUG UART0 · PA10 TX / PA11 RX · 115200
 */
#ifndef UART_DEBUG_H
#define UART_DEBUG_H

#include <stdint.h>
#include <stdarg.h>

void UartDebug_Init(void);
void UartDebug_Putc(char c);
void UartDebug_Write(const char *s, uint32_t len);
void UartDebug_Puts(const char *s);

/** 有限长度格式化输出（内部 128 字节缓冲） */
void UartDebug_Printf(const char *fmt, ...);

#endif /* UART_DEBUG_H */
