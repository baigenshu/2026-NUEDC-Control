#include "ti_msp_dl_config.h"
#include "board.h"

int main(void)
{

    /* 系统初始化 */
    SYSCFG_DL_init();


    OLED_Init();
    OLED_Clear();

    OLED_ShowString(0,0,(u8*)"ready",8);
    uint8_t addr=MPU6050_GetID();
    OLED_ShowNum(30,0,addr,4,8);

    MPU6050_Init();
    MPU6050_Calibrate();

    while (1)
    {
        MPU6050_Calculate();
        OLED_ShowString(0,9,(u8*)"roll:",8);
        OLED_ShowFloat(41,9,roll,8,2,8);
        //OLED_ShowFloat(0,18,pitch,8,2,8);
        //OLED_ShowFloat(0,27,yaw,8,2,8);
    }
}