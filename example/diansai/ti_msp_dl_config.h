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
 *  DO NOT EDIT - This file is generated for the MSPM0G350X
 *  by the SysConfig tool.
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

/*
 *  ======== SYSCFG_DL_init ========
 *  Perform all required MSP DL initialization
 *
 *  This function should be called once at a point before any use of
 *  MSP DL.
 */


/* clang-format off */

#define POWER_STARTUP_DELAY                                                (16)



#define CPUCLK_FREQ                                                     80000000




/* Defines for I2C_MPU6050 */
#define I2C_MPU6050_INST                                                    I2C1
#define I2C_MPU6050_INST_IRQHandler                              I2C1_IRQHandler
#define I2C_MPU6050_INST_INT_IRQN                                  I2C1_INT_IRQn
#define I2C_MPU6050_BUS_SPEED_HZ                                          400000
#define GPIO_I2C_MPU6050_SDA_PORT                                          GPIOB
#define GPIO_I2C_MPU6050_SDA_PIN                                   DL_GPIO_PIN_3
#define GPIO_I2C_MPU6050_IOMUX_SDA                               (IOMUX_PINCM16)
#define GPIO_I2C_MPU6050_IOMUX_SDA_FUNC                IOMUX_PINCM16_PF_I2C1_SDA
#define GPIO_I2C_MPU6050_SCL_PORT                                          GPIOB
#define GPIO_I2C_MPU6050_SCL_PIN                                   DL_GPIO_PIN_2
#define GPIO_I2C_MPU6050_IOMUX_SCL                               (IOMUX_PINCM15)
#define GPIO_I2C_MPU6050_IOMUX_SCL_FUNC                IOMUX_PINCM15_PF_I2C1_SCL



/* Port definition for Pin Group GPIO_MPU6050 */
#define GPIO_MPU6050_PORT                                                (GPIOB)

/* Defines for PIN_INT: GPIOB.1 with pinCMx 13 on package pin 48 */
// pins affected by this interrupt request:["PIN_INT"]
#define GPIO_MPU6050_INT_IRQN                                   (GPIOB_INT_IRQn)
#define GPIO_MPU6050_INT_IIDX                   (DL_INTERRUPT_GROUP1_IIDX_GPIOB)
#define GPIO_MPU6050_PIN_INT_IIDX                            (DL_GPIO_IIDX_DIO1)
#define GPIO_MPU6050_PIN_INT_PIN                                 (DL_GPIO_PIN_1)
#define GPIO_MPU6050_PIN_INT_IOMUX                               (IOMUX_PINCM13)
/* Port definition for Pin Group GPIO_OLED */
#define GPIO_OLED_PORT                                                   (GPIOB)

/* Defines for DC: GPIOB.11 with pinCMx 28 on package pin 63 */
#define GPIO_OLED_DC_PIN                                        (DL_GPIO_PIN_11)
#define GPIO_OLED_DC_IOMUX                                       (IOMUX_PINCM28)
/* Defines for CS: GPIOB.14 with pinCMx 31 on package pin 2 */
#define GPIO_OLED_CS_PIN                                        (DL_GPIO_PIN_14)
#define GPIO_OLED_CS_IOMUX                                       (IOMUX_PINCM31)
/* Defines for RST: GPIOB.10 with pinCMx 27 on package pin 62 */
#define GPIO_OLED_RST_PIN                                       (DL_GPIO_PIN_10)
#define GPIO_OLED_RST_IOMUX                                      (IOMUX_PINCM27)
/* Defines for SCL_OLED: GPIOB.9 with pinCMx 26 on package pin 61 */
#define GPIO_OLED_SCL_OLED_PIN                                   (DL_GPIO_PIN_9)
#define GPIO_OLED_SCL_OLED_IOMUX                                 (IOMUX_PINCM26)
/* Defines for SDA_OLED: GPIOB.8 with pinCMx 25 on package pin 60 */
#define GPIO_OLED_SDA_OLED_PIN                                   (DL_GPIO_PIN_8)
#define GPIO_OLED_SDA_OLED_IOMUX                                 (IOMUX_PINCM25)
/* Defines for PIN_0: GPIOB.19 with pinCMx 45 on package pin 16 */
#define GPIO_GRAY_PIN_0_PORT                                             (GPIOB)
#define GPIO_GRAY_PIN_0_PIN                                     (DL_GPIO_PIN_19)
#define GPIO_GRAY_PIN_0_IOMUX                                    (IOMUX_PINCM45)
/* Defines for PIN_1: GPIOB.17 with pinCMx 43 on package pin 14 */
#define GPIO_GRAY_PIN_1_PORT                                             (GPIOB)
#define GPIO_GRAY_PIN_1_PIN                                     (DL_GPIO_PIN_17)
#define GPIO_GRAY_PIN_1_IOMUX                                    (IOMUX_PINCM43)
/* Defines for PIN_2: GPIOA.16 with pinCMx 38 on package pin 9 */
#define GPIO_GRAY_PIN_2_PORT                                             (GPIOA)
#define GPIO_GRAY_PIN_2_PIN                                     (DL_GPIO_PIN_16)
#define GPIO_GRAY_PIN_2_IOMUX                                    (IOMUX_PINCM38)
/* Defines for PIN_3: GPIOA.14 with pinCMx 36 on package pin 7 */
#define GPIO_GRAY_PIN_3_PORT                                             (GPIOA)
#define GPIO_GRAY_PIN_3_PIN                                     (DL_GPIO_PIN_14)
#define GPIO_GRAY_PIN_3_IOMUX                                    (IOMUX_PINCM36)
/* Defines for PIN_4: GPIOB.20 with pinCMx 48 on package pin 19 */
#define GPIO_GRAY_PIN_4_PORT                                             (GPIOB)
#define GPIO_GRAY_PIN_4_PIN                                     (DL_GPIO_PIN_20)
#define GPIO_GRAY_PIN_4_IOMUX                                    (IOMUX_PINCM48)
/* Defines for PIN_5: GPIOB.25 with pinCMx 56 on package pin 27 */
#define GPIO_GRAY_PIN_5_PORT                                             (GPIOB)
#define GPIO_GRAY_PIN_5_PIN                                     (DL_GPIO_PIN_25)
#define GPIO_GRAY_PIN_5_IOMUX                                    (IOMUX_PINCM56)
/* Defines for PIN_6: GPIOA.25 with pinCMx 55 on package pin 26 */
#define GPIO_GRAY_PIN_6_PORT                                             (GPIOA)
#define GPIO_GRAY_PIN_6_PIN                                     (DL_GPIO_PIN_25)
#define GPIO_GRAY_PIN_6_IOMUX                                    (IOMUX_PINCM55)
/* Defines for PIN_7: GPIOA.27 with pinCMx 60 on package pin 31 */
#define GPIO_GRAY_PIN_7_PORT                                             (GPIOA)
#define GPIO_GRAY_PIN_7_PIN                                     (DL_GPIO_PIN_27)
#define GPIO_GRAY_PIN_7_IOMUX                                    (IOMUX_PINCM60)



/* clang-format on */

void SYSCFG_DL_init(void);
void SYSCFG_DL_initPower(void);
void SYSCFG_DL_GPIO_init(void);
void SYSCFG_DL_SYSCTL_init(void);
void SYSCFG_DL_I2C_MPU6050_init(void);

void SYSCFG_DL_SYSTICK_init(void);


#ifdef __cplusplus
}
#endif

#endif /* ti_msp_dl_config_h */
