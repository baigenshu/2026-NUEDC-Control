#include "ti_msp_dl_config.h"
#include "delay.h"
#include "uart.h"
#include "step_motor.h"

/*
 * 双路步进测试
 *
 * 接线:
 *   M1: PA0(STEP)  PA1(DIR)  PA7(DCY)  PA8(SLP)  PA9(RST)
 *   M2: PA12(STEP) PA13(DIR) PA14(DCY) PA15(SLP) PA16(RST)
 *   UART: PA28 TX / PA31 RX  115200
 *
 * 循环: 两路同向 90° → 反向 90°
 */
int main(void)
{
    SYSCFG_DL_init();
    step_motor_init();
    NVIC_EnableIRQ(PRINT_INST_INT_IRQN);
    NVIC_EnableIRQ(DEBUG_INST_INT_IRQN);

    step_set_speed(100, 1);
    step_set_speed(60, 2);

    UART_send_string(PRINT_INST, "\r\nPRINT ready (UART0 PA28/PA31)\r\n");
    UART_send_string(DEBUG_INST, "\r\nDEBUG ready (UART1 PB6/PB7)\r\n");

    while (1) {
        /* 两路正转 90° */
        step_motor_dir_set(0, 1);
        step_motor_dir_set(0, 2);
        step_motor_set_angle(90, 1);
        step_motor_set_angle(90, 2);
        delay_ms(2000);

        /* 两路反转 90° */
        step_motor_dir_set(1, 1);
        step_motor_dir_set(1, 2);
        step_motor_set_angle(90, 1);
        step_motor_set_angle(90, 2);
        delay_ms(2000);
    }
}
