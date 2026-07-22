#include "ti_msp_dl_config.h"
#include "delay.h"
#include "uart.h"
#include "step_motor.h"
#include "track_proto.h"
#include "track_control.h"

#include <stdio.h>

/*
 * 视觉通讯测试
 *
 * 接线:
 *   MaixCAM TX → MCU PA31 (UART0 RX)  115200
 *   MaixCAM RX ← MCU PA28 (UART0 TX)  可选
 *   GND 共地
 *   DEBUG: PB6 TX / PB7 RX  115200  看解析结果
 *
 * 帧 15 字节: AA 55 | type=01 | flags | err_x | err_y | pitch | roll | yaw | sum
 *
 * TRACK_MOVE=0 只解析打印，不驱动电机
 * TRACK_MOVE=1 解析后走 P 控制云台
 */
#define TRACK_MOVE  1

int main(void)
{
    uint32_t ok_cnt   = 0;
    uint32_t last_dbg = 0;

    SYSCFG_DL_init();
    systick_init();
    step_motor_init();

    NVIC_EnableIRQ(PRINT_INST_INT_IRQN);
    NVIC_EnableIRQ(DEBUG_INST_INT_IRQN);

    g_track_cmd.fresh = 0;
    track_control_init();

    UART_send_string(DEBUG_INST, "\r\n=== vision link test ===\r\n");
    UART_send_string(DEBUG_INST, "UART0 RX wait AA 55 frames @115200\r\n");
#if TRACK_MOVE
    UART_send_string(DEBUG_INST, "TRACK_MOVE=1 motors ON\r\n");
#else
    UART_send_string(DEBUG_INST, "TRACK_MOVE=0 motors OFF (parse only)\r\n");
#endif

    while (1) {
        track_cmd_t m;

        if (track_proto_take(&m)) {
            ok_cnt++;
#if TRACK_MOVE
            /* take 清了 fresh，写回后再 update */
            g_track_cmd.found   = m.found;
            g_track_cmd.err_x   = m.err_x;
            g_track_cmd.err_y   = m.err_y;
            g_track_cmd.pitch   = m.pitch;
            g_track_cmd.roll    = m.roll;
            g_track_cmd.yaw     = m.yaw;
            g_track_cmd.last_ms = m.last_ms;
            g_track_cmd.fresh   = 1;
            track_control_update();
#endif
        } else {
#if TRACK_MOVE
            track_control_update();
#endif
        }

        /* DEBUG 降频，避免 snprintf/串口拖慢控制 */
        {
            uint32_t now = millis();
            if (now - last_dbg >= 100U) {
                last_dbg = now;
                if (ok_cnt == 0) {
                    UART_send_string(DEBUG_INST, "waiting frame...\r\n");
                } else {
                    char line[80];
                    snprintf(line, sizeof(line),
                             "f=%u ex=%d ey=%d n=%lu\r\n",
                             (unsigned)g_track_cmd.found,
                             (int)g_track_cmd.err_x,
                             (int)g_track_cmd.err_y,
                             (unsigned long)ok_cnt);
                    UART_send_string(DEBUG_INST, line);
                }
            }
        }
    }
}
