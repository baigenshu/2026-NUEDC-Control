#include "ti_msp_dl_config.h"
#include <stdarg.h>
#include <stdio.h>

/* ~1ms @ 80MHz (approximate) */
#define POLL_SLICE_CYCLES (80000U)
/* Heartbeat every ~5s so uplink stays quiet for echo tests */
#define HEARTBEAT_SLICES (5000U)

static void debug_putc(char c)
{
    DL_UART_Main_transmitDataBlocking(DEBUG_UART_INST, (uint8_t)c);
}

static void debug_puts(const char *s)
{
    while (*s)
        debug_putc(*s++);
}

static void debug_printf(const char *fmt, ...)
{
    char buf[96];
    va_list ap;
    int n, i;

    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n <= 0)
        return;
    if (n > (int)sizeof(buf))
        n = (int)sizeof(buf);
    for (i = 0; i < n; i++)
        debug_putc(buf[i]);
}

static void echo_rx(void)
{
    while (!DL_UART_Main_isRXFIFOEmpty(DEBUG_UART_INST)) {
        uint8_t b = DL_UART_Main_receiveData(DEBUG_UART_INST);
        DL_UART_Main_transmitDataBlocking(DEBUG_UART_INST, b);
    }
}

int main(void)
{
    uint32_t cnt = 0;
    uint32_t i;

    SYSCFG_DL_init();

    /* Drain boot garbage from ESP-01 ROM/firmware prints on shared UART */
    for (i = 0; i < 200000U; i++) {
        while (!DL_UART_Main_isRXFIFOEmpty(DEBUG_UART_INST)) {
            (void)DL_UART_Main_receiveData(DEBUG_UART_INST);
        }
    }

    debug_puts("\r\n=== duplex echo @115200 ===\r\n");
    debug_puts("type text on PC; MCU echoes it back\r\n");

    while (1) {
        for (i = 0; i < HEARTBEAT_SLICES; i++) {
            echo_rx();
            delay_cycles(POLL_SLICE_CYCLES);
        }

        cnt++;
        debug_printf("alive cnt=%lu\r\n", (unsigned long)cnt);
    }
}
