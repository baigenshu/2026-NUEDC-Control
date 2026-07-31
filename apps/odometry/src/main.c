#include "ti_msp_dl_config.h"
#include "board.h"

/*
 * 陀螺仪yaw角调试子工程 — MSPM0G3507
 * 不断接收IMU yaw角数据，并且向debug串口传输
 * 串口格式参考esp8266_ti里的代码
 * IMU PA8/9  DEBUG PA10 TX @115200
 */
int main(void)
{
    uint32_t t0;

    SYSCFG_DL_init();
    Timebase_Init();
    UartDebug_Init();

    UartDebug_Write("\r\n[imu] yaw debug, keep STILL...\r\n");
    IMU601_Init();

    t0 = millis();
    while (!IMU601_DataReady() && (millis() - t0) < 3000u)
        IMU601_Poll();

    if (IMU601_DataReady())
        UartDebug_Write("[imu] IMU ready\r\n");
    else
        UartDebug_Write("[imu] IMU timeout (check PA8/PA9)\r\n");

    UartDebug_Write("[imu] running @10Hz, sending yaw\r\n");

    while (1) {
        IMU601_Poll();

        uint32_t now = millis();
        if ((now - t0) % 100 == 0) {
            float yaw = IMU601_Attitude.yaw;
            UartDebug_Printf("YAW=%.2f\r\n", yaw);
        }
    }
}

