#ifndef __TIMEBASE_H
#define __TIMEBASE_H

#include <stdint.h>

/* Millisecond timebase based on SysTick free-running counter (80 MHz). */
void Timebase_Init(void);

/* Monotonic milliseconds since boot (wraps ~49 days). */
uint32_t millis(void);

/* Seconds elapsed since last call (handles single SysTick period wrap). */
float Timebase_GetDeltaSec(void);

#endif
