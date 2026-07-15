/*
 * Copyright (c) 2023, Texas Instruments Incorporated
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
 *  ============ ti_msp_dl_config.c =============
 *  Gimbal dual-stepper configuration
 *  CPUCLK = 80 MHz (SYSOSC → SYSPLL ×5 /2 → 80 MHz MCLK)
 */

#include "ti_msp_dl_config.h"

DL_TimerA_backupConfig gSTEPPER1_PWMBackup;
DL_TimerA_backupConfig gSTEPPER2_PWMBackup;

/*
 *  ======== SYSCFG_DL_init ========
 */
SYSCONFIG_WEAK void SYSCFG_DL_init(void)
{
    SYSCFG_DL_initPower();
    SYSCFG_DL_GPIO_init();
    /* Module-Specific Initializations */
    SYSCFG_DL_SYSCTL_init();
    SYSCFG_DL_STEPPER1_PWM_init();
    SYSCFG_DL_STEPPER2_PWM_init();
    SYSCFG_DL_SYSTICK_init();

    gSTEPPER1_PWMBackup.backupRdy = false;
    gSTEPPER2_PWMBackup.backupRdy = false;
}

SYSCONFIG_WEAK bool SYSCFG_DL_saveConfiguration(void)
{
    bool retStatus = true;

    retStatus &= DL_TimerA_saveConfiguration(STEPPER1_PWM_INST, &gSTEPPER1_PWMBackup);
    retStatus &= DL_TimerA_saveConfiguration(STEPPER2_PWM_INST, &gSTEPPER2_PWMBackup);

    return retStatus;
}

SYSCONFIG_WEAK bool SYSCFG_DL_restoreConfiguration(void)
{
    bool retStatus = true;

    retStatus &= DL_TimerA_restoreConfiguration(STEPPER1_PWM_INST, &gSTEPPER1_PWMBackup, false);
    retStatus &= DL_TimerA_restoreConfiguration(STEPPER2_PWM_INST, &gSTEPPER2_PWMBackup, false);

    return retStatus;
}

SYSCONFIG_WEAK void SYSCFG_DL_initPower(void)
{
    DL_GPIO_reset(GPIOA);
    DL_GPIO_reset(GPIOB);
    DL_TimerA_reset(STEPPER1_PWM_INST);
    DL_TimerA_reset(STEPPER2_PWM_INST);

    DL_GPIO_enablePower(GPIOA);
    DL_GPIO_enablePower(GPIOB);
    DL_TimerA_enablePower(STEPPER1_PWM_INST);
    DL_TimerA_enablePower(STEPPER2_PWM_INST);

    delay_cycles(POWER_STARTUP_DELAY);
}

SYSCONFIG_WEAK void SYSCFG_DL_GPIO_init(void)
{
    /* STEP PWM pins */
    DL_GPIO_initPeripheralOutputFunction(
        GPIO_STEPPER1_PWM_C0_IOMUX, GPIO_STEPPER1_PWM_C0_IOMUX_FUNC);
    DL_GPIO_enableOutput(GPIO_STEPPER1_PWM_C0_PORT, GPIO_STEPPER1_PWM_C0_PIN);

    DL_GPIO_initPeripheralOutputFunction(
        GPIO_STEPPER2_PWM_C0_IOMUX, GPIO_STEPPER2_PWM_C0_IOMUX_FUNC);
    DL_GPIO_enableOutput(GPIO_STEPPER2_PWM_C0_PORT, GPIO_STEPPER2_PWM_C0_PIN);

    /* Control pins: DIR / DCY / SLP / RST */
    DL_GPIO_initDigitalOutput(STEPPER1_DIR1_IOMUX);
    DL_GPIO_initDigitalOutput(STEPPER1_DCY1_IOMUX);
    DL_GPIO_initDigitalOutput(STEPPER1_SLP1_IOMUX);
    DL_GPIO_initDigitalOutput(STEPPER1_RST1_IOMUX);

    DL_GPIO_initDigitalOutput(STEPPER2_DIR2_IOMUX);
    DL_GPIO_initDigitalOutput(STEPPER2_DCY2_IOMUX);
    DL_GPIO_initDigitalOutput(STEPPER2_SLP2_IOMUX);
    DL_GPIO_initDigitalOutput(STEPPER2_RST2_IOMUX);

    /* DIR = 0 (CW), DCY/SLP/RST = 1 (active / awake / out of reset) */
    DL_GPIO_clearPins(GPIOA, STEPPER1_DIR1_PIN | STEPPER2_DIR2_PIN);
    DL_GPIO_setPins(GPIOA,
                    STEPPER1_DCY1_PIN | STEPPER1_SLP1_PIN | STEPPER1_RST1_PIN |
                        STEPPER2_DCY2_PIN | STEPPER2_SLP2_PIN | STEPPER2_RST2_PIN);
    DL_GPIO_enableOutput(GPIOA,
                         STEPPER1_DIR1_PIN | STEPPER1_DCY1_PIN | STEPPER1_SLP1_PIN |
                             STEPPER1_RST1_PIN | STEPPER2_DIR2_PIN | STEPPER2_DCY2_PIN |
                             STEPPER2_SLP2_PIN | STEPPER2_RST2_PIN);
}

/* SYSPLL: SYSOSC 32 MHz → /2 (PDIV) ×5 (QDIV) ×2 (CLK2x) = 160 MHz → MCLK=80 MHz after /2 path
 * Matches diansai gSYSPLLConfig. */
static const DL_SYSCTL_SYSPLLConfig gSYSPLLConfig = {
    .inputFreq   = DL_SYSCTL_SYSPLL_INPUT_FREQ_16_32_MHZ,
    .rDivClk2x   = 1,
    .rDivClk1    = 0,
    .rDivClk0    = 0,
    .enableCLK2x = DL_SYSCTL_SYSPLL_CLK2X_ENABLE,
    .enableCLK1  = DL_SYSCTL_SYSPLL_CLK1_DISABLE,
    .enableCLK0  = DL_SYSCTL_SYSPLL_CLK0_DISABLE,
    .sysPLLMCLK  = DL_SYSCTL_SYSPLL_MCLK_CLK2X,
    .sysPLLRef   = DL_SYSCTL_SYSPLL_REF_SYSOSC,
    .qDiv        = 4,
    .pDiv        = DL_SYSCTL_SYSPLL_PDIV_2
};

SYSCONFIG_WEAK void SYSCFG_DL_SYSCTL_init(void)
{
    /* Low Power Mode is configured to be SLEEP0 */
    DL_SYSCTL_setBORThreshold(DL_SYSCTL_BOR_THRESHOLD_LEVEL_0);
    DL_SYSCTL_setFlashWaitState(DL_SYSCTL_FLASH_WAIT_STATE_2);

    DL_SYSCTL_setSYSOSCFreq(DL_SYSCTL_SYSOSC_FREQ_BASE);
    /* Set default configuration */
    DL_SYSCTL_disableHFXT();
    DL_SYSCTL_disableSYSPLL();
    DL_SYSCTL_configSYSPLL((DL_SYSCTL_SYSPLLConfig *)&gSYSPLLConfig);
    DL_SYSCTL_setULPCLKDivider(DL_SYSCTL_ULPCLK_DIV_2);
    DL_SYSCTL_setMCLKSource(SYSOSC, HSCLK, DL_SYSCTL_HSCLK_SOURCE_SYSPLL);
}

/*
 * Timer clock: BUSCLK 80 MHz / 1 / 1 = 80 MHz
 * Default period 800 → 100 kHz PWM (overridden at runtime by Stepper_SetSpeed)
 * startTimer = STOP (driver starts when needed)
 */
static const DL_TimerA_ClockConfig gSTEPPER1_PWMClockConfig = {
    .clockSel    = DL_TIMER_CLOCK_BUSCLK,
    .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
    .prescale    = 0U
};

static const DL_TimerA_PWMConfig gSTEPPER1_PWMConfig = {
    .pwmMode          = DL_TIMER_PWM_MODE_EDGE_ALIGN,
    .period           = 800,
    .isTimerWithFourCC = true,
    .startTimer       = DL_TIMER_STOP,
};

SYSCONFIG_WEAK void SYSCFG_DL_STEPPER1_PWM_init(void)
{
    DL_TimerA_setClockConfig(
        STEPPER1_PWM_INST, (DL_TimerA_ClockConfig *)&gSTEPPER1_PWMClockConfig);

    DL_TimerA_initPWMMode(
        STEPPER1_PWM_INST, (DL_TimerA_PWMConfig *)&gSTEPPER1_PWMConfig);

    DL_TimerA_setCounterControl(STEPPER1_PWM_INST, DL_TIMER_CZC_CCCTL0_ZCOND,
                                DL_TIMER_CAC_CCCTL0_ACOND, DL_TIMER_CLC_CCCTL0_LCOND);

    DL_TimerA_setCaptureCompareOutCtl(STEPPER1_PWM_INST, DL_TIMER_CC_OCTL_INIT_VAL_LOW,
                                      DL_TIMER_CC_OCTL_INV_OUT_DISABLED,
                                      DL_TIMER_CC_OCTL_SRC_FUNCVAL,
                                      DL_TIMERA_CAPTURE_COMPARE_0_INDEX);

    DL_TimerA_setCaptCompUpdateMethod(STEPPER1_PWM_INST,
                                      DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE,
                                      DL_TIMERA_CAPTURE_COMPARE_0_INDEX);
    DL_TimerA_setCaptureCompareValue(STEPPER1_PWM_INST, 400, DL_TIMER_CC_0_INDEX);

    DL_TimerA_enableClock(STEPPER1_PWM_INST);
    DL_TimerA_enableInterrupt(STEPPER1_PWM_INST, DL_TIMER_INTERRUPT_LOAD_EVENT);
    DL_TimerA_setCCPDirection(STEPPER1_PWM_INST, DL_TIMER_CC0_OUTPUT);
}

static const DL_TimerA_ClockConfig gSTEPPER2_PWMClockConfig = {
    .clockSel    = DL_TIMER_CLOCK_BUSCLK,
    .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
    .prescale    = 0U
};

static const DL_TimerA_PWMConfig gSTEPPER2_PWMConfig = {
    .pwmMode          = DL_TIMER_PWM_MODE_EDGE_ALIGN,
    .period           = 800,
    .isTimerWithFourCC = true,
    .startTimer       = DL_TIMER_STOP,
};

SYSCONFIG_WEAK void SYSCFG_DL_STEPPER2_PWM_init(void)
{
    DL_TimerA_setClockConfig(
        STEPPER2_PWM_INST, (DL_TimerA_ClockConfig *)&gSTEPPER2_PWMClockConfig);

    DL_TimerA_initPWMMode(
        STEPPER2_PWM_INST, (DL_TimerA_PWMConfig *)&gSTEPPER2_PWMConfig);

    DL_TimerA_setCounterControl(STEPPER2_PWM_INST, DL_TIMER_CZC_CCCTL0_ZCOND,
                                DL_TIMER_CAC_CCCTL0_ACOND, DL_TIMER_CLC_CCCTL0_LCOND);

    DL_TimerA_setCaptureCompareOutCtl(STEPPER2_PWM_INST, DL_TIMER_CC_OCTL_INIT_VAL_LOW,
                                      DL_TIMER_CC_OCTL_INV_OUT_DISABLED,
                                      DL_TIMER_CC_OCTL_SRC_FUNCVAL,
                                      DL_TIMERA_CAPTURE_COMPARE_0_INDEX);

    DL_TimerA_setCaptCompUpdateMethod(STEPPER2_PWM_INST,
                                      DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE,
                                      DL_TIMERA_CAPTURE_COMPARE_0_INDEX);
    DL_TimerA_setCaptureCompareValue(STEPPER2_PWM_INST, 400, DL_TIMER_CC_0_INDEX);

    DL_TimerA_enableClock(STEPPER2_PWM_INST);
    DL_TimerA_enableInterrupt(STEPPER2_PWM_INST, DL_TIMER_INTERRUPT_LOAD_EVENT);
    DL_TimerA_setCCPDirection(STEPPER2_PWM_INST, DL_TIMER_CC0_OUTPUT);
}

SYSCONFIG_WEAK void SYSCFG_DL_SYSTICK_init(void)
{
    /* 24-bit max period (~209.7 ms @ 80 MHz) */
    DL_SYSTICK_init(16777216);
    DL_SYSTICK_enable();
}
