#include "uart_debug.h"
#include "ti_msp_dl_config.h"
#include <stdarg.h>
#include <stdio.h>

void UartDebug_Init(void)
{
    /* Peripheral already enabled in SYSCFG_DL_DEBUG_UART_init */
}

void UartDebug_WriteBuf(const uint8_t *p, uint16_t n)
{
    uint16_t i;
    for (i = 0; i < n; i++)
        DL_UART_Main_transmitDataBlocking(DEBUG_UART_INST, p[i]);
}

void UartDebug_Write(const char *s)
{
    while (*s)
        DL_UART_Main_transmitDataBlocking(DEBUG_UART_INST, (uint8_t)*s++);
}

void UartDebug_Printf(const char *fmt, ...)
{
    char buf[160];
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n > 0) {
        if (n > (int)sizeof(buf))
            n = (int)sizeof(buf);
        UartDebug_WriteBuf((const uint8_t *)buf, (uint16_t)n);
    }
}
