/**
 * @file tmc2209.h
 * @brief TMC2209 UART 电流 + 微步配置
 */
#ifndef TMC2209_H
#define TMC2209_H

#include <stdbool.h>

void TMC2209_Init(void);
bool TMC2209_SetCurrentCodes(unsigned ihold, unsigned irun);
bool TMC2209_SetMicrosteps(unsigned microsteps);

#endif /* TMC2209_H */