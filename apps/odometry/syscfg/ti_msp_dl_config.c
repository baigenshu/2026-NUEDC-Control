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

DL_TimerG_backupConfig gQEI_ABackup;

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
    SYSCFG_DL_QEI_A_init();
    SYSCFG_DL_ODOM_TIM_init();
    SYSCFG_DL_DEBUG_UART_init();
    SYSCFG_DL_IMU601_init();
    SYSCFG_DL_DMA_init();
    SYSCFG_DL_SYSTICK_init();
    /* Ensure backup structures have no valid state */
	gQEI_ABackup.backupRdy 	= false;



}
/*
 * User should take care to save and restore register configuration in application.
 * See Retention Configuration section for more details.
 */
SYSCONFIG_WEAK bool SYSCFG_DL_saveConfiguration(void)
{
    bool retStatus = true;

	retStatus &= DL_TimerG_saveConfiguration(QEI_A_INST, &gQEI_ABackup);

    return retStatus;
}


SYSCONFIG_WEAK bool SYSCFG_DL_restoreConfiguration(void)
{
    bool retStatus = true;

	retStatus &= DL_TimerG_restoreConfiguration(QEI_A_INST, &gQEI_ABackup, false);

    return retStatus;
}

SYSCONFIG_WEAK void SYSCFG_DL_initPower(void)
{
    DL_GPIO_reset(GPIOA);
    DL_GPIO_reset(GPIOB);
    DL_TimerG_reset(QEI_A_INST);
    DL_TimerG_reset(ODOM_TIM_INST);
    DL_UART_Main_reset(DEBUG_UART_INST);
    DL_UART_Main_reset(IMU601_INST);



    DL_GPIO_enablePower(GPIOA);
    DL_GPIO_enablePower(GPIOB);
    DL_TimerG_enablePower(QEI_A_INST);
    DL_TimerG_enablePower(ODOM_TIM_INST);
    DL_UART_Main_enablePower(DEBUG_UART_INST);
    DL_UART_Main_enablePower(IMU601_INST);


    delay_cycles(POWER_STARTUP_DELAY);
}

SYSCONFIG_WEAK void SYSCFG_DL_GPIO_init(void)
{

    DL_GPIO_initPeripheralInputFunction(GPIO_QEI_A_PHA_IOMUX,GPIO_QEI_A_PHA_IOMUX_FUNC);
    DL_GPIO_initPeripheralInputFunction(GPIO_QEI_A_PHB_IOMUX,GPIO_QEI_A_PHB_IOMUX_FUNC);

    DL_GPIO_initPeripheralOutputFunction(
        GPIO_DEBUG_UART_IOMUX_TX, GPIO_DEBUG_UART_IOMUX_TX_FUNC);
    DL_GPIO_initPeripheralInputFunction(
        GPIO_DEBUG_UART_IOMUX_RX, GPIO_DEBUG_UART_IOMUX_RX_FUNC);
    DL_GPIO_initPeripheralOutputFunction(
        GPIO_IMU601_IOMUX_TX, GPIO_IMU601_IOMUX_TX_FUNC);
    DL_GPIO_initPeripheralInputFunction(
        GPIO_IMU601_IOMUX_RX, GPIO_IMU601_IOMUX_RX_FUNC);

    DL_GPIO_initDigitalInputFeatures(GPIO_ENCODERB_E2A_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(GPIO_ENCODERB_E2B_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_setLowerPinsPolarity(GPIO_ENCODERB_PORT, DL_GPIO_PIN_0_EDGE_RISE_FALL |
		DL_GPIO_PIN_5_EDGE_RISE_FALL);
    DL_GPIO_clearInterruptStatus(GPIO_ENCODERB_PORT, GPIO_ENCODERB_E2A_PIN |
		GPIO_ENCODERB_E2B_PIN);
    DL_GPIO_enableInterrupt(GPIO_ENCODERB_PORT, GPIO_ENCODERB_E2A_PIN |
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


static const DL_TimerG_ClockConfig gQEI_AClockConfig = {
    .clockSel = DL_TIMER_CLOCK_BUSCLK,
    .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
    .prescale = 0U
};


SYSCONFIG_WEAK void SYSCFG_DL_QEI_A_init(void) {

    DL_TimerG_setClockConfig(
        QEI_A_INST, (DL_TimerG_ClockConfig *) &gQEI_AClockConfig);

    DL_TimerG_configQEI(QEI_A_INST, DL_TIMER_QEI_MODE_2_INPUT,
        DL_TIMER_CC_INPUT_INV_NOINVERT, DL_TIMER_CC_0_INDEX);
    DL_TimerG_configQEI(QEI_A_INST, DL_TIMER_QEI_MODE_2_INPUT,
        DL_TIMER_CC_INPUT_INV_NOINVERT, DL_TIMER_CC_1_INDEX);
    DL_TimerG_setLoadValue(QEI_A_INST, 65535);
    DL_TimerG_enableClock(QEI_A_INST);
}



/*
 * Timer clock configuration to be sourced by BUSCLK /  (40000000 Hz)
 * timerClkFreq = (timerClkSrc / (timerClkDivRatio * (timerClkPrescale + 1)))
 *   1000000 Hz = 40000000 Hz / (1 * (39 + 1))
 */
static const DL_TimerG_ClockConfig gODOM_TIMClockConfig = {
    .clockSel    = DL_TIMER_CLOCK_BUSCLK,
    .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
    .prescale    = 39U,
};

/*
 * Timer load value (where the counter starts from) is calculated as (timerPeriod * timerClockFreq) - 1
 * ODOM_TIM_INST_LOAD_VALUE = (10 ms * 1000000 Hz) - 1
 */
static const DL_TimerG_TimerConfig gODOM_TIMTimerConfig = {
    .period     = ODOM_TIM_INST_LOAD_VALUE,
    .timerMode  = DL_TIMER_TIMER_MODE_PERIODIC,
    .startTimer = DL_TIMER_START,
};

SYSCONFIG_WEAK void SYSCFG_DL_ODOM_TIM_init(void) {

    DL_TimerG_setClockConfig(ODOM_TIM_INST,
        (DL_TimerG_ClockConfig *) &gODOM_TIMClockConfig);

    DL_TimerG_initTimerMode(ODOM_TIM_INST,
        (DL_TimerG_TimerConfig *) &gODOM_TIMTimerConfig);
    DL_TimerG_enableInterrupt(ODOM_TIM_INST , DL_TIMERG_INTERRUPT_ZERO_EVENT);
    DL_TimerG_enableClock(ODOM_TIM_INST);





}


static const DL_UART_Main_ClockConfig gDEBUG_UARTClockConfig = {
    .clockSel    = DL_UART_MAIN_CLOCK_BUSCLK,
    .divideRatio = DL_UART_MAIN_CLOCK_DIVIDE_RATIO_1
};

static const DL_UART_Main_Config gDEBUG_UARTConfig = {
    .mode        = DL_UART_MAIN_MODE_NORMAL,
    .direction   = DL_UART_MAIN_DIRECTION_TX_RX,
    .flowControl = DL_UART_MAIN_FLOW_CONTROL_NONE,
    .parity      = DL_UART_MAIN_PARITY_NONE,
    .wordLength  = DL_UART_MAIN_WORD_LENGTH_8_BITS,
    .stopBits    = DL_UART_MAIN_STOP_BITS_ONE
};

SYSCONFIG_WEAK void SYSCFG_DL_DEBUG_UART_init(void)
{
    DL_UART_Main_setClockConfig(DEBUG_UART_INST, (DL_UART_Main_ClockConfig *) &gDEBUG_UARTClockConfig);

    DL_UART_Main_init(DEBUG_UART_INST, (DL_UART_Main_Config *) &gDEBUG_UARTConfig);
    /*
     * Configure baud rate by setting oversampling and baud rate divisors.
     *  Target baud rate: 115200
     *  Actual baud rate: 115190.78
     */
    DL_UART_Main_setOversampling(DEBUG_UART_INST, DL_UART_OVERSAMPLING_RATE_16X);
    DL_UART_Main_setBaudRateDivisor(DEBUG_UART_INST, DEBUG_UART_IBRD_40_MHZ_115200_BAUD, DEBUG_UART_FBRD_40_MHZ_115200_BAUD);



    DL_UART_Main_enable(DEBUG_UART_INST);
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
                                 DL_UART_MAIN_INTERRUPT_DMA_DONE_RX);

    /* Configure DMA Receive Event */
    DL_UART_Main_enableDMAReceiveEvent(IMU601_INST, DL_UART_DMA_INTERRUPT_RX);
    /* Configure FIFOs */
    DL_UART_Main_enableFIFOs(IMU601_INST);
    DL_UART_Main_setRXFIFOThreshold(IMU601_INST, DL_UART_RX_FIFO_LEVEL_1_2_FULL);
    DL_UART_Main_setTXFIFOThreshold(IMU601_INST, DL_UART_TX_FIFO_LEVEL_1_2_EMPTY);

    DL_UART_Main_enable(IMU601_INST);
}

static const DL_DMA_Config gDMA_CH0Config = {
    .transferMode   = DL_DMA_SINGLE_TRANSFER_MODE,
    .extendedMode   = DL_DMA_NORMAL_MODE,
    .destIncrement  = DL_DMA_ADDR_INCREMENT,
    .srcIncrement   = DL_DMA_ADDR_UNCHANGED,
    .destWidth      = DL_DMA_WIDTH_BYTE,
    .srcWidth       = DL_DMA_WIDTH_BYTE,
    .trigger        = IMU601_INST_DMA_TRIGGER,
    .triggerType    = DL_DMA_TRIGGER_TYPE_EXTERNAL,
};

SYSCONFIG_WEAK void SYSCFG_DL_DMA_CH0_init(void)
{
    DL_DMA_initChannel(DMA, DMA_CH0_CHAN_ID , (DL_DMA_Config *) &gDMA_CH0Config);
}
SYSCONFIG_WEAK void SYSCFG_DL_DMA_init(void){
    SYSCFG_DL_DMA_CH0_init();
}


SYSCONFIG_WEAK void SYSCFG_DL_SYSTICK_init(void)
{
    /* Initialize the period to 209.72 ms */
    DL_SYSTICK_init(16777216);
    /* Enable the SysTick and start counting */
    DL_SYSTICK_enable();
}

