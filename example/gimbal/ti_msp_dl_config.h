/*
 * Copyright (c) 2023, Texas Instruments Incorporated - http://www.ti.com
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ============ ti_msp_dl_config.h =============
 *  Configured MSPM0 DriverLib module declarations
 *
 *  Gimbal project — dual DCC-100v3 stepper
 *  仅使用板子已引出脚:
 *    GPIOA: 0,1,7,8,9,12,13,14,15,16,17,22,24,25,26,27
 *    GPIOB: 2,3,6,7,16,17,20,24
 *
 *  Wiring:
 *    Stepper1: PA0(STEP=TIMA0_CCP0) PA1(DIR) PA7(DCY) PA8(SLP) PA9(RST)
 *    Stepper2: PA15(STEP=TIMA1_CCP0) PA13(DIR) PA14(DCY) PA12(SLP) PA16(RST)
 */
#ifndef ti_msp_dl_config_h
#define ti_msp_dl_config_h

#define CONFIG_MSPM0G350X
#define CONFIG_MSPM0G3507

#if defined(__ti_version__) || defined(__TI_COMPILER_VERSION__)
#define SYSCONFIG_WEAK __attribute__((weak))
#elif defined(__IAR_SYSTEMS_ICC__)
#define SYSCONFIG_WEAK __weak
#elif defined(__GNUC__)
#define SYSCONFIG_WEAK __attribute__((weak))
#endif

#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_core.h>

#ifdef __cplusplus
extern "C" {
#endif

/* clang-format off */

#define POWER_STARTUP_DELAY                                                (16)

#define CPUCLK_FREQ                                                     80000000

/* Defines for STEPPER1_PWM — TIMA0 @ PA0 (TIMA0_CCP0) */
#define STEPPER1_PWM_INST                                                  TIMA0
#define STEPPER1_PWM_INST_IRQHandler                            TIMA0_IRQHandler
#define STEPPER1_PWM_INST_INT_IRQN                              (TIMA0_INT_IRQn)
#define STEPPER1_PWM_INST_CLK_FREQ                                      80000000
#define GPIO_STEPPER1_PWM_C0_PORT                                          GPIOA
#define GPIO_STEPPER1_PWM_C0_PIN                                   DL_GPIO_PIN_0
#define GPIO_STEPPER1_PWM_C0_IOMUX                                (IOMUX_PINCM1)
#define GPIO_STEPPER1_PWM_C0_IOMUX_FUNC               IOMUX_PINCM1_PF_TIMA0_CCP0
#define GPIO_STEPPER1_PWM_C0_IDX                             DL_TIMER_CC_0_INDEX

/* Defines for STEPPER2_PWM — TIMA1 @ PA15 (TIMA1_CCP0) */
#define STEPPER2_PWM_INST                                                  TIMA1
#define STEPPER2_PWM_INST_IRQHandler                            TIMA1_IRQHandler
#define STEPPER2_PWM_INST_INT_IRQN                              (TIMA1_INT_IRQn)
#define STEPPER2_PWM_INST_CLK_FREQ                                      80000000
#define GPIO_STEPPER2_PWM_C0_PORT                                          GPIOA
#define GPIO_STEPPER2_PWM_C0_PIN                                  DL_GPIO_PIN_15
#define GPIO_STEPPER2_PWM_C0_IOMUX                               (IOMUX_PINCM37)
#define GPIO_STEPPER2_PWM_C0_IOMUX_FUNC              IOMUX_PINCM37_PF_TIMA1_CCP0
#define GPIO_STEPPER2_PWM_C0_IDX                             DL_TIMER_CC_0_INDEX

/* Port definition for Pin Group STEPPER1 */
#define STEPPER1_PORT                                                    (GPIOA)

/* DIR1: GPIOA.1  pinCMx 2 */
#define STEPPER1_DIR1_PIN                                        (DL_GPIO_PIN_1)
#define STEPPER1_DIR1_IOMUX                                       (IOMUX_PINCM2)
/* DCY1: GPIOA.7  pinCMx 14 */
#define STEPPER1_DCY1_PIN                                        (DL_GPIO_PIN_7)
#define STEPPER1_DCY1_IOMUX                                      (IOMUX_PINCM14)
/* SLP1: GPIOA.8  pinCMx 19 */
#define STEPPER1_SLP1_PIN                                        (DL_GPIO_PIN_8)
#define STEPPER1_SLP1_IOMUX                                      (IOMUX_PINCM19)
/* RST1: GPIOA.9  pinCMx 20 */
#define STEPPER1_RST1_PIN                                        (DL_GPIO_PIN_9)
#define STEPPER1_RST1_IOMUX                                      (IOMUX_PINCM20)

/* Port definition for Pin Group STEPPER2 */
#define STEPPER2_PORT                                                    (GPIOA)

/* DIR2: GPIOA.13 pinCMx 35 */
#define STEPPER2_DIR2_PIN                                       (DL_GPIO_PIN_13)
#define STEPPER2_DIR2_IOMUX                                      (IOMUX_PINCM35)
/* DCY2: GPIOA.14 pinCMx 36 */
#define STEPPER2_DCY2_PIN                                       (DL_GPIO_PIN_14)
#define STEPPER2_DCY2_IOMUX                                      (IOMUX_PINCM36)
/* SLP2: GPIOA.12 pinCMx 34 */
#define STEPPER2_SLP2_PIN                                       (DL_GPIO_PIN_12)
#define STEPPER2_SLP2_IOMUX                                      (IOMUX_PINCM34)
/* RST2: GPIOA.16 pinCMx 38 */
#define STEPPER2_RST2_PIN                                       (DL_GPIO_PIN_16)
#define STEPPER2_RST2_IOMUX                                      (IOMUX_PINCM38)

/* clang-format on */

void SYSCFG_DL_init(void);
void SYSCFG_DL_initPower(void);
void SYSCFG_DL_GPIO_init(void);
void SYSCFG_DL_SYSCTL_init(void);
void SYSCFG_DL_STEPPER1_PWM_init(void);
void SYSCFG_DL_STEPPER2_PWM_init(void);
void SYSCFG_DL_SYSTICK_init(void);

bool SYSCFG_DL_saveConfiguration(void);
bool SYSCFG_DL_restoreConfiguration(void);

#ifdef __cplusplus
}
#endif

#endif /* ti_msp_dl_config_h */
