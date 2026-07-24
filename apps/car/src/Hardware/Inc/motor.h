#ifndef __MOTOR_H
#define __MOTOR_H

#include <stdint.h>

#define MOTOR_PWM_PERIOD  4000U

void Motor_Init(void);
void Motor_Standby(void);
void Motor_AllStop(void);
void Motor_AllBrake(void);
void Motor_SetPWM(int16_t m1, int16_t m2, int16_t m3, int16_t m4);

/* A: PA12 TIMG0  PB13/15 */
void MotorA_Init(void);
void MotorA_SetSpeed(int16_t speed);
void MotorA_Forward(uint16_t speed);
void MotorA_Reverse(uint16_t speed);
void MotorA_Brake(void);
void MotorA_Stop(void);

/* B: PA21 TIMG6  PB4/6 */
void MotorB_Init(void);
void MotorB_SetSpeed(int16_t speed);
void MotorB_Forward(uint16_t speed);
void MotorB_Reverse(uint16_t speed);
void MotorB_Brake(void);
void MotorB_Stop(void);

/* C: PA13 TIMA0_CC3_CMPL  PB1/2 */
void MotorC_Init(void);
void MotorC_SetSpeed(int16_t speed);
void MotorC_Forward(uint16_t speed);
void MotorC_Reverse(uint16_t speed);
void MotorC_Brake(void);
void MotorC_Stop(void);

/* D: PA22 TIMA0_CC1  PB3/7 */
void MotorD_Init(void);
void MotorD_SetSpeed(int16_t speed);
void MotorD_Forward(uint16_t speed);
void MotorD_Reverse(uint16_t speed);
void MotorD_Brake(void);
void MotorD_Stop(void);

#define Motor_SetSpeed  MotorA_SetSpeed
#define Motor_Forward   MotorA_Forward
#define Motor_Reverse   MotorA_Reverse
#define Motor_Brake     MotorA_Brake
#define Motor_Stop      MotorA_Stop

#endif
