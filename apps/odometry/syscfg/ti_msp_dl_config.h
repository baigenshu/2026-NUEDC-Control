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



/* Defines for ODOM_TIM */
#define ODOM_TIM_INST                                                    (TIMG0)
#define ODOM_TIM_INST_IRQHandler                                TIMG0_IRQHandler
#define ODOM_TIM_INST_INT_IRQN                                  (TIMG0_INT_IRQn)
#define ODOM_TIM_INST_LOAD_VALUE                                         (9999U)



/* Defines for DEBUG_UART */
#define DEBUG_UART_INST                                                    UART0
#define DEBUG_UART_INST_FREQUENCY                                       40000000
#define DEBUG_UART_INST_IRQHandler                              UART0_IRQHandler
#define DEBUG_UART_INST_INT_IRQN                                  UART0_INT_IRQn
#define GPIO_DEBUG_UART_RX_PORT                                            GPIOA
#define GPIO_DEBUG_UART_TX_PORT                                            GPIOA
#define GPIO_DEBUG_UART_RX_PIN                                    DL_GPIO_PIN_11
#define GPIO_DEBUG_UART_TX_PIN                                    DL_GPIO_PIN_10
#define GPIO_DEBUG_UART_IOMUX_RX                                 (IOMUX_PINCM22)
#define GPIO_DEBUG_UART_IOMUX_TX                                 (IOMUX_PINCM21)
#define GPIO_DEBUG_UART_IOMUX_RX_FUNC                  IOMUX_PINCM22_PF_UART0_RX
#define GPIO_DEBUG_UART_IOMUX_TX_FUNC                  IOMUX_PINCM21_PF_UART0_TX
#define DEBUG_UART_BAUD_RATE                                            (115200)
#define DEBUG_UART_IBRD_40_MHZ_115200_BAUD                                  (21)
#define DEBUG_UART_FBRD_40_MHZ_115200_BAUD                                  (45)
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





/* Defines for DMA_CH0 */
#define DMA_CH0_CHAN_ID                                                      (0)
#define IMU601_INST_DMA_TRIGGER                              (DMA_UART1_RX_TRIG)


/* Port definition for Pin Group GPIO_ENCODERA */
#define GPIO_ENCODERA_PORT                                               (GPIOB)

/* Defines for E1A: GPIOB.6 with pinCMx 23 on package pin 20 */
// groups represented: ["GPIO_ENCODERB","GPIO_ENCODERA"]
// pins affected: ["E2A","E2B","E1A","E1B"]
#define GPIO_MULTIPLE_GPIOB_INT_IRQN                            (GPIOB_INT_IRQn)
#define GPIO_MULTIPLE_GPIOB_INT_IIDX            (DL_INTERRUPT_GROUP1_IIDX_GPIOB)
#define GPIO_ENCODERA_E1A_IIDX                               (DL_GPIO_IIDX_DIO6)
#define GPIO_ENCODERA_E1A_PIN                                    (DL_GPIO_PIN_6)
#define GPIO_ENCODERA_E1A_IOMUX                                  (IOMUX_PINCM23)
/* Defines for E1B: GPIOB.7 with pinCMx 24 on package pin 21 */
#define GPIO_ENCODERA_E1B_IIDX                               (DL_GPIO_IIDX_DIO7)
#define GPIO_ENCODERA_E1B_PIN                                    (DL_GPIO_PIN_7)
#define GPIO_ENCODERA_E1B_IOMUX                                  (IOMUX_PINCM24)
/* Port definition for Pin Group GPIO_ENCODERB */
#define GPIO_ENCODERB_PORT                                               (GPIOB)

/* Defines for E2A: GPIOB.15 with pinCMx 32 on package pin 25 */
#define GPIO_ENCODERB_E2A_IIDX                              (DL_GPIO_IIDX_DIO15)
#define GPIO_ENCODERB_E2A_PIN                                   (DL_GPIO_PIN_15)
#define GPIO_ENCODERB_E2A_IOMUX                                  (IOMUX_PINCM32)
/* Defines for E2B: GPIOB.16 with pinCMx 33 on package pin 26 */
#define GPIO_ENCODERB_E2B_IIDX                              (DL_GPIO_IIDX_DIO16)
#define GPIO_ENCODERB_E2B_PIN                                   (DL_GPIO_PIN_16)
#define GPIO_ENCODERB_E2B_IOMUX                                  (IOMUX_PINCM33)



/* clang-format on */

void SYSCFG_DL_init(void);
void SYSCFG_DL_initPower(void);
void SYSCFG_DL_GPIO_init(void);
void SYSCFG_DL_SYSCTL_init(void);
void SYSCFG_DL_ODOM_TIM_init(void);
void SYSCFG_DL_DEBUG_UART_init(void);
void SYSCFG_DL_IMU601_init(void);
void SYSCFG_DL_DMA_init(void);

void SYSCFG_DL_SYSTICK_init(void);

bool SYSCFG_DL_saveConfiguration(void);
bool SYSCFG_DL_restoreConfiguration(void);

#ifdef __cplusplus
}
#endif

#endif /* ti_msp_dl_config_h */
