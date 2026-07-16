#ifndef STEP_MOTOR_H
#define STEP_MOTOR_H

// 接线
// 第二路
// PA12 PWM
// PA13 DIR
// PA14 DCY
// PA15 SLP
// PA16 RST

#include "ti_msp_dl_config.h"

void step_motor_init(void);
void step_motor_dir_set(uint8_t direction, uint8_t stepper_id);
void step_motor_step_set(uint8_t step, uint8_t stepper_id);

#endif // STEP_MOTOR_H
