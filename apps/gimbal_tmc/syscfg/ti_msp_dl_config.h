/*
 * Copyright (c) 2023, Texas Instruments Incorporated - http://www.ti.com
 * All rights reserved.
 *
 *  ============ ti_msp_dl_config.h =============
 *  Configured MSPM0 DriverLib module declarations
 *  TMC2209 dual stepper (pin map aligned with apps/gimbal)
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


#define GPIO_HFXT_PORT                                                     GPIOA
#define GPIO_HFXIN_PIN                                             DL_GPIO_PIN_5
#define GPIO_HFXIN_IOMUX                                         (IOMUX_PINCM10)
#define GPIO_HFXOUT_PIN                                            DL_GPIO_PIN_6
#define GPIO_HFXOUT_IOMUX                                        (IOMUX_PINCM11)
#define CPUCLK_FREQ                                                     80000000



/* Defines for TMC_PWM1 — STEP1 @ PA0 (TIMA0) */
#define TMC_PWM1_INST                                                      TIMA0
#define TMC_PWM1_INST_IRQHandler                                TIMA0_IRQHandler
#define TMC_PWM1_INST_INT_IRQN                                  (TIMA0_INT_IRQn)
#define TMC_PWM1_INST_CLK_FREQ                                          10000000
/* GPIO defines for channel 0 */
#define GPIO_TMC_PWM1_C0_PORT                                              GPIOA
#define GPIO_TMC_PWM1_C0_PIN                                       DL_GPIO_PIN_0
#define GPIO_TMC_PWM1_C0_IOMUX                                    (IOMUX_PINCM1)
#define GPIO_TMC_PWM1_C0_IOMUX_FUNC                   IOMUX_PINCM1_PF_TIMA0_CCP0
#define GPIO_TMC_PWM1_C0_IDX                                 DL_TIMER_CC_0_INDEX

/* Defines for TMC_PWM2 — STEP2 @ PA12 (TIMG0) */
#define TMC_PWM2_INST                                                      TIMG0
#define TMC_PWM2_INST_IRQHandler                                TIMG0_IRQHandler
#define TMC_PWM2_INST_INT_IRQN                                  (TIMG0_INT_IRQn)
#define TMC_PWM2_INST_CLK_FREQ                                           5000000
/* GPIO defines for channel 0 */
#define GPIO_TMC_PWM2_C0_PORT                                              GPIOA
#define GPIO_TMC_PWM2_C0_PIN                                      DL_GPIO_PIN_12
#define GPIO_TMC_PWM2_C0_IOMUX                                   (IOMUX_PINCM34)
#define GPIO_TMC_PWM2_C0_IOMUX_FUNC                  IOMUX_PINCM34_PF_TIMG0_CCP0
#define GPIO_TMC_PWM2_C0_IDX                                 DL_TIMER_CC_0_INDEX


/* Defines for DEBUG */
#define DEBUG_INST                                                         UART1
#define DEBUG_INST_FREQUENCY                                            40000000
#define DEBUG_INST_IRQHandler                                   UART1_IRQHandler
#define DEBUG_INST_INT_IRQN                                       UART1_INT_IRQn
#define GPIO_DEBUG_RX_PORT                                                 GPIOB
#define GPIO_DEBUG_TX_PORT                                                 GPIOB
#define GPIO_DEBUG_RX_PIN                                          DL_GPIO_PIN_7
#define GPIO_DEBUG_TX_PIN                                          DL_GPIO_PIN_6
#define GPIO_DEBUG_IOMUX_RX                                      (IOMUX_PINCM24)
#define GPIO_DEBUG_IOMUX_TX                                      (IOMUX_PINCM23)
#define GPIO_DEBUG_IOMUX_RX_FUNC                       IOMUX_PINCM24_PF_UART1_RX
#define GPIO_DEBUG_IOMUX_TX_FUNC                       IOMUX_PINCM23_PF_UART1_TX
#define DEBUG_BAUD_RATE                                                 (115200)
#define DEBUG_IBRD_40_MHZ_115200_BAUD                                       (21)
#define DEBUG_FBRD_40_MHZ_115200_BAUD                                       (45)


/* Port definition for Pin Group TMC1 */
#define TMC1_PORT                                                        (GPIOA)

/* Defines for DIR1: GPIOA.1 */
#define TMC1_DIR1_PIN                                            (DL_GPIO_PIN_1)
#define TMC1_DIR1_IOMUX                                           (IOMUX_PINCM2)
/* Defines for EN1: GPIOA.7  (active low) */
#define TMC1_EN1_PIN                                             (DL_GPIO_PIN_7)
#define TMC1_EN1_IOMUX                                           (IOMUX_PINCM14)
/* Defines for MS1_1: GPIOA.8 */
#define TMC1_MS1_1_PIN                                           (DL_GPIO_PIN_8)
#define TMC1_MS1_1_IOMUX                                         (IOMUX_PINCM19)
/* Defines for MS2_1: GPIOA.9 */
#define TMC1_MS2_1_PIN                                           (DL_GPIO_PIN_9)
#define TMC1_MS2_1_IOMUX                                         (IOMUX_PINCM20)

/* Port definition for Pin Group TMC2 */
#define TMC2_PORT                                                        (GPIOA)

/* Defines for DIR2: GPIOA.13 */
#define TMC2_DIR2_PIN                                           (DL_GPIO_PIN_13)
#define TMC2_DIR2_IOMUX                                          (IOMUX_PINCM35)
/* Defines for EN2: GPIOA.14  (active low) */
#define TMC2_EN2_PIN                                            (DL_GPIO_PIN_14)
#define TMC2_EN2_IOMUX                                           (IOMUX_PINCM36)
/* Defines for MS1_2: GPIOA.15 */
#define TMC2_MS1_2_PIN                                          (DL_GPIO_PIN_15)
#define TMC2_MS1_2_IOMUX                                         (IOMUX_PINCM37)
/* Defines for MS2_2: GPIOA.16 */
#define TMC2_MS2_2_PIN                                          (DL_GPIO_PIN_16)
#define TMC2_MS2_2_IOMUX                                         (IOMUX_PINCM38)

/* clang-format on */

void SYSCFG_DL_init(void);
void SYSCFG_DL_initPower(void);
void SYSCFG_DL_GPIO_init(void);
void SYSCFG_DL_SYSCTL_init(void);
void SYSCFG_DL_TMC_PWM1_init(void);
void SYSCFG_DL_TMC_PWM2_init(void);
void SYSCFG_DL_DEBUG_init(void);

bool SYSCFG_DL_saveConfiguration(void);
bool SYSCFG_DL_restoreConfiguration(void);

#ifdef __cplusplus
}
#endif

#endif /* ti_msp_dl_config_h */
