#include "ti_msp_dl_config.h"
#include "delay.h"
#include "uart.h"
#include "step_motor.h"

/*
 * 云台开环角度测试
 *
 * 电机1 = Yaw（左右，1:4 齿轮）
 * 电机2 = Pitch（上下，直驱）
 *
 * 流程：左 90° → 右 90° → 上 90° → 下 90° → 停 1s 循环
 * 速度/方向按现场可改 SPEED_* 与 DIR_*。
 */
#define SPEED_YAW_DPS     30U
#define SPEED_PITCH_DPS   30U
#define ANGLE_TEST_DEG    90U
#define PAUSE_MS          800U

/* 方向：接线反了把 0/1 对调 */
#define DIR_LEFT          0U
#define DIR_RIGHT         1U
#define DIR_UP            1U
#define DIR_DOWN          0U

#define MOTOR_YAW         1U
#define MOTOR_PITCH       2U

static void wait_motor(uint8_t id)
{
    while (step_motor_is_busy(id)) {
        /* spin */
    }
}

/* 相对转 angle_deg（负载侧 °），走完自动停 */
static void move_angle(uint8_t id, uint8_t dir, uint16_t angle_deg, uint16_t speed_dps)
{
    step_motor_dir_set(dir, id);
    step_set_speed(speed_dps, id);
    step_motor_set_angle(angle_deg, id);
    wait_motor(id);
}

int main(void)
{
    SYSCFG_DL_init();
    systick_init();
    step_motor_init();

    NVIC_EnableIRQ(DEBUG_INST_INT_IRQN);
    UART_send_string(DEBUG_INST, "\r\n=== gimbal 90deg test ===\r\n");
    UART_send_string(DEBUG_INST, "L/R/U/D each 90 deg, loop\r\n");

    while (1) {
        UART_send_string(DEBUG_INST, "LEFT  90\r\n");
        move_angle(MOTOR_YAW, DIR_LEFT, ANGLE_TEST_DEG, SPEED_YAW_DPS);
        delay_ms(PAUSE_MS);

        UART_send_string(DEBUG_INST, "RIGHT 90\r\n");
        move_angle(MOTOR_YAW, DIR_RIGHT, ANGLE_TEST_DEG, SPEED_YAW_DPS);
        delay_ms(PAUSE_MS);

        UART_send_string(DEBUG_INST, "UP    90\r\n");
        move_angle(MOTOR_PITCH, DIR_UP, ANGLE_TEST_DEG, SPEED_PITCH_DPS);
        delay_ms(PAUSE_MS);

        UART_send_string(DEBUG_INST, "DOWN  90\r\n");
        move_angle(MOTOR_PITCH, DIR_DOWN, ANGLE_TEST_DEG, SPEED_PITCH_DPS);
        delay_ms(1000);
    }
}
