#include "imu601.h"
#include "bsp_systick.h"

/*
 * 汇电籽-601
 * 软复位: AA 55 60 12 00 72
 * 校准 yaw: AA 55 60 14 04 66 E6 B4 43 BB
 * 姿态帧: AA 55 | 60 | 01 | 06 | yaw_le u16 | pitch_le s16 | roll_le s16 | sum
 */

volatile IMU601_Attitude_t IMU601_Attitude;
volatile uint32_t IMU601_FrameCount;

static uint8_t RX_buffer[12];
static uint8_t RX_index;
static uint8_t last_byte;
static uint8_t is_receiving;

static void UART_send_buffer(const uint8_t *buf, uint8_t len)
{
    uint8_t i;
    for (i = 0; i < len; i++) {
        DL_UART_transmitDataBlocking(IMU601_INST, buf[i]);
    }
}

/* payload: 6 字节, little-endian: u16 yaw, s16 pitch, s16 roll, /100 → ° */
static void parse_attitude_only(const uint8_t *payload)
{
    uint16_t yaw_raw;
    int16_t pitch_raw;
    int16_t roll_raw;

    yaw_raw   = (uint16_t)(((uint16_t)payload[1] << 8) | payload[0]);
    pitch_raw = (int16_t)(((uint16_t)payload[3] << 8) | payload[2]);
    roll_raw  = (int16_t)(((uint16_t)payload[5] << 8) | payload[4]);

    IMU601_Attitude.yaw   = yaw_raw   / 100.0f;
    IMU601_Attitude.pitch = pitch_raw / 100.0f;
    IMU601_Attitude.roll  = roll_raw  / 100.0f;
}

static void parse_imu601_data(void)
{
    uint8_t checksum = 0;
    uint8_t i;

    for (i = 2; i < 11; i++) {
        checksum += RX_buffer[i];
    }

    /*
     * 与 car_06 一致: 校验通过即解析。
     * 文档姿态帧为 ID=0x60 CMD=0x01 LEN=0x06；
     * 不强制 CMD（部分固件上报码不同），强制 CMD 会导致永远收不到 yaw。
     */
    if (checksum == RX_buffer[11] && RX_buffer[4] == 0x06u) {
        parse_attitude_only(&RX_buffer[5]);
        IMU601_FrameCount++;
    }
}

void IMU601_INST_IRQHandler(void)
{
    switch (DL_UART_getPendingInterrupt(IMU601_INST)) {
    case DL_UART_IIDX_RX:
        RX_buffer[RX_index] = (uint8_t)DL_UART_receiveData(IMU601_INST);

        if (RX_buffer[RX_index] == 0x55u && last_byte == 0xAAu) {
            RX_index = 2;
            RX_buffer[0] = 0xAA;
            RX_buffer[1] = 0x55;
            is_receiving = 1;
        } else {
            RX_index++;
        }

        last_byte = RX_buffer[RX_index - 1u];

        if (RX_index >= 12u) {
            RX_index = 0;
            is_receiving = 0;
            parse_imu601_data();
        }
        break;

    default:
        break;
    }
}

void IMU601_Init(void)
{
    /* 软复位 */
    static const uint8_t IMU_reset[] = {
        0xAA, 0x55, 0x60, 0x12, 0x00, 0x72
    };
    /* 校准 yaw（文档给定数值） */
    static const uint8_t IMU_cali[] = {
        0xAA, 0x55, 0x60, 0x14, 0x04, 0x66, 0xE6, 0xB4, 0x43, 0xBB
    };

    RX_index = 0;
    last_byte = 0;
    is_receiving = 0;
    IMU601_FrameCount = 0;
    IMU601_Attitude.yaw = 0.0f;
    IMU601_Attitude.pitch = 0.0f;
    IMU601_Attitude.roll = 0.0f;

    /* 先开中断再发命令，避免校准后短时间内的上报被丢掉 */
    NVIC_EnableIRQ(IMU601_INST_INT_IRQN);

    UART_send_buffer(IMU_reset, (uint8_t)sizeof(IMU_reset));
    delay_ms(500);
    UART_send_buffer(IMU_cali, (uint8_t)sizeof(IMU_cali));
}
