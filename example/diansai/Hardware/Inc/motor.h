#ifndef __MOTOR_H
#define __MOTOR_H

#include <stdint.h>

/* PWM period from SysConfig (4000) */
#define MOTOR_PWM_PERIOD    4000

/* ---- Motor A (PWMA = TIMG0/PA12, AIN1=PB13, AIN2=PB15) ---- */
void MotorA_Init(void);
void MotorA_SetSpeed(int16_t speed);
void MotorA_Forward(uint16_t speed);
void MotorA_Reverse(uint16_t speed);
void MotorA_Brake(void);
void MotorA_Stop(void);

/* ---- Motor B (PWMB = TIMG6/PA21, BIN1=PB4, BIN2=PB6) ---- */
void MotorB_Init(void);
void MotorB_SetSpeed(int16_t speed);
void MotorB_Forward(uint16_t speed);
void MotorB_Reverse(uint16_t speed);
void MotorB_Brake(void);
void MotorB_Stop(void);

/* ---- Common ---- */
void Motor_Init(void);       /* init both A and B, enable STBY */
void Motor_Standby(void);    /* STBY low → all outputs off */

/* Backward-compatible aliases */
#define Motor_SetSpeed  MotorA_SetSpeed
#define Motor_Forward   MotorA_Forward
#define Motor_Reverse   MotorA_Reverse
#define Motor_Brake     MotorA_Brake
#define Motor_Stop      MotorA_Stop

#endif /* __MOTOR_H */