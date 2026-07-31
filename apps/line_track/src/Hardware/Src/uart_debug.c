/**
 * @file uart_debug.c
 */
#include "uart_debug.h"
#include "ti_msp_dl_config.h"
#include <stdio.h>

void UartDebug_Init(void)
{
    /* 外设已在 SYSCFG_DL_DEBUG_UART_init 中使能 */
}

void UartDebug_Putc(char c)
{
    if (c == '\n')
        DL_UART_Main_transmitDataBlocking(DEBUG_UART_INST, (uint8_t)'\r');
    DL_UART_Main_transmitDataBlocking(DEBUG_UART_INST, (uint8_t)c);
}

void UartDebug_Write(const char *s, uint32_t len)
{
    uint32_t i;

    if (!s)
        return;
    for (i = 0; i < len; ++i)
        UartDebug_Putc(s[i]);
}

void UartDebug_Puts(const char *s)
{
    if (!s)
        return;
    while (*s)
        UartDebug_Putc(*s++);
}

void UartDebug_Printf(const char *fmt, ...)
{
    char buf[128];
    int n;
    va_list ap;

    if (!fmt)
        return;
    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n <= 0)
        return;
    if (n > (int)sizeof(buf) - 1)
        n = (int)sizeof(buf) - 1;
    UartDebug_Write(buf, (uint32_t)n);
}
