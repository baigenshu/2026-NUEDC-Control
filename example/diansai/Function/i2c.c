#include "i2c.h"

/**
 * @brief  写单字节到寄存器
 *         时序: Start + [地址+W] + [寄存器地址] + [数据] + Stop
 */
void I2C_Write(uint8_t dev_addr, uint8_t reg_addr, uint8_t data)
{
    volatile uint32_t timeout;
    uint8_t tx_buf[2];

    tx_buf[0] = reg_addr;
    tx_buf[1] = data;

    /* 清空 TX FIFO */
    DL_I2C_flushControllerTXFIFO(HW_I2C);

    /* 将寄存器地址+数据填入 TX FIFO */
    DL_I2C_fillControllerTXFIFO(HW_I2C, tx_buf, 2);

    /* 启动写传输：目标地址 dev_addr，写方向，2字节 */
    DL_I2C_startControllerTransfer(HW_I2C, dev_addr,
        DL_I2C_CONTROLLER_DIRECTION_TX, 2);

    /* 等待传输完成（BUSY 清零） */
    timeout = I2C_TIMEOUT;
    while ((DL_I2C_getControllerStatus(HW_I2C) & DL_I2C_CONTROLLER_STATUS_BUSY) && timeout)
    {
        timeout--;
    }
}

/**
 * @brief  从寄存器读单字节
 *         时序: Start + [地址+W] + [寄存器地址] + RepeatedStart + [地址+R] + [数据] + Stop
 */
uint8_t I2C_Read(uint8_t dev_addr, uint8_t reg_addr)
{
    volatile uint32_t timeout;
    uint8_t data = 0;

    /* ==== 第一阶段：写寄存器地址 ==== */
    DL_I2C_flushControllerTXFIFO(HW_I2C);

    DL_I2C_fillControllerTXFIFO(HW_I2C, &reg_addr, 1);

    DL_I2C_startControllerTransfer(HW_I2C, dev_addr,
        DL_I2C_CONTROLLER_DIRECTION_TX, 1);

    timeout = I2C_TIMEOUT;
    while ((DL_I2C_getControllerStatus(HW_I2C) & DL_I2C_CONTROLLER_STATUS_BUSY) && timeout)
    {
        timeout--;
    }

    /* ==== 第二阶段：读取数据 ==== */
    DL_I2C_flushControllerRXFIFO(HW_I2C);

    DL_I2C_startControllerTransfer(HW_I2C, dev_addr,
        DL_I2C_CONTROLLER_DIRECTION_RX, 1);

    timeout = I2C_TIMEOUT;
    while ((DL_I2C_getControllerStatus(HW_I2C) & DL_I2C_CONTROLLER_STATUS_BUSY) && timeout)
    {
        timeout--;
    }

    /* 取出接收到的数据 */
    data = DL_I2C_receiveControllerData(HW_I2C);

    return data;
}

/**
 * @brief  从连续寄存器读取多字节（突发读取）
 *         用于高效读取 MPU6050 全部加速度/陀螺仪数据
 */
void I2C_ReadMulti(uint8_t dev_addr, uint8_t reg_addr, uint8_t *buf, uint8_t len)
{
    volatile uint32_t timeout;
    uint8_t i;

    if (len == 0) return;

    /* ==== 第一阶段：写起始寄存器地址 ==== */
    DL_I2C_flushControllerTXFIFO(HW_I2C);

    DL_I2C_fillControllerTXFIFO(HW_I2C, &reg_addr, 1);

    DL_I2C_startControllerTransfer(HW_I2C, dev_addr,
        DL_I2C_CONTROLLER_DIRECTION_TX, 1);

    timeout = I2C_TIMEOUT;
    while ((DL_I2C_getControllerStatus(HW_I2C) & DL_I2C_CONTROLLER_STATUS_BUSY) && timeout)
    {
        timeout--;
    }

    /* ==== 第二阶段：连续读取 len 字节 ==== */
    DL_I2C_flushControllerRXFIFO(HW_I2C);

    DL_I2C_startControllerTransfer(HW_I2C, dev_addr,
        DL_I2C_CONTROLLER_DIRECTION_RX, len);

    timeout = I2C_TIMEOUT;
    while ((DL_I2C_getControllerStatus(HW_I2C) & DL_I2C_CONTROLLER_STATUS_BUSY) && timeout)
    {
        timeout--;
    }

    /* 逐个取出接收到的数据 */
    for (i = 0; i < len; i++)
    {
        buf[i] = DL_I2C_receiveControllerData(HW_I2C);
    }
}