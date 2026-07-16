#include "ti_msp_dl_config.h"
#include "delay.h"
#include "step_motor.h"
#include "uart.h"

/*
 * 第二路步进 + 串口调试
 *
 * 步进:
 *   PA12=STEP  PA13=DIR  PA14=DCY  PA15=SLP  PA16=RST
 * 串口 PRINT (UART0 115200):
 *   PA28=TX  PA31=RX  → USB 转串口，终端 115200 8N1
 *
 * 每个 STEP 上升沿打印一个 '.'
 * 每 100 步换行打印累计步数
 *
 * 串口比 GPIO 慢，加打印后脉冲会变慢，只用来确认软件在发脉冲。
 */
static void uart_print_u32(uint32_t v)
{
    char tmp[12];
    int i = 0;

    if (v == 0U) {
        UART_send_char(PRINT_INST, '0');
        return;
    }
    while (v > 0U && i < (int)sizeof(tmp)) {
        tmp[i++] = (char)('0' + (v % 10U));
        v /= 10U;
    }
    while (i > 0) {
        UART_send_char(PRINT_INST, tmp[--i]);
    }
}

int main(void)
{
    uint32_t step_cnt = 0;

    SYSCFG_DL_init();
    step_motor_init();
    NVIC_EnableIRQ(PRINT_INST_INT_IRQN);

    step_motor_dir_set(0, 2);

    // UART_send_string(PRINT_INST, "\r\n=== step motor debug ===\r\n");
    // UART_send_string(PRINT_INST, "each '.' = 1 STEP rising edge\r\n");

    while (1) {
        /* STEP 上升沿：DRV8825 在此步进 */
        step_motor_step_set(1, 2);
        // UART_send_char(PRINT_INST, '.');
        // step_cnt++;

        // if ((step_cnt % 100U) == 0U) {
        //     UART_send_string(PRINT_INST, " cnt=");
        //     uart_print_u32(step_cnt);
        //     UART_send_string(PRINT_INST, "\r\n");
        // }

        delay_ms(1);

        step_motor_step_set(0, 2);
        delay_ms(1);
    }
}
