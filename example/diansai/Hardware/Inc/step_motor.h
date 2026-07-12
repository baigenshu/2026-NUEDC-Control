#ifndef __STEP_MOTOR_H
#define __STEP_MOTOR_H

#include "ti_msp_dl_config.h"
#include <stdint.h>

/* Stepper IDs */
#define STEPPER_1   1
#define STEPPER_2   2

/* Direction */
#define DIR_CW   0
#define DIR_CCW  1

/*
 * Stepper physical params (DCC-100v3 default: 6400 pulse/rev)
 *   deg_per_pulse = 360.0 / 6400 = 0.05625 deg
 */
#define STEPPER_DEG_PER_PULSE  0.05625f
#define STEPPER_PULSE_PER_REV  6400

/* ---- API ---- */

/* Init both steppers: enable NVIC, GPIO already done by SYSCFG_DL */
void Stepper_Init(void);

/* Set direction */
void Stepper_SetDir(uint8_t id, uint8_t dir);

/* Set angular speed (deg/s). 0 = stop. Max ~360 deg/s */
void Stepper_SetSpeed(uint8_t id, uint16_t deg_per_sec);

/* Start PWM output (non-blocking) */
void Stepper_Start(uint8_t id);

/* Stop PWM output immediately */
void Stepper_Stop(uint8_t id);

/* Run N pulses then auto-stop (non-blocking).
 * After calling this, poll Stepper_IsDone() or just wait. */
void Stepper_SetPulses(uint8_t id, uint32_t pulses);

/* Run to a specific angle then auto-stop (non-blocking) */
void Stepper_SetAngle(uint8_t id, uint16_t angle_deg);

/* Check if stepper has finished moving */
uint8_t Stepper_IsDone(uint8_t id);

/* Blocking: run N pulses at given speed */
void Stepper_RunPulses(uint8_t id, uint32_t pulses, uint8_t dir, uint16_t deg_per_sec);

/* Blocking: turn to angle at given speed */
void Stepper_RunAngle(uint8_t id, uint16_t angle_deg, uint8_t dir, uint16_t deg_per_sec);

/* SLP pin control */
void Stepper_Enable(uint8_t id);
void Stepper_Disable(uint8_t id);

/* RST pulse */
void Stepper_Reset(uint8_t id);

#endif /* __STEP_MOTOR_H */
