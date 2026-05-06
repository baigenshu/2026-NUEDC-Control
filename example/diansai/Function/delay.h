#ifndef __DELAY_H
#define __DELAY_H

#include <stdint.h>

void Delay_us(uint32_t us);
void Delay_ms(uint32_t ms);
void Delay_s(uint32_t s);
void SysTick_Init(void);           // 初始化 SysTick 为 1ms 中断模式

extern volatile uint32_t g_sysTick;  // 毫秒计数器

#endif