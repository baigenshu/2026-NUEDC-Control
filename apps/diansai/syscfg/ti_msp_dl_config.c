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

DL_TimerG_backupConfig gPWMBBackup;
DL_SPI_backupConfig gSPI_OLEDBackup;

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
    SYSCFG_DL_PWMA_init();
    SYSCFG_DL_PWMB_init();
    SYSCFG_DL_I2C_init();
    SYSCFG_DL_IMU601_init();
    SYSCFG_DL_SPI_OLED_init();
    SYSCFG_DL_SYSTICK_init();
    /* Ensure backup structures have no valid state */
	gPWMBBackup.backupRdy 	= false;

	gSPI_OLEDBackup.backupRdy 	= false;

}
/*
 * User should take care to save and restore register configuration in application.
 * See Retention Configuration section for more details.
 */
SYSCONFIG_WEAK bool SYSCFG_DL_saveConfiguration(void)
{
    bool retStatus = true;

	retStatus &= DL_TimerG_saveConfiguration(PWMB_INST, &gPWMBBackup);
	retStatus &= DL_SPI_saveConfiguration(SPI_OLED_INST, &gSPI_OLEDBackup);

    return retStatus;
}


SYSCONFIG_WEAK bool SYSCFG_DL_restoreConfiguration(void)
{
    bool retStatus = true;

	retStatus &= DL_TimerG_restoreConfiguration(PWMB_INST, &gPWMBBackup, false);
	retStatus &= DL_SPI_restoreConfiguration(SPI_OLED_INST, &gSPI_OLEDBackup);

    return retStatus;
}

SYSCONFIG_WEAK void SYSCFG_DL_initPower(void)
{
    DL_GPIO_reset(GPIOA);
    DL_GPIO_reset(GPIOB);
    DL_TimerG_reset(PWMA_INST);
    DL_TimerG_reset(PWMB_INST);
    DL_I2C_reset(I2C_INST);
    DL_UART_Main_reset(IMU601_INST);
    DL_SPI_reset(SPI_OLED_INST);


    DL_GPIO_enablePower(GPIOA);
    DL_GPIO_enablePower(GPIOB);
    DL_TimerG_enablePower(PWMA_INST);
    DL_TimerG_enablePower(PWMB_INST);
    DL_I2C_enablePower(I2C_INST);
    DL_UART_Main_enablePower(IMU601_INST);
    DL_SPI_enablePower(SPI_OLED_INST);

    delay_cycles(POWER_STARTUP_DELAY);
}

SYSCONFIG_WEAK void SYSCFG_DL_GPIO_init(void)
{

    DL_GPIO_initPeripheralOutputFunction(GPIO_PWMA_C0_IOMUX,GPIO_PWMA_C0_IOMUX_FUNC);
    DL_GPIO_enableOutput(GPIO_PWMA_C0_PORT, GPIO_PWMA_C0_PIN);
    DL_GPIO_initPeripheralOutputFunction(GPIO_PWMB_C0_IOMUX,GPIO_PWMB_C0_IOMUX_FUNC);
    DL_GPIO_enableOutput(GPIO_PWMB_C0_PORT, GPIO_PWMB_C0_PIN);

    
	DL_GPIO_initPeripheralInputFunctionFeatures(
		 GPIO_I2C_IOMUX_SDA, GPIO_I2C_IOMUX_SDA_FUNC,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
	DL_GPIO_initPeripheralInputFunctionFeatures(
		 GPIO_I2C_IOMUX_SCL, GPIO_I2C_IOMUX_SCL_FUNC,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_enableHiZ(GPIO_I2C_IOMUX_SDA);
    DL_GPIO_enableHiZ(GPIO_I2C_IOMUX_SCL);

    DL_GPIO_initPeripheralOutputFunction(
        GPIO_IMU601_IOMUX_TX, GPIO_IMU601_IOMUX_TX_FUNC);
    DL_GPIO_initPeripheralInputFunction(
        GPIO_IMU601_IOMUX_RX, GPIO_IMU601_IOMUX_RX_FUNC);

    DL_GPIO_initPeripheralOutputFunction(
        GPIO_SPI_OLED_IOMUX_SCLK, GPIO_SPI_OLED_IOMUX_SCLK_FUNC);
    DL_GPIO_initPeripheralOutputFunction(
        GPIO_SPI_OLED_IOMUX_PICO, GPIO_SPI_OLED_IOMUX_PICO_FUNC);
    DL_GPIO_initPeripheralInputFunction(
        GPIO_SPI_OLED_IOMUX_POCI, GPIO_SPI_OLED_IOMUX_POCI_FUNC);

    DL_GPIO_initDigitalOutput(SPI_OLED_CTRL_CS_IOMUX);

    DL_GPIO_initDigitalOutput(SPI_OLED_CTRL_DC_IOMUX);

    DL_GPIO_initDigitalOutput(SPI_OLED_CTRL_RES_IOMUX);

    DL_GPIO_initDigitalInputFeatures(GPIO_GRAY_PIN_0_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(GPIO_GRAY_PIN_1_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(GPIO_GRAY_PIN_2_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(GPIO_GRAY_PIN_3_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(GPIO_GRAY_PIN_4_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(GPIO_GRAY_PIN_5_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(GPIO_GRAY_PIN_6_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(GPIO_GRAY_PIN_7_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalOutput(GPIO_MOTOR_AIN1_IOMUX);

    DL_GPIO_initDigitalOutput(GPIO_MOTOR_AIN2_IOMUX);

    DL_GPIO_initDigitalOutput(GPIO_MOTOR_STBY_IOMUX);

    DL_GPIO_initDigitalOutput(GPIO_MOTOR_BIN1_IOMUX);

    DL_GPIO_initDigitalOutput(GPIO_MOTOR_BIN2_IOMUX);

    DL_GPIO_initDigitalInputFeatures(GPIO_ENCODERA_E1A_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(GPIO_ENCODERA_E1B_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(GPIO_ENCODERB_E2A_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(GPIO_ENCODERB_E2B_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_clearPins(GPIOB, GPIO_MOTOR_AIN1_PIN |
		GPIO_MOTOR_AIN2_PIN |
		GPIO_MOTOR_BIN1_PIN |
		GPIO_MOTOR_BIN2_PIN);
    DL_GPIO_setPins(GPIOB, SPI_OLED_CTRL_CS_PIN |
		SPI_OLED_CTRL_DC_PIN |
		SPI_OLED_CTRL_RES_PIN |
		GPIO_MOTOR_STBY_PIN);
    DL_GPIO_enableOutput(GPIOB, SPI_OLED_CTRL_CS_PIN |
		SPI_OLED_CTRL_DC_PIN |
		SPI_OLED_CTRL_RES_PIN |
		GPIO_MOTOR_AIN1_PIN |
		GPIO_MOTOR_AIN2_PIN |
		GPIO_MOTOR_STBY_PIN |
		GPIO_MOTOR_BIN1_PIN |
		GPIO_MOTOR_BIN2_PIN);
    DL_GPIO_setLowerPinsPolarity(GPIOB, DL_GPIO_PIN_0_EDGE_RISE_FALL |
		DL_GPIO_PIN_5_EDGE_RISE_FALL |
		DL_GPIO_PIN_12_EDGE_RISE_FALL);
    DL_GPIO_setUpperPinsPolarity(GPIOB, DL_GPIO_PIN_18_EDGE_RISE_FALL);
    DL_GPIO_clearInterruptStatus(GPIOB, GPIO_ENCODERA_E1A_PIN |
		GPIO_ENCODERA_E1B_PIN |
		GPIO_ENCODERB_E2A_PIN |
		GPIO_ENCODERB_E2B_PIN);
    DL_GPIO_enableInterrupt(GPIOB, GPIO_ENCODERA_E1A_PIN |
		GPIO_ENCODERA_E1B_PIN |
		GPIO_ENCODERB_E2A_PIN |
		GPIO_ENCODERB_E2B_PIN);

}


static const DL_SYSCTL_SYSPLLConfig gSYSPLLConfig = {
    .inputFreq              = DL_SYSCTL_SYSPLL_INPUT_FREQ_16_32_MHZ,
	.rDivClk2x              = 1,
	.rDivClk1               = 0,
	.rDivClk0               = 0,
	.enableCLK2x            = DL_SYSCTL_SYSPLL_CLK2X_ENABLE,
	.enableCLK1             = DL_SYSCTL_SYSPLL_CLK1_DISABLE,
	.enableCLK0             = DL_SYSCTL_SYSPLL_CLK0_DISABLE,
	.sysPLLMCLK             = DL_SYSCTL_SYSPLL_MCLK_CLK2X,
	.sysPLLRef              = DL_SYSCTL_SYSPLL_REF_SYSOSC,
	.qDiv                   = 4,
	.pDiv                   = DL_SYSCTL_SYSPLL_PDIV_2
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
    DL_SYSCTL_configSYSPLL((DL_SYSCTL_SYSPLLConfig *) &gSYSPLLConfig);
    DL_SYSCTL_setULPCLKDivider(DL_SYSCTL_ULPCLK_DIV_2);
    DL_SYSCTL_setMCLKSource(SYSOSC, HSCLK, DL_SYSCTL_HSCLK_SOURCE_SYSPLL);

}


/*
 * Timer clock configuration to be sourced by  / 1 (40000000 Hz)
 * timerClkFreq = (timerClkSrc / (timerClkDivRatio * (timerClkPrescale + 1)))
 *   40000000 Hz = 40000000 Hz / (1 * (0 + 1))
 */
static const DL_TimerG_ClockConfig gPWMAClockConfig = {
    .clockSel = DL_TIMER_CLOCK_BUSCLK,
    .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
    .prescale = 0U
};

static const DL_TimerG_PWMConfig gPWMAConfig = {
    .pwmMode = DL_TIMER_PWM_MODE_EDGE_ALIGN_UP,
    .period = 4000,
    .isTimerWithFourCC = false,
    .startTimer = DL_TIMER_START,
};

SYSCONFIG_WEAK void SYSCFG_DL_PWMA_init(void) {

    DL_TimerG_setClockConfig(
        PWMA_INST, (DL_TimerG_ClockConfig *) &gPWMAClockConfig);

    DL_TimerG_initPWMMode(
        PWMA_INST, (DL_TimerG_PWMConfig *) &gPWMAConfig);

    // Set Counter control to the smallest CC index being used
    DL_TimerG_setCounterControl(PWMA_INST,DL_TIMER_CZC_CCCTL0_ZCOND,DL_TIMER_CAC_CCCTL0_ACOND,DL_TIMER_CLC_CCCTL0_LCOND);

    DL_TimerG_setCaptureCompareOutCtl(PWMA_INST, DL_TIMER_CC_OCTL_INIT_VAL_LOW,
		DL_TIMER_CC_OCTL_INV_OUT_DISABLED, DL_TIMER_CC_OCTL_SRC_FUNCVAL,
		DL_TIMERG_CAPTURE_COMPARE_0_INDEX);

    DL_TimerG_setCaptCompUpdateMethod(PWMA_INST, DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE, DL_TIMERG_CAPTURE_COMPARE_0_INDEX);
    DL_TimerG_setCaptureCompareValue(PWMA_INST, 0, DL_TIMER_CC_0_INDEX);

    DL_TimerG_enableClock(PWMA_INST);


    
    DL_TimerG_setCCPDirection(PWMA_INST , DL_TIMER_CC0_OUTPUT );


}
/*
 * Timer clock configuration to be sourced by  / 1 (80000000 Hz)
 * timerClkFreq = (timerClkSrc / (timerClkDivRatio * (timerClkPrescale + 1)))
 *   40000000 Hz = 80000000 Hz / (1 * (1 + 1))
 */
static const DL_TimerG_ClockConfig gPWMBClockConfig = {
    .clockSel = DL_TIMER_CLOCK_BUSCLK,
    .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
    .prescale = 1U
};

static const DL_TimerG_PWMConfig gPWMBConfig = {
    .pwmMode = DL_TIMER_PWM_MODE_EDGE_ALIGN_UP,
    .period = 4000,
    .isTimerWithFourCC = false,
    .startTimer = DL_TIMER_START,
};

SYSCONFIG_WEAK void SYSCFG_DL_PWMB_init(void) {

    DL_TimerG_setClockConfig(
        PWMB_INST, (DL_TimerG_ClockConfig *) &gPWMBClockConfig);

    DL_TimerG_initPWMMode(
        PWMB_INST, (DL_TimerG_PWMConfig *) &gPWMBConfig);

    // Set Counter control to the smallest CC index being used
    DL_TimerG_setCounterControl(PWMB_INST,DL_TIMER_CZC_CCCTL0_ZCOND,DL_TIMER_CAC_CCCTL0_ACOND,DL_TIMER_CLC_CCCTL0_LCOND);

    DL_TimerG_setCaptureCompareOutCtl(PWMB_INST, DL_TIMER_CC_OCTL_INIT_VAL_LOW,
		DL_TIMER_CC_OCTL_INV_OUT_DISABLED, DL_TIMER_CC_OCTL_SRC_FUNCVAL,
		DL_TIMERG_CAPTURE_COMPARE_0_INDEX);

    DL_TimerG_setCaptCompUpdateMethod(PWMB_INST, DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE, DL_TIMERG_CAPTURE_COMPARE_0_INDEX);
    DL_TimerG_setCaptureCompareValue(PWMB_INST, 0, DL_TIMER_CC_0_INDEX);

    DL_TimerG_enableClock(PWMB_INST);


    
    DL_TimerG_setCCPDirection(PWMB_INST , DL_TIMER_CC0_OUTPUT );


}


static const DL_I2C_ClockConfig gI2CClockConfig = {
    .clockSel = DL_I2C_CLOCK_BUSCLK,
    .divideRatio = DL_I2C_CLOCK_DIVIDE_1,
};

SYSCONFIG_WEAK void SYSCFG_DL_I2C_init(void) {

    DL_I2C_setClockConfig(I2C_INST,
        (DL_I2C_ClockConfig *) &gI2CClockConfig);
    DL_I2C_setAnalogGlitchFilterPulseWidth(I2C_INST,
        DL_I2C_ANALOG_GLITCH_FILTER_WIDTH_50NS);
    DL_I2C_enableAnalogGlitchFilter(I2C_INST);

    /* Configure Controller Mode */
    DL_I2C_resetControllerTransfer(I2C_INST);
    /* Set frequency to 400000 Hz*/
    DL_I2C_setTimerPeriod(I2C_INST, 9);
    DL_I2C_setControllerTXFIFOThreshold(I2C_INST, DL_I2C_TX_FIFO_LEVEL_EMPTY);
    DL_I2C_setControllerRXFIFOThreshold(I2C_INST, DL_I2C_RX_FIFO_LEVEL_BYTES_1);
    DL_I2C_enableControllerClockStretching(I2C_INST);


    /* Enable module */
    DL_I2C_enableController(I2C_INST);


}

static const DL_UART_Main_ClockConfig gIMU601ClockConfig = {
    .clockSel    = DL_UART_MAIN_CLOCK_BUSCLK,
    .divideRatio = DL_UART_MAIN_CLOCK_DIVIDE_RATIO_1
};

static const DL_UART_Main_Config gIMU601Config = {
    .mode        = DL_UART_MAIN_MODE_NORMAL,
    .direction   = DL_UART_MAIN_DIRECTION_TX_RX,
    .flowControl = DL_UART_MAIN_FLOW_CONTROL_NONE,
    .parity      = DL_UART_MAIN_PARITY_NONE,
    .wordLength  = DL_UART_MAIN_WORD_LENGTH_8_BITS,
    .stopBits    = DL_UART_MAIN_STOP_BITS_ONE
};

SYSCONFIG_WEAK void SYSCFG_DL_IMU601_init(void)
{
    DL_UART_Main_setClockConfig(IMU601_INST, (DL_UART_Main_ClockConfig *) &gIMU601ClockConfig);

    DL_UART_Main_init(IMU601_INST, (DL_UART_Main_Config *) &gIMU601Config);
    /*
     * Configure baud rate by setting oversampling and baud rate divisors.
     *  Target baud rate: 115200
     *  Actual baud rate: 115190.78
     */
    DL_UART_Main_setOversampling(IMU601_INST, DL_UART_OVERSAMPLING_RATE_16X);
    DL_UART_Main_setBaudRateDivisor(IMU601_INST, IMU601_IBRD_40_MHZ_115200_BAUD, IMU601_FBRD_40_MHZ_115200_BAUD);


    /* Configure Interrupts */
    DL_UART_Main_enableInterrupt(IMU601_INST,
                                 DL_UART_MAIN_INTERRUPT_RX);


    DL_UART_Main_enable(IMU601_INST);
}

static const DL_SPI_Config gSPI_OLED_config = {
    .mode        = DL_SPI_MODE_CONTROLLER,
    .frameFormat = DL_SPI_FRAME_FORMAT_MOTO3_POL0_PHA0,
    .parity      = DL_SPI_PARITY_NONE,
    .dataSize    = DL_SPI_DATA_SIZE_8,
    .bitOrder    = DL_SPI_BIT_ORDER_MSB_FIRST,
};

static const DL_SPI_ClockConfig gSPI_OLED_clockConfig = {
    .clockSel    = DL_SPI_CLOCK_BUSCLK,
    .divideRatio = DL_SPI_CLOCK_DIVIDE_RATIO_1
};

SYSCONFIG_WEAK void SYSCFG_DL_SPI_OLED_init(void) {
    DL_SPI_setClockConfig(SPI_OLED_INST, (DL_SPI_ClockConfig *) &gSPI_OLED_clockConfig);

    DL_SPI_init(SPI_OLED_INST, (DL_SPI_Config *) &gSPI_OLED_config);

    /* Configure Controller mode */
    /*
     * Set the bit rate clock divider to generate the serial output clock
     *     outputBitRate = (spiInputClock) / ((1 + SCR) * 2)
     *     20000000 = (80000000)/((1 + 1) * 2)
     */
    DL_SPI_setBitRateSerialClockDivider(SPI_OLED_INST, 1);
    /* Set RX and TX FIFO threshold levels */
    DL_SPI_setFIFOThreshold(SPI_OLED_INST, DL_SPI_RX_FIFO_LEVEL_1_2_FULL, DL_SPI_TX_FIFO_LEVEL_1_2_EMPTY);

    /* Enable module */
    DL_SPI_enable(SPI_OLED_INST);
}

SYSCONFIG_WEAK void SYSCFG_DL_SYSTICK_init(void)
{
    /* Initialize the period to 209.72 ms */
    DL_SYSTICK_init(16777216);
    /* Enable the SysTick and start counting */
    DL_SYSTICK_enable();
}

