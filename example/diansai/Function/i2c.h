#ifndef __I2C_H
#define __I2C_H

#include "ti_msp_dl_config.h"
#include <stdint.h>

/* I2C 实例（来自 SysConfig） */
#define HW_I2C    I2C_MPU6050_INST

/* 超时计数 */
#define I2C_TIMEOUT  100000

/* ========== 硬件 I2C API ========== */

/**
 * @brief  写单字节到寄存器
 * @param  dev_addr  7位设备地址（如 MPU6050 为 0x68）
 * @param  reg_addr  寄存器地址
 * @param  data      要写入的数据
 */
void I2C_Write(uint8_t dev_addr, uint8_t reg_addr, uint8_t data);

/**
 * @brief  从寄存器读单字节
 * @param  dev_addr  7位设备地址
 * @param  reg_addr  寄存器地址
 * @return 读取到的数据
 */
uint8_t I2C_Read(uint8_t dev_addr, uint8_t reg_addr);

/**
 * @brief  从连续寄存器读取多字节（突发读取）
 * @param  dev_addr  7位设备地址
 * @param  reg_addr  起始寄存器地址
 * @param  buf       数据缓冲区
 * @param  len       读取字节数
 */
void I2C_ReadMulti(uint8_t dev_addr, uint8_t reg_addr, uint8_t *buf, uint8_t len);

#endif