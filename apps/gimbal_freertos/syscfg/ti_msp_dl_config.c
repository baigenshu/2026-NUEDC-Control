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
 *  Configured MSPM0 DriverLib module definitions
 *
 *  DO NOT EDIT - This file is generated for the MSPM0G350X
 *  by the SysConfig tool.
 */

#include "ti_msp_dl_config.h"

DL_TimerA_backupConfig gDCC_100_PWM1Backup;

/*
 *  ======== SYSCFG_DL_init ========
 *  Perform any initialization needed before using any board APIs
 */
SYSCONFIG_WEAK void SYSCFG_DL_init(void)
{
    SYSCFG_DL_initPower();
    SYSCFG_DL_GPIO_init();
    /* Module-Specific Initializations*/
    SYSCFG_DL_SYSCTL_init();
    SYSCFG_DL_DCC_100_PWM1_init();
    SYSCFG_DL_DCC_100_PWM2_init();
    SYSCFG_DL_OLED_init();
    SYSCFG_DL_PRINT_init();
    SYSCFG_DL_DEBUG_init();
    /* Ensure backup structures have no valid state */
	gDCC_100_PWM1Backup.backupRdy 	= false;


}
/*
 * User should take care to save and restore register configuration in application.
 * See Retention Configuration section for more details.
 */
SYSCONFIG_WEAK bool SYSCFG_DL_saveConfiguration(void)
{
    bool retStatus = true;

	retStatus &= DL_TimerA_saveConfiguration(DCC_100_PWM1_INST, &gDCC_100_PWM1Backup);

    return retStatus;
}


SYSCONFIG_WEAK bool SYSCFG_DL_restoreConfiguration(void)
{
    bool retStatus = true;

	retStatus &= DL_TimerA_restoreConfiguration(DCC_100_PWM1_INST, &gDCC_100_PWM1Backup, false);

    return retStatus;
}

SYSCONFIG_WEAK void SYSCFG_DL_initPower(void)
{
    DL_GPIO_reset(GPIOA);
    DL_GPIO_reset(GPIOB);
    DL_TimerA_reset(DCC_100_PWM1_INST);
    DL_TimerG_reset(DCC_100_PWM2_INST);
    DL_I2C_reset(OLED_INST);
    DL_UART_Main_reset(PRINT_INST);
    DL_UART_Main_reset(DEBUG_INST);

    DL_GPIO_enablePower(GPIOA);
    DL_GPIO_enablePower(GPIOB);
    DL_TimerA_enablePower(DCC_100_PWM1_INST);
    DL_TimerG_enablePower(DCC_100_PWM2_INST);
    DL_I2C_enablePower(OLED_INST);
    DL_UART_Main_enablePower(PRINT_INST);
    DL_UART_Main_enablePower(DEBUG_INST);
    delay_cycles(POWER_STARTUP_DELAY);
}

SYSCONFIG_WEAK void SYSCFG_DL_GPIO_init(void)
{

    DL_GPIO_initPeripheralAnalogFunction(GPIO_HFXIN_IOMUX);
    DL_GPIO_initPeripheralAnalogFunction(GPIO_HFXOUT_IOMUX);

    DL_GPIO_initPeripheralOutputFunction(GPIO_DCC_100_PWM1_C0_IOMUX,GPIO_DCC_100_PWM1_C0_IOMUX_FUNC);
    DL_GPIO_enableOutput(GPIO_DCC_100_PWM1_C0_PORT, GPIO_DCC_100_PWM1_C0_PIN);
    DL_GPIO_initPeripheralOutputFunction(GPIO_DCC_100_PWM2_C0_IOMUX,GPIO_DCC_100_PWM2_C0_IOMUX_FUNC);
    DL_GPIO_enableOutput(GPIO_DCC_100_PWM2_C0_PORT, GPIO_DCC_100_PWM2_C0_PIN);

    
	DL_GPIO_initPeripheralInputFunctionFeatures(
		 GPIO_OLED_IOMUX_SDA, GPIO_OLED_IOMUX_SDA_FUNC,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
	DL_GPIO_initPeripheralInputFunctionFeatures(
		 GPIO_OLED_IOMUX_SCL, GPIO_OLED_IOMUX_SCL_FUNC,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_enableHiZ(GPIO_OLED_IOMUX_SDA);
    DL_GPIO_enableHiZ(GPIO_OLED_IOMUX_SCL);

    DL_GPIO_initPeripheralOutputFunction(
        GPIO_PRINT_IOMUX_TX, GPIO_PRINT_IOMUX_TX_FUNC);
    DL_GPIO_initPeripheralInputFunction(
        GPIO_PRINT_IOMUX_RX, GPIO_PRINT_IOMUX_RX_FUNC);
    DL_GPIO_initPeripheralOutputFunction(
        GPIO_DEBUG_IOMUX_TX, GPIO_DEBUG_IOMUX_TX_FUNC);
    DL_GPIO_initPeripheralInputFunction(
        GPIO_DEBUG_IOMUX_RX, GPIO_DEBUG_IOMUX_RX_FUNC);

    DL_GPIO_initDigitalOutput(STEP_MOTOR1_DIR1_IOMUX);

    DL_GPIO_initDigitalOutput(STEP_MOTOR1_DCY1_IOMUX);

    DL_GPIO_initDigitalOutput(STEP_MOTOR1_SLP1_IOMUX);

    DL_GPIO_initDigitalOutput(STEP_MOTOR1_RST1_IOMUX);

    DL_GPIO_initDigitalOutput(STEP_MOTOR2_DIR2_IOMUX);

    DL_GPIO_initDigitalOutput(STEP_MOTOR2_DCY2_IOMUX);

    DL_GPIO_initDigitalOutput(STEP_MOTOR2_SLP2_IOMUX);

    DL_GPIO_initDigitalOutput(STEP_MOTOR2_RST2_IOMUX);

    DL_GPIO_clearPins(GPIOA, STEP_MOTOR1_DIR1_PIN |
		STEP_MOTOR1_DCY1_PIN |
		STEP_MOTOR1_SLP1_PIN |
		STEP_MOTOR1_RST1_PIN |
		STEP_MOTOR2_DIR2_PIN |
		STEP_MOTOR2_DCY2_PIN |
		STEP_MOTOR2_SLP2_PIN |
		STEP_MOTOR2_RST2_PIN);
    DL_GPIO_enableOutput(GPIOA, STEP_MOTOR1_DIR1_PIN |
		STEP_MOTOR1_DCY1_PIN |
		STEP_MOTOR1_SLP1_PIN |
		STEP_MOTOR1_RST1_PIN |
		STEP_MOTOR2_DIR2_PIN |
		STEP_MOTOR2_DCY2_PIN |
		STEP_MOTOR2_SLP2_PIN |
		STEP_MOTOR2_RST2_PIN);

}


static const DL_SYSCTL_SYSPLLConfig gSYSPLLConfig = {
    .inputFreq              = DL_SYSCTL_SYSPLL_INPUT_FREQ_32_48_MHZ,
	.rDivClk2x              = 1,
	.rDivClk1               = 0,
	.rDivClk0               = 0,
	.enableCLK2x            = DL_SYSCTL_SYSPLL_CLK2X_DISABLE,
	.enableCLK1             = DL_SYSCTL_SYSPLL_CLK1_DISABLE,
	.enableCLK0             = DL_SYSCTL_SYSPLL_CLK0_ENABLE,
	.sysPLLMCLK             = DL_SYSCTL_SYSPLL_MCLK_CLK0,
	.sysPLLRef              = DL_SYSCTL_SYSPLL_REF_SYSOSC,
	.qDiv                   = 4,
	.pDiv                   = DL_SYSCTL_SYSPLL_PDIV_1
};
SYSCONFIG_WEAK void SYSCFG_DL_SYSCTL_init(void)
{

	//Low Power Mode is configured to be SLEEP0
    DL_SYSCTL_setBORThreshold(DL_SYSCTL_BOR_THRESHOLD_LEVEL_0);
    DL_SYSCTL_setFlashWaitState(DL_SYSCTL_FLASH_WAIT_STATE_2);

    
	DL_SYSCTL_setSYSOSCFreq(DL_SYSCTL_SYSOSC_FREQ_BASE);
	/* Set default configuration */
	DL_SYSCTL_disableHFXT();
	DL_SYSCTL_disableSYSPLL();
    DL_SYSCTL_setHFCLKSourceHFXTParams(DL_SYSCTL_HFXT_RANGE_32_48_MHZ,0, false);
    DL_SYSCTL_configSYSPLL((DL_SYSCTL_SYSPLLConfig *) &gSYSPLLConfig);
    DL_SYSCTL_setULPCLKDivider(DL_SYSCTL_ULPCLK_DIV_2);
    DL_SYSCTL_setMCLKSource(SYSOSC, HSCLK, DL_SYSCTL_HSCLK_SOURCE_SYSPLL);

}


/*
 * Timer clock configuration to be sourced by  / 8 (10000000 Hz)
 * timerClkFreq = (timerClkSrc / (timerClkDivRatio * (timerClkPrescale + 1)))
 *   10000000 Hz = 10000000 Hz / (8 * (0 + 1))
 */
static const DL_TimerA_ClockConfig gDCC_100_PWM1ClockConfig = {
    .clockSel = DL_TIMER_CLOCK_BUSCLK,
    .divideRatio = DL_TIMER_CLOCK_DIVIDE_8,
    .prescale = 0U
};

static const DL_TimerA_PWMConfig gDCC_100_PWM1Config = {
    .pwmMode = DL_TIMER_PWM_MODE_EDGE_ALIGN_UP,
    .period = 10000,
    .isTimerWithFourCC = true,
    .startTimer = DL_TIMER_STOP,
};

SYSCONFIG_WEAK void SYSCFG_DL_DCC_100_PWM1_init(void) {

    DL_TimerA_setClockConfig(
        DCC_100_PWM1_INST, (DL_TimerA_ClockConfig *) &gDCC_100_PWM1ClockConfig);

    DL_TimerA_initPWMMode(
        DCC_100_PWM1_INST, (DL_TimerA_PWMConfig *) &gDCC_100_PWM1Config);

    // Set Counter control to the smallest CC index being used
    DL_TimerA_setCounterControl(DCC_100_PWM1_INST,DL_TIMER_CZC_CCCTL0_ZCOND,DL_TIMER_CAC_CCCTL0_ACOND,DL_TIMER_CLC_CCCTL0_LCOND);

    DL_TimerA_setCaptureCompareOutCtl(DCC_100_PWM1_INST, DL_TIMER_CC_OCTL_INIT_VAL_LOW,
		DL_TIMER_CC_OCTL_INV_OUT_DISABLED, DL_TIMER_CC_OCTL_SRC_FUNCVAL,
		DL_TIMERA_CAPTURE_COMPARE_0_INDEX);

    DL_TimerA_setCaptCompUpdateMethod(DCC_100_PWM1_INST, DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE, DL_TIMERA_CAPTURE_COMPARE_0_INDEX);
    DL_TimerA_setCaptureCompareValue(DCC_100_PWM1_INST, 5000, DL_TIMER_CC_0_INDEX);

    DL_TimerA_enableClock(DCC_100_PWM1_INST);


    DL_TimerA_enableInterrupt(DCC_100_PWM1_INST , DL_TIMER_INTERRUPT_LOAD_EVENT);

    DL_TimerA_setCCPDirection(DCC_100_PWM1_INST , DL_TIMER_CC0_OUTPUT );


}
/*
 * Timer clock configuration to be sourced by  / 8 (5000000 Hz)
 * timerClkFreq = (timerClkSrc / (timerClkDivRatio * (timerClkPrescale + 1)))
 *   5000000 Hz = 5000000 Hz / (8 * (0 + 1))
 */
static const DL_TimerG_ClockConfig gDCC_100_PWM2ClockConfig = {
    .clockSel = DL_TIMER_CLOCK_BUSCLK,
    .divideRatio = DL_TIMER_CLOCK_DIVIDE_8,
    .prescale = 0U
};

static const DL_TimerG_PWMConfig gDCC_100_PWM2Config = {
    .pwmMode = DL_TIMER_PWM_MODE_EDGE_ALIGN_UP,
    .period = 10000,
    .isTimerWithFourCC = false,
    .startTimer = DL_TIMER_STOP,
};

SYSCONFIG_WEAK void SYSCFG_DL_DCC_100_PWM2_init(void) {

    DL_TimerG_setClockConfig(
        DCC_100_PWM2_INST, (DL_TimerG_ClockConfig *) &gDCC_100_PWM2ClockConfig);

    DL_TimerG_initPWMMode(
        DCC_100_PWM2_INST, (DL_TimerG_PWMConfig *) &gDCC_100_PWM2Config);

    // Set Counter control to the smallest CC index being used
    DL_TimerG_setCounterControl(DCC_100_PWM2_INST,DL_TIMER_CZC_CCCTL0_ZCOND,DL_TIMER_CAC_CCCTL0_ACOND,DL_TIMER_CLC_CCCTL0_LCOND);

    DL_TimerG_setCaptureCompareOutCtl(DCC_100_PWM2_INST, DL_TIMER_CC_OCTL_INIT_VAL_LOW,
		DL_TIMER_CC_OCTL_INV_OUT_DISABLED, DL_TIMER_CC_OCTL_SRC_FUNCVAL,
		DL_TIMERG_CAPTURE_COMPARE_0_INDEX);

    DL_TimerG_setCaptCompUpdateMethod(DCC_100_PWM2_INST, DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE, DL_TIMERG_CAPTURE_COMPARE_0_INDEX);
    DL_TimerG_setCaptureCompareValue(DCC_100_PWM2_INST, 5000, DL_TIMER_CC_0_INDEX);

    DL_TimerG_enableClock(DCC_100_PWM2_INST);


    DL_TimerG_enableInterrupt(DCC_100_PWM2_INST , DL_TIMER_INTERRUPT_LOAD_EVENT);

    DL_TimerG_setCCPDirection(DCC_100_PWM2_INST , DL_TIMER_CC0_OUTPUT );


}


static const DL_I2C_ClockConfig gOLEDClockConfig = {
    .clockSel = DL_I2C_CLOCK_BUSCLK,
    .divideRatio = DL_I2C_CLOCK_DIVIDE_1,
};

SYSCONFIG_WEAK void SYSCFG_DL_OLED_init(void) {

    DL_I2C_setClockConfig(OLED_INST,
        (DL_I2C_ClockConfig *) &gOLEDClockConfig);
    DL_I2C_setAnalogGlitchFilterPulseWidth(OLED_INST,
        DL_I2C_ANALOG_GLITCH_FILTER_WIDTH_50NS);
    DL_I2C_enableAnalogGlitchFilter(OLED_INST);
    DL_I2C_setDigitalGlitchFilterPulseWidth(OLED_INST,
        DL_I2C_DIGITAL_GLITCH_FILTER_WIDTH_CLOCKS_1);

    /* Configure Controller Mode */
    DL_I2C_resetControllerTransfer(OLED_INST);
    /* Set frequency to 100000 Hz*/
    DL_I2C_setTimerPeriod(OLED_INST, 39);
    DL_I2C_setControllerTXFIFOThreshold(OLED_INST, DL_I2C_TX_FIFO_LEVEL_BYTES_7);
    DL_I2C_setControllerRXFIFOThreshold(OLED_INST, DL_I2C_RX_FIFO_LEVEL_BYTES_8);
    DL_I2C_enableControllerClockStretching(OLED_INST);


    /* Enable module */
    DL_I2C_enableController(OLED_INST);


}

static const DL_UART_Main_ClockConfig gPRINTClockConfig = {
    .clockSel    = DL_UART_MAIN_CLOCK_BUSCLK,
    .divideRatio = DL_UART_MAIN_CLOCK_DIVIDE_RATIO_1
};

static const DL_UART_Main_Config gPRINTConfig = {
    .mode        = DL_UART_MAIN_MODE_NORMAL,
    .direction   = DL_UART_MAIN_DIRECTION_TX_RX,
    .flowControl = DL_UART_MAIN_FLOW_CONTROL_NONE,
    .parity      = DL_UART_MAIN_PARITY_NONE,
    .wordLength  = DL_UART_MAIN_WORD_LENGTH_8_BITS,
    .stopBits    = DL_UART_MAIN_STOP_BITS_ONE
};

SYSCONFIG_WEAK void SYSCFG_DL_PRINT_init(void)
{
    DL_UART_Main_setClockConfig(PRINT_INST, (DL_UART_Main_ClockConfig *) &gPRINTClockConfig);

    DL_UART_Main_init(PRINT_INST, (DL_UART_Main_Config *) &gPRINTConfig);
    /*
     * Configure baud rate by setting oversampling and baud rate divisors.
     *  Target baud rate: 115200
     *  Actual baud rate: 115190.78
     */
    DL_UART_Main_setOversampling(PRINT_INST, DL_UART_OVERSAMPLING_RATE_16X);
    DL_UART_Main_setBaudRateDivisor(PRINT_INST, PRINT_IBRD_40_MHZ_115200_BAUD, PRINT_FBRD_40_MHZ_115200_BAUD);


    /* Configure Interrupts */
    DL_UART_Main_enableInterrupt(PRINT_INST,
                                 DL_UART_MAIN_INTERRUPT_RX);


    DL_UART_Main_enable(PRINT_INST);
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
    DL_UART_Main_setClockConfig(DEBUG_INST, (DL_UART_Main_ClockConfig *) &gDEBUGClockConfig);

    DL_UART_Main_init(DEBUG_INST, (DL_UART_Main_Config *) &gDEBUGConfig);
    /*
     * Configure baud rate by setting oversampling and baud rate divisors.
     *  Target baud rate: 115200
     *  Actual baud rate: 115190.78
     */
    DL_UART_Main_setOversampling(DEBUG_INST, DL_UART_OVERSAMPLING_RATE_16X);
    DL_UART_Main_setBaudRateDivisor(DEBUG_INST, DEBUG_IBRD_40_MHZ_115200_BAUD, DEBUG_FBRD_40_MHZ_115200_BAUD);


    /* Configure Interrupts */
    DL_UART_Main_enableInterrupt(DEBUG_INST,
                                 DL_UART_MAIN_INTERRUPT_RX);


    DL_UART_Main_enable(DEBUG_INST);
}

