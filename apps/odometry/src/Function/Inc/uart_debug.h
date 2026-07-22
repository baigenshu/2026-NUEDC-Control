#ifndef __UART_DEBUG_H
#define __UART_DEBUG_H

#include <stdint.h>

/* UART0 PA10/PA11 @115200 — human-readable debug */
void UartDebug_Init(void);
void UartDebug_Write(const char *s);
void UartDebug_WriteBuf(const uint8_t *p, uint16_t n);
void UartDebug_Printf(const char *fmt, ...);

#endif
