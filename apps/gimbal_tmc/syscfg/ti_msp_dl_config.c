/*
 * Copyright (c) 2023, Texas Instruments Incorporated
 * All rights reserved.
 *
 *  ============ ti_msp_dl_config.c =============
 *  TMC2209 dual stepper — pin map aligned with apps/gimbal
 */

#include "ti_msp_dl_config.h"

DL_TimerA_backupConfig gTMC_PWM1Backup;

SYSCONFIG_WEAK void SYSCFG_DL_init(void)
{
    SYSCFG_DL_initPower();
    SYSCFG_DL_GPIO_init();
    SYSCFG_DL_SYSCTL_init();
    SYSCFG_DL_TMC_PWM1_init();
    SYSCFG_DL_TMC_PWM2_init();
    SYSCFG_DL_DEBUG_init();
    gTMC_PWM1Backup.backupRdy = false;
}

SYSCONFIG_WEAK bool SYSCFG_DL_saveConfiguration(void)
{
    bool retStatus = true;
    retStatus &= DL_TimerA_saveConfiguration(TMC_PWM1_INST, &gTMC_PWM1Backup);
    return retStatus;
}

SYSCONFIG_WEAK bool SYSCFG_DL_restoreConfiguration(void)
{
    bool retStatus = true;
    retStatus &= DL_TimerA_restoreConfiguration(TMC_PWM1_INST, &gTMC_PWM1Backup, false);
    return retStatus;
}

SYSCONFIG_WEAK void SYSCFG_DL_initPower(void)
{
    DL_GPIO_reset(GPIOA);
    DL_GPIO_reset(GPIOB);
    DL_TimerA_reset(TMC_PWM1_INST);
    DL_TimerG_reset(TMC_PWM2_INST);
    DL_UART_Main_reset(DEBUG_INST);

    DL_GPIO_enablePower(GPIOA);
    DL_GPIO_enablePower(GPIOB);
    DL_TimerA_enablePower(TMC_PWM1_INST);
    DL_TimerG_enablePower(TMC_PWM2_INST);
    DL_UART_Main_enablePower(DEBUG_INST);
    delay_cycles(POWER_STARTUP_DELAY);
}

SYSCONFIG_WEAK void SYSCFG_DL_GPIO_init(void)
{
    DL_GPIO_initPeripheralAnalogFunction(GPIO_HFXIN_IOMUX);
    DL_GPIO_initPeripheralAnalogFunction(GPIO_HFXOUT_IOMUX);

    DL_GPIO_initPeripheralOutputFunction(GPIO_TMC_PWM1_C0_IOMUX, GPIO_TMC_PWM1_C0_IOMUX_FUNC);
    DL_GPIO_enableOutput(GPIO_TMC_PWM1_C0_PORT, GPIO_TMC_PWM1_C0_PIN);
    DL_GPIO_initPeripheralOutputFunction(GPIO_TMC_PWM2_C0_IOMUX, GPIO_TMC_PWM2_C0_IOMUX_FUNC);
    DL_GPIO_enableOutput(GPIO_TMC_PWM2_C0_PORT, GPIO_TMC_PWM2_C0_PIN);

    DL_GPIO_initPeripheralOutputFunction(GPIO_DEBUG_IOMUX_TX, GPIO_DEBUG_IOMUX_TX_FUNC);
    DL_GPIO_initPeripheralInputFunction(GPIO_DEBUG_IOMUX_RX, GPIO_DEBUG_IOMUX_RX_FUNC);

    DL_GPIO_initDigitalOutput(TMC1_DIR1_IOMUX);
    DL_GPIO_initDigitalOutput(TMC1_EN1_IOMUX);
    DL_GPIO_initDigitalOutput(TMC1_MS1_1_IOMUX);
    DL_GPIO_initDigitalOutput(TMC1_MS2_1_IOMUX);

    DL_GPIO_initDigitalOutput(TMC2_DIR2_IOMUX);
    DL_GPIO_initDigitalOutput(TMC2_EN2_IOMUX);
    DL_GPIO_initDigitalOutput(TMC2_MS1_2_IOMUX);
    DL_GPIO_initDigitalOutput(TMC2_MS2_2_IOMUX);

    /* EN=1 (disable driver) until step_motor_init; MS/DIR low */
    DL_GPIO_clearPins(GPIOA,
                      TMC1_DIR1_PIN | TMC1_MS1_1_PIN | TMC1_MS2_1_PIN |
                      TMC2_DIR2_PIN | TMC2_MS1_2_PIN | TMC2_MS2_2_PIN);
    DL_GPIO_setPins(GPIOA, TMC1_EN1_PIN | TMC2_EN2_PIN);
    DL_GPIO_enableOutput(GPIOA,
                         TMC1_DIR1_PIN | TMC1_EN1_PIN | TMC1_MS1_1_PIN | TMC1_MS2_1_PIN |
                         TMC2_DIR2_PIN | TMC2_EN2_PIN | TMC2_MS1_2_PIN | TMC2_MS2_2_PIN);
}

static const DL_SYSCTL_SYSPLLConfig gSYSPLLConfig = {
    .inputFreq   = DL_SYSCTL_SYSPLL_INPUT_FREQ_32_48_MHZ,
    .rDivClk2x   = 1,
    .rDivClk1    = 0,
    .rDivClk0    = 0,
    .enableCLK2x = DL_SYSCTL_SYSPLL_CLK2X_DISABLE,
    .enableCLK1  = DL_SYSCTL_SYSPLL_CLK1_DISABLE,
    .enableCLK0  = DL_SYSCTL_SYSPLL_CLK0_ENABLE,
    .sysPLLMCLK  = DL_SYSCTL_SYSPLL_MCLK_CLK0,
    .sysPLLRef   = DL_SYSCTL_SYSPLL_REF_SYSOSC,
    .qDiv        = 4,
    .pDiv        = DL_SYSCTL_SYSPLL_PDIV_1
};

SYSCONFIG_WEAK void SYSCFG_DL_SYSCTL_init(void)
{
    DL_SYSCTL_setBORThreshold(DL_SYSCTL_BOR_THRESHOLD_LEVEL_0);
    DL_SYSCTL_setFlashWaitState(DL_SYSCTL_FLASH_WAIT_STATE_2);

    DL_SYSCTL_setSYSOSCFreq(DL_SYSCTL_SYSOSC_FREQ_BASE);
    DL_SYSCTL_disableHFXT();
    DL_SYSCTL_disableSYSPLL();
    DL_SYSCTL_setHFCLKSourceHFXTParams(DL_SYSCTL_HFXT_RANGE_32_48_MHZ, 0, false);
    DL_SYSCTL_configSYSPLL((DL_SYSCTL_SYSPLLConfig *)&gSYSPLLConfig);
    DL_SYSCTL_setULPCLKDivider(DL_SYSCTL_ULPCLK_DIV_2);
    DL_SYSCTL_setMCLKSource(SYSOSC, HSCLK, DL_SYSCTL_HSCLK_SOURCE_SYSPLL);
}

/*
 * Timer clock: BUSCLK/8 = 10 MHz (PWM1)
 */
static const DL_TimerA_ClockConfig gTMC_PWM1ClockConfig = {
    .clockSel    = DL_TIMER_CLOCK_BUSCLK,
    .divideRatio = DL_TIMER_CLOCK_DIVIDE_8,
    .prescale    = 0U
};

static const DL_TimerA_PWMConfig gTMC_PWM1Config = {
    .pwmMode           = DL_TIMER_PWM_MODE_EDGE_ALIGN_UP,
    .period            = 10000,
    .isTimerWithFourCC = true,
    .startTimer        = DL_TIMER_STOP,
};

SYSCONFIG_WEAK void SYSCFG_DL_TMC_PWM1_init(void)
{
    DL_TimerA_setClockConfig(TMC_PWM1_INST, (DL_TimerA_ClockConfig *)&gTMC_PWM1ClockConfig);
    DL_TimerA_initPWMMode(TMC_PWM1_INST, (DL_TimerA_PWMConfig *)&gTMC_PWM1Config);
    DL_TimerA_setCounterControl(TMC_PWM1_INST,
                                DL_TIMER_CZC_CCCTL0_ZCOND,
                                DL_TIMER_CAC_CCCTL0_ACOND,
                                DL_TIMER_CLC_CCCTL0_LCOND);
    DL_TimerA_setCaptureCompareOutCtl(TMC_PWM1_INST,
                                      DL_TIMER_CC_OCTL_INIT_VAL_LOW,
                                      DL_TIMER_CC_OCTL_INV_OUT_DISABLED,
                                      DL_TIMER_CC_OCTL_SRC_FUNCVAL,
                                      DL_TIMERA_CAPTURE_COMPARE_0_INDEX);
    DL_TimerA_setCaptCompUpdateMethod(TMC_PWM1_INST,
                                      DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE,
                                      DL_TIMERA_CAPTURE_COMPARE_0_INDEX);
    DL_TimerA_setCaptureCompareValue(TMC_PWM1_INST, 5000, DL_TIMER_CC_0_INDEX);
    DL_TimerA_enableClock(TMC_PWM1_INST);
    DL_TimerA_enableInterrupt(TMC_PWM1_INST, DL_TIMER_INTERRUPT_LOAD_EVENT);
    DL_TimerA_setCCPDirection(TMC_PWM1_INST, DL_TIMER_CC0_OUTPUT);
}

/*
 * Timer clock: BUSCLK/8 on ULP path ≈ 5 MHz (PWM2)
 */
static const DL_TimerG_ClockConfig gTMC_PWM2ClockConfig = {
    .clockSel    = DL_TIMER_CLOCK_BUSCLK,
    .divideRatio = DL_TIMER_CLOCK_DIVIDE_8,
    .prescale    = 0U
};

static const DL_TimerG_PWMConfig gTMC_PWM2Config = {
    .pwmMode           = DL_TIMER_PWM_MODE_EDGE_ALIGN_UP,
    .period            = 10000,
    .isTimerWithFourCC = false,
    .startTimer        = DL_TIMER_STOP,
};

SYSCONFIG_WEAK void SYSCFG_DL_TMC_PWM2_init(void)
{
    DL_TimerG_setClockConfig(TMC_PWM2_INST, (DL_TimerG_ClockConfig *)&gTMC_PWM2ClockConfig);
    DL_TimerG_initPWMMode(TMC_PWM2_INST, (DL_TimerG_PWMConfig *)&gTMC_PWM2Config);
    DL_TimerG_setCounterControl(TMC_PWM2_INST,
                                DL_TIMER_CZC_CCCTL0_ZCOND,
                                DL_TIMER_CAC_CCCTL0_ACOND,
                                DL_TIMER_CLC_CCCTL0_LCOND);
    DL_TimerG_setCaptureCompareOutCtl(TMC_PWM2_INST,
                                      DL_TIMER_CC_OCTL_INIT_VAL_LOW,
                                      DL_TIMER_CC_OCTL_INV_OUT_DISABLED,
                                      DL_TIMER_CC_OCTL_SRC_FUNCVAL,
                                      DL_TIMERG_CAPTURE_COMPARE_0_INDEX);
    DL_TimerG_setCaptCompUpdateMethod(TMC_PWM2_INST,
                                      DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE,
                                      DL_TIMERG_CAPTURE_COMPARE_0_INDEX);
    DL_TimerG_setCaptureCompareValue(TMC_PWM2_INST, 5000, DL_TIMER_CC_0_INDEX);
    DL_TimerG_enableClock(TMC_PWM2_INST);
    DL_TimerG_enableInterrupt(TMC_PWM2_INST, DL_TIMER_INTERRUPT_LOAD_EVENT);
    DL_TimerG_setCCPDirection(TMC_PWM2_INST, DL_TIMER_CC0_OUTPUT);
}

static const DL_UART_Main_ClockConfig gDEBUGClockConfig = {
    .clockSel    = DL_UART_MAIN_CLOCK_BUSCLK,
    .divideRatio = DL_UART_MAIN_CLOCK_DIVIDE_RATIO_1
};

static const DL_UART_Main_Config gDEBUGConfig = {
    .mode        = DL_UART_MAIN_MODE_NORMAL,
    .direction   = DL_UART_MAIN_DIRECTION_TX_RX,
    .flowControl = DL_UART_MAIN_FLOW_CONTROL_NONE,
    .parity      = DL_UART_MAIN_PARITY_NONE,
    .wordLength  = DL_UART_MAIN_WORD_LENGTH_8_BITS,
    .stopBits    = DL_UART_MAIN_STOP_BITS_ONE
};

SYSCONFIG_WEAK void SYSCFG_DL_DEBUG_init(void)
{
    DL_UART_Main_setClockConfig(DEBUG_INST, (DL_UART_Main_ClockConfig *)&gDEBUGClockConfig);
    DL_UART_Main_init(DEBUG_INST, (DL_UART_Main_Config *)&gDEBUGConfig);
    DL_UART_Main_setOversampling(DEBUG_INST, DL_UART_OVERSAMPLING_RATE_16X);
    DL_UART_Main_setBaudRateDivisor(DEBUG_INST,
                                    DEBUG_IBRD_40_MHZ_115200_BAUD,
                                    DEBUG_FBRD_40_MHZ_115200_BAUD);
    DL_UART_Main_enableInterrupt(DEBUG_INST, DL_UART_MAIN_INTERRUPT_RX);
    DL_UART_Main_enable(DEBUG_INST);
}
