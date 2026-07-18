/*
 * gimbal_freertos — FreeRTOS 版云台工程
 *
 * 原 example/gimbal 保持不动；本工程从中拷贝驱动后改为任务结构。
 *
 * 任务:
 *   motion_task  — 双步进开环运动（原 main 循环）
 *   comm_task    — 串口 PRINT/DEBUG 提示（后续可接命令）
 *
 * 步进 STEP 仍由 TIMA0/TIMG0 硬件 PWM + ISR 完成，不在任务里软件翻转。
 */

#include "ti_msp_dl_config.h"
#include "step_motor.h"
#include "uart.h"

#include "FreeRTOS.h"
#include "task.h"

#define MOTION_TASK_STACK   (256)
#define COMM_TASK_STACK     (192)
#define MOTION_TASK_PRIO    (2)
#define COMM_TASK_PRIO      (1)

static void motion_task(void *arg)
{
    (void)arg;

    step_set_speed(60, 1);
    step_set_speed(60, 2);

    for (;;) {
        /* 两路正转 90°（负载侧；电机1 内部 ×4 齿轮） */
        step_motor_dir_set(0, 1);
        step_motor_dir_set(0, 2);
        step_motor_set_angle(90, 1);
        step_motor_set_angle(90, 2);
        vTaskDelay(pdMS_TO_TICKS(2000));

        /* 两路反转 90° */
        step_motor_dir_set(1, 1);
        step_motor_dir_set(1, 2);
        step_motor_set_angle(90, 1);
        step_motor_set_angle(90, 2);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

static void comm_task(void *arg)
{
    (void)arg;

    NVIC_EnableIRQ(PRINT_INST_INT_IRQN);
    NVIC_EnableIRQ(DEBUG_INST_INT_IRQN);

    UART_send_string(PRINT_INST, "\r\n[gimbal_freertos] PRINT ready\r\n");
    UART_send_string(DEBUG_INST, "\r\n[gimbal_freertos] DEBUG ready\r\n");

    for (;;) {
        /* 心跳：每 5s 打一行，证明调度器在跑 */
        UART_send_string(PRINT_INST, "heartbeat\r\n");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

int main(void)
{
    /* 硬件初始化（与 bare-metal 相同） */
    SYSCFG_DL_init();
    step_motor_init();

    /* 创建任务 */
    xTaskCreate(motion_task, "motion", MOTION_TASK_STACK, NULL,
                MOTION_TASK_PRIO, NULL);
    xTaskCreate(comm_task, "comm", COMM_TASK_STACK, NULL, COMM_TASK_PRIO,
                NULL);

    /* 启动调度器（不会返回） */
    vTaskStartScheduler();

    /* 若堆不够创建 idle/timer 任务会走到这里 */
    for (;;) {
    }
}
