#include "imu601.h"
#include "bsp_systick.h"

/*
 * 汇电籽-601 IMU 驱动 — UART 串口通信 (UART1)
 *
 * SysConfig 配置: UART1, TX=PA8, RX=PA9, Baud=115200, 8N1
 *   中断: RX (每收到 1 字节触发), 在 SysConfig 中已使能 (empty.syscfg)。
 *
 * 协议帧 (姿态数据, 固定 12 字节):
 *   0xAA 0x55 | ID(1) CMD(1) Len(1) | Data(6) | Checksum(1)
 *   Data = yaw(2,小端,无符号) pitch(2,小端,有符号) roll(2,小端,有符号), 均为 ×100
 *   Checksum = (ID + CMD + Len + Data[...]) & 0xFF
 *
 * 实现对齐参考工程 car_06_IMU601。
 */

/* ---- 全局姿态 (ISR 更新, 外部只读) ---- */
volatile IMU601_Attitude_t IMU601_Attitude;

/* ---- 接收缓冲与状态 (仿参考实现) ---- */
static uint8_t RX_buffer[12] = {0};
static uint8_t RX_index      = 0;
static uint8_t last_byte     = 0;
static uint8_t is_receiving  = 0;

/* ============ 底层: 通过 UART 发送单字节 ============ */
static inline void uart_putc(uint8_t c)
{
    DL_UART_transmitDataBlocking(IMU601_INST, c);
}

/* ============ 底层: 发送缓冲区 ============ */
static void uart_send(const uint8_t *buf, uint8_t len)
{
    uint8_t i;
    for (i = 0; i < len; i++) {
        uart_putc(buf[i]);
    }
}

/* ============ 计算校验和 (ID + CMD + Len + Data) ============ */
static uint8_t calc_checksum(const uint8_t *pkt, uint8_t len)
{
    uint8_t sum = 0;
    uint8_t i;
    for (i = 2; i < len - 1; i++) {
        sum += pkt[i];
    }
    return sum;
}

/* ============ 构建并发送命令帧 ============ */
void IMU601_SendCmd(uint8_t id, uint8_t cmd, const uint8_t *data, uint8_t len)
{
    uint8_t pkt[64];
    uint8_t i;

    pkt[0] = 0xAA;
    pkt[1] = 0x55;
    pkt[2] = id;
    pkt[3] = cmd;
    pkt[4] = len;

    for (i = 0; i < len; i++) {
        pkt[5 + i] = data[i];
    }

    pkt[5 + len] = calc_checksum(pkt, 6 + len);
    uart_send(pkt, 6 + len);
}

/* ============ 解析姿态数据 (Data 区 6 字节, 小端) ============ */
static void parse_attitude(const uint8_t *payload)
{
    uint16_t yaw_raw   = ((uint16_t)payload[1] << 8) | payload[0];
    int16_t  pitch_raw = (int16_t)(((uint16_t)payload[3] << 8) | payload[2]);
    int16_t  roll_raw  = (int16_t)(((uint16_t)payload[5] << 8) | payload[4]);

    IMU601_Attitude.yaw   = yaw_raw   / 100.0f;
    IMU601_Attitude.pitch = pitch_raw / 100.0f;
    IMU601_Attitude.roll  = roll_raw  / 100.0f;
}

/* ============ 校验并解析完整 12 字节帧 ============ */
static void parse_imu601_data(void)
{
    uint8_t checksum = 0;
    uint8_t i;
    for (i = 2; i < 11; i++) {
        checksum += RX_buffer[i];
    }
    if (checksum == RX_buffer[11]) {
        parse_attitude(&RX_buffer[5]);
    }
}

/* ============ UART 接收中断处理 (每收到 1 字节触发) ============ */
void IMU601_INST_IRQHandler(void)
{
    switch (DL_UART_getPendingInterrupt(IMU601_INST)) {
    case DL_UART_IIDX_RX:
    {
        RX_buffer[RX_index] = (uint8_t)DL_UART_receiveData(IMU601_INST);

        /* 检测帧头 0xAA 0x55 */
        if (RX_buffer[RX_index] == 0x55 && last_byte == 0xAA) {
            RX_index = 2;
            RX_buffer[0] = 0xAA;
            RX_buffer[1] = 0x55;
            is_receiving = 1;
        } else {
            RX_index++;
        }

        last_byte = RX_buffer[RX_index - 1];

        /* 收满 12 字节, 校验并解析 */
        if (RX_index >= 12) {
            RX_index = 0;
            is_receiving = 0;
            parse_imu601_data();
        }
        break;
    }
    default:
        break;
    }
}

/* ============ 初始化 ============ */
void IMU601_Init(void)
{
    RX_index = 0;
    last_byte = 0;
    is_receiving = 0;
    IMU601_Attitude.yaw   = 0.0f;
    IMU601_Attitude.pitch = 0.0f;
    IMU601_Attitude.roll  = 0.0f;

    /* 复位命令: AA 55 60 12 00 72 */
    {
        uint8_t reset[] = {0xAA, 0x55, 0x60, 0x12, 0x00, 0x72};
        uart_send(reset, sizeof(reset));
    }
    delay_ms(500);

    /* 校准命令: AA 55 60 14 04 CD 4C B4 43 88 */
    {
        uint8_t cali[] = {0xAA, 0x55, 0x60, 0x14, 0x04,
                          0xCD, 0x4C, 0xB4, 0x43, 0x88};
        uart_send(cali, sizeof(cali));
    }

    /* SysConfig 已使能 RX 中断 (按字节接收), 这里只需使能 NVIC */
    NVIC_EnableIRQ(IMU601_INST_INT_IRQN);
}

void IMU601_SetRate(uint16_t freq_hz)
{
    uint8_t data = (uint8_t)freq_hz;
    IMU601_SendCmd(0x60, 0x13, &data, 1);
}

void IMU601_UnlockBaud(void)
{
    uint8_t i;
    for (i = 0; i < 20; i++) {
        uart_putc(0x55);
        delay_ms(2);
    }
}
