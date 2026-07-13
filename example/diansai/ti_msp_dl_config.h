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



/* Defines for PWMA */
#define PWMA_INST                                                          TIMG0
#define PWMA_INST_IRQHandler                                    TIMG0_IRQHandler
#define PWMA_INST_INT_IRQN                                      (TIMG0_INT_IRQn)
#define PWMA_INST_CLK_FREQ                                              40000000
/* GPIO defines for channel 0 */
#define GPIO_PWMA_C0_PORT                                                  GPIOA
#define GPIO_PWMA_C0_PIN                                          DL_GPIO_PIN_12
#define GPIO_PWMA_C0_IOMUX                                       (IOMUX_PINCM34)
#define GPIO_PWMA_C0_IOMUX_FUNC                      IOMUX_PINCM34_PF_TIMG0_CCP0
#define GPIO_PWMA_C0_IDX                                     DL_TIMER_CC_0_INDEX

/* Defines for PWMB */
#define PWMB_INST                                                          TIMG6
#define PWMB_INST_IRQHandler                                    TIMG6_IRQHandler
#define PWMB_INST_INT_IRQN                                      (TIMG6_INT_IRQn)
#define PWMB_INST_CLK_FREQ                                              40000000
/* GPIO defines for channel 0 */
#define GPIO_PWMB_C0_PORT                                                  GPIOA
#define GPIO_PWMB_C0_PIN                                          DL_GPIO_PIN_21
#define GPIO_PWMB_C0_IOMUX                                       (IOMUX_PINCM46)
#define GPIO_PWMB_C0_IOMUX_FUNC                      IOMUX_PINCM46_PF_TIMG6_CCP0
#define GPIO_PWMB_C0_IDX                                     DL_TIMER_CC_0_INDEX

/* Defines for STEPPER2_PWM */
#define STEPPER2_PWM_INST                                                  TIMA1
#define STEPPER2_PWM_INST_IRQHandler                            TIMA1_IRQHandler
#define STEPPER2_PWM_INST_INT_IRQN                              (TIMA1_INT_IRQn)
#define STEPPER2_PWM_INST_CLK_FREQ                                      80000000
/* GPIO defines for channel 0 */
#define GPIO_STEPPER2_PWM_C0_PORT                                          GPIOA
#define GPIO_STEPPER2_PWM_C0_PIN                                  DL_GPIO_PIN_10
#define GPIO_STEPPER2_PWM_C0_IOMUX                               (IOMUX_PINCM21)
#define GPIO_STEPPER2_PWM_C0_IOMUX_FUNC              IOMUX_PINCM21_PF_TIMA1_CCP0
#define GPIO_STEPPER2_PWM_C0_IDX                             DL_TIMER_CC_0_INDEX

/* Defines for STEPPER1_PWM */
#define STEPPER1_PWM_INST                                                  TIMA0
#define STEPPER1_PWM_INST_IRQHandler                            TIMA0_IRQHandler
#define STEPPER1_PWM_INST_INT_IRQN                              (TIMA0_INT_IRQn)
#define STEPPER1_PWM_INST_CLK_FREQ                                      80000000
/* GPIO defines for channel 0 */
#define GPIO_STEPPER1_PWM_C0_PORT                                          GPIOA
#define GPIO_STEPPER1_PWM_C0_PIN                                   DL_GPIO_PIN_0
#define GPIO_STEPPER1_PWM_C0_IOMUX                                (IOMUX_PINCM1)
#define GPIO_STEPPER1_PWM_C0_IOMUX_FUNC               IOMUX_PINCM1_PF_TIMA0_CCP0
#define GPIO_STEPPER1_PWM_C0_IDX                             DL_TIMER_CC_0_INDEX




/* Defines for I2C */
#define I2C_INST                                                            I2C1
#define I2C_INST_IRQHandler                                      I2C1_IRQHandler
#define I2C_INST_INT_IRQN                                          I2C1_INT_IRQn
#define I2C_BUS_SPEED_HZ                                                  400000
#define GPIO_I2C_SDA_PORT                                                  GPIOB
#define GPIO_I2C_SDA_PIN                                           DL_GPIO_PIN_3
#define GPIO_I2C_IOMUX_SDA                                       (IOMUX_PINCM16)
#define GPIO_I2C_IOMUX_SDA_FUNC                        IOMUX_PINCM16_PF_I2C1_SDA
#define GPIO_I2C_SCL_PORT                                                  GPIOB
#define GPIO_I2C_SCL_PIN                                           DL_GPIO_PIN_2
#define GPIO_I2C_IOMUX_SCL                                       (IOMUX_PINCM15)
#define GPIO_I2C_IOMUX_SCL_FUNC                        IOMUX_PINCM15_PF_I2C1_SCL


/* Defines for IMU601 */
#define IMU601_INST                                                        UART1
#define IMU601_INST_FREQUENCY                                           40000000
#define IMU601_INST_IRQHandler                                  UART1_IRQHandler
#define IMU601_INST_INT_IRQN                                      UART1_INT_IRQn
#define GPIO_IMU601_RX_PORT                                                GPIOA
#define GPIO_IMU601_TX_PORT                                                GPIOA
#define GPIO_IMU601_RX_PIN                                         DL_GPIO_PIN_9
#define GPIO_IMU601_TX_PIN                                         DL_GPIO_PIN_8
#define GPIO_IMU601_IOMUX_RX                                     (IOMUX_PINCM20)
#define GPIO_IMU601_IOMUX_TX                                     (IOMUX_PINCM19)
#define GPIO_IMU601_IOMUX_RX_FUNC                      IOMUX_PINCM20_PF_UART1_RX
#define GPIO_IMU601_IOMUX_TX_FUNC                      IOMUX_PINCM19_PF_UART1_TX
#define IMU601_BAUD_RATE                                                (115200)
#define IMU601_IBRD_40_MHZ_115200_BAUD                                      (21)
#define IMU601_FBRD_40_MHZ_115200_BAUD                                      (45)




/* Defines for SPI_OLED */
#define SPI_OLED_INST                                                      SPI1
#define SPI_OLED_INST_IRQHandler                                SPI1_IRQHandler
#define SPI_OLED_INST_INT_IRQN                                    SPI1_INT_IRQn
#define GPIO_SPI_OLED_PICO_PORT                                           GPIOB
#define GPIO_SPI_OLED_PICO_PIN                                    DL_GPIO_PIN_8
#define GPIO_SPI_OLED_IOMUX_PICO                                (IOMUX_PINCM25)
#define GPIO_SPI_OLED_IOMUX_PICO_FUNC                IOMUX_PINCM25_PF_SPI1_PICO
#define GPIO_SPI_OLED_POCI_PORT                                           GPIOB
#define GPIO_SPI_OLED_POCI_PIN                                   DL_GPIO_PIN_21
#define GPIO_SPI_OLED_IOMUX_POCI                                (IOMUX_PINCM49)
#define GPIO_SPI_OLED_IOMUX_POCI_FUNC                IOMUX_PINCM49_PF_SPI1_POCI
/* GPIO configuration for SPI_OLED */
#define GPIO_SPI_OLED_SCLK_PORT                                           GPIOB
#define GPIO_SPI_OLED_SCLK_PIN                                    DL_GPIO_PIN_9
#define GPIO_SPI_OLED_IOMUX_SCLK                                (IOMUX_PINCM26)
#define GPIO_SPI_OLED_IOMUX_SCLK_FUNC                IOMUX_PINCM26_PF_SPI1_SCLK



/* Port definition for Pin Group SPI_OLED_CTRL */
#define SPI_OLED_CTRL_PORT                                               (GPIOB)

/* Defines for CS: GPIOB.14 with pinCMx 31 on package pin 2 */
#define SPI_OLED_CTRL_CS_PIN                                    (DL_GPIO_PIN_14)
#define SPI_OLED_CTRL_CS_IOMUX                                   (IOMUX_PINCM31)
/* Defines for DC: GPIOB.11 with pinCMx 28 on package pin 63 */
#define SPI_OLED_CTRL_DC_PIN                                    (DL_GPIO_PIN_11)
#define SPI_OLED_CTRL_DC_IOMUX                                   (IOMUX_PINCM28)
/* Defines for RES: GPIOB.10 with pinCMx 27 on package pin 62 */
#define SPI_OLED_CTRL_RES_PIN                                   (DL_GPIO_PIN_10)
#define SPI_OLED_CTRL_RES_IOMUX                                  (IOMUX_PINCM27)
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
/* Port definition for Pin Group GPIO_MOTOR */
#define GPIO_MOTOR_PORT                                                  (GPIOB)

/* Defines for AIN1: GPIOB.13 with pinCMx 30 on package pin 1 */
#define GPIO_MOTOR_AIN1_PIN                                     (DL_GPIO_PIN_13)
#define GPIO_MOTOR_AIN1_IOMUX                                    (IOMUX_PINCM30)
/* Defines for AIN2: GPIOB.15 with pinCMx 32 on package pin 3 */
#define GPIO_MOTOR_AIN2_PIN                                     (DL_GPIO_PIN_15)
#define GPIO_MOTOR_AIN2_IOMUX                                    (IOMUX_PINCM32)
/* Defines for STBY: GPIOB.16 with pinCMx 33 on package pin 4 */
#define GPIO_MOTOR_STBY_PIN                                     (DL_GPIO_PIN_16)
#define GPIO_MOTOR_STBY_IOMUX                                    (IOMUX_PINCM33)
/* Defines for BIN1: GPIOB.4 with pinCMx 17 on package pin 52 */
#define GPIO_MOTOR_BIN1_PIN                                      (DL_GPIO_PIN_4)
#define GPIO_MOTOR_BIN1_IOMUX                                    (IOMUX_PINCM17)
/* Defines for BIN2: GPIOB.6 with pinCMx 23 on package pin 58 */
#define GPIO_MOTOR_BIN2_PIN                                      (DL_GPIO_PIN_6)
#define GPIO_MOTOR_BIN2_IOMUX                                    (IOMUX_PINCM23)
/* Port definition for Pin Group GPIO_ENCODERA */
#define GPIO_ENCODERA_PORT                                               (GPIOB)

/* Defines for E1A: GPIOB.0 with pinCMx 12 on package pin 47 */
// groups represented: ["GPIO_ENCODERB","GPIO_ENCODERA"]
// pins affected: ["E2A","E2B","E1A","E1B"]
#define GPIO_MULTIPLE_GPIOB_INT_IRQN                            (GPIOB_INT_IRQn)
#define GPIO_MULTIPLE_GPIOB_INT_IIDX            (DL_INTERRUPT_GROUP1_IIDX_GPIOB)
#define GPIO_ENCODERA_E1A_IIDX                               (DL_GPIO_IIDX_DIO0)
#define GPIO_ENCODERA_E1A_PIN                                    (DL_GPIO_PIN_0)
#define GPIO_ENCODERA_E1A_IOMUX                                  (IOMUX_PINCM12)
/* Defines for E1B: GPIOB.5 with pinCMx 18 on package pin 53 */
#define GPIO_ENCODERA_E1B_IIDX                               (DL_GPIO_IIDX_DIO5)
#define GPIO_ENCODERA_E1B_PIN                                    (DL_GPIO_PIN_5)
#define GPIO_ENCODERA_E1B_IOMUX                                  (IOMUX_PINCM18)
/* Port definition for Pin Group GPIO_ENCODERB */
#define GPIO_ENCODERB_PORT                                               (GPIOB)

/* Defines for E2A: GPIOB.12 with pinCMx 29 on package pin 64 */
#define GPIO_ENCODERB_E2A_IIDX                              (DL_GPIO_IIDX_DIO12)
#define GPIO_ENCODERB_E2A_PIN                                   (DL_GPIO_PIN_12)
#define GPIO_ENCODERB_E2A_IOMUX                                  (IOMUX_PINCM29)
/* Defines for E2B: GPIOB.18 with pinCMx 44 on package pin 15 */
#define GPIO_ENCODERB_E2B_IIDX                              (DL_GPIO_IIDX_DIO18)
#define GPIO_ENCODERB_E2B_PIN                                   (DL_GPIO_PIN_18)
#define GPIO_ENCODERB_E2B_IOMUX                                  (IOMUX_PINCM44)
/* Port definition for Pin Group STEPPER1 */
#define STEPPER1_PORT                                                    (GPIOA)

/* Defines for DIR1: GPIOA.6 with pinCMx 11 on package pin 46 */
#define STEPPER1_DIR1_PIN                                        (DL_GPIO_PIN_6)
#define STEPPER1_DIR1_IOMUX                                      (IOMUX_PINCM11)
/* Defines for DCY1: GPIOA.7 with pinCMx 14 on package pin 49 */
#define STEPPER1_DCY1_PIN                                        (DL_GPIO_PIN_7)
#define STEPPER1_DCY1_IOMUX                                      (IOMUX_PINCM14)
/* Defines for SLP1: GPIOA.13 with pinCMx 35 on package pin 6 */
#define STEPPER1_SLP1_PIN                                       (DL_GPIO_PIN_13)
#define STEPPER1_SLP1_IOMUX                                      (IOMUX_PINCM35)
/* Defines for RST1: GPIOA.15 with pinCMx 37 on package pin 8 */
#define STEPPER1_RST1_PIN                                       (DL_GPIO_PIN_15)
#define STEPPER1_RST1_IOMUX                                      (IOMUX_PINCM37)
/* Port definition for Pin Group STEPPER2 */
#define STEPPER2_PORT                                                    (GPIOA)

/* Defines for DIR2: GPIOA.23 with pinCMx 53 on package pin 24 */
#define STEPPER2_DIR2_PIN                                       (DL_GPIO_PIN_23)
#define STEPPER2_DIR2_IOMUX                                      (IOMUX_PINCM53)
/* Defines for DCY2: GPIOA.24 with pinCMx 54 on package pin 25 */
#define STEPPER2_DCY2_PIN                                       (DL_GPIO_PIN_24)
#define STEPPER2_DCY2_IOMUX                                      (IOMUX_PINCM54)
/* Defines for SLP2: GPIOA.26 with pinCMx 59 on package pin 30 */
#define STEPPER2_SLP2_PIN                                       (DL_GPIO_PIN_26)
#define STEPPER2_SLP2_IOMUX                                      (IOMUX_PINCM59)
/* Defines for RST2: GPIOA.28 with pinCMx 3 on package pin 35 */
#define STEPPER2_RST2_PIN                                       (DL_GPIO_PIN_28)
#define STEPPER2_RST2_IOMUX                                       (IOMUX_PINCM3)



/* clang-format on */

void SYSCFG_DL_init(void);
void SYSCFG_DL_initPower(void);
void SYSCFG_DL_GPIO_init(void);
void SYSCFG_DL_SYSCTL_init(void);
void SYSCFG_DL_PWMA_init(void);
void SYSCFG_DL_PWMB_init(void);
void SYSCFG_DL_STEPPER2_PWM_init(void);
void SYSCFG_DL_STEPPER1_PWM_init(void);
void SYSCFG_DL_I2C_init(void);
void SYSCFG_DL_IMU601_init(void);
void SYSCFG_DL_SPI_OLED_init(void);

void SYSCFG_DL_SYSTICK_init(void);

bool SYSCFG_DL_saveConfiguration(void);
bool SYSCFG_DL_restoreConfiguration(void);

#ifdef __cplusplus
}
#endif

#endif /* ti_msp_dl_config_h */
