/**
 * @file main.c
 * @brief balance 入口（仅硬件 + 步进驱动初始化；业务另接）
 */
#include "ti_msp_dl_config.h"
#include "stepper.h"

int main(void)
{
    SYSCFG_DL_init();
    Stepper_Init();
    /* 默认禁用线圈，业务层使能后再运动 */
    Stepper_SetEnable(false);

    while (1) {
        __WFI();
    }
}