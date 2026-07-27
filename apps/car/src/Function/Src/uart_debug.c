#include "uart_debug.h"
#include "ti_msp_dl_config.h"
#include <stdarg.h>
#include <stdio.h>

/* JustFloat frame tail */
static const uint8_t s_jf_tail[4] = {0x00u, 0x00u, 0x80u, 0x7Fu};

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

void UartDebug_JustFloat(const float *data, uint8_t n)
{
    if (data == 0 || n == 0)
        return;
    UartDebug_WriteBuf((const uint8_t *)data, (uint16_t)n * 4u);
    UartDebug_WriteBuf(s_jf_tail, 4u);
}

void UartDebug_FireWater(const float *data, uint8_t n)
{
    char buf[160];
    int pos = 0;
    uint8_t i;

    if (data == 0 || n == 0)
        return;

    for (i = 0; i < n && pos < (int)sizeof(buf) - 16; i++) {
        int w;
        if (i > 0 && pos < (int)sizeof(buf) - 1)
            buf[pos++] = ',';
        w = snprintf(buf + pos, sizeof(buf) - (size_t)pos, "%.4f", (double)data[i]);
        if (w <= 0)
            break;
        pos += w;
    }
    if (pos < (int)sizeof(buf) - 1) {
        buf[pos++] = '\n';
        buf[pos] = '\0';
        UartDebug_WriteBuf((const uint8_t *)buf, (uint16_t)pos);
    }
}
