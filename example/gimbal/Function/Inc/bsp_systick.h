#ifndef __BSP_SYSTICK_H
#define __BSP_SYSTICK_H

#include "ti_msp_dl_config.h"

/* Systick 最大计数值, 24 位 */
#define SysTickMAX_COUNT 0xFFFFFF

/* Systick 计数频率 (与 CPUCLK 一致, 80 MHz) */
#define SysTickFre 80000000

/* 将 systick 计数值转换为具体时间单位 */
#define SysTick_MS(x)  ((SysTickFre / 1000U) * (uint32_t)(x))
#define SysTick_US(x)  ((SysTickFre / 1000000U) * (uint32_t)(x))

uint32_t Systick_getTick(void);
void delay_ms(uint32_t ms);
void delay_us(uint32_t us);

#endif /* __BSP_SYSTICK_H */
