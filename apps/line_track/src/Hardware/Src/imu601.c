#include "imu601.h"
#include "bsp_systick.h"

/*
 * UART1 RX → DMA CH0 → ring → IMU601_Poll()
 * 上电: 软复位 → 等稳定 → 校准 yaw → 等收敛 → 清缓冲
 */

#define IMU_RING_SIZE   256u
#define IMU_DMA_CHUNK   32u

/* 校准后模块静止收敛时间 (ms) */
#define IMU_RESET_WAIT_MS   800u
#define IMU_CALI_WAIT_MS    1500u

volatile IMU601_Attitude_t IMU601_Attitude;
volatile uint32_t IMU601_FrameCount;
volatile uint32_t IMU601_DmaIrqCount;

static uint8_t s_ring[IMU_RING_SIZE];
static volatile uint16_t s_wr;
static volatile uint16_t s_rd;
static uint8_t s_dma_chunk[IMU_DMA_CHUNK];

static const uint8_t IMU_CMD_RESET[] = {
    0xAA, 0x55, 0x60, 0x12, 0x00, 0x72
};
/* 文档校准 yaw: AA 55 60 14 04 66 E6 B4 43 BB */
static const uint8_t IMU_CMD_CALI[] = {
    0xAA, 0x55, 0x60, 0x14, 0x04, 0x66, 0xE6, 0xB4, 0x43, 0xBB
};

static void uart_send(const uint8_t *buf, uint8_t len)
{
    uint8_t i;
    for (i = 0; i < len; i++)
        DL_UART_transmitDataBlocking(IMU601_INST, buf[i]);
}

static void dma_arm(void)
{
    DL_DMA_disableChannel(DMA, DMA_CH0_CHAN_ID);
    DL_DMA_setSrcAddr(DMA, DMA_CH0_CHAN_ID, (uint32_t)&IMU601_INST->RXDATA);
    DL_DMA_setDestAddr(DMA, DMA_CH0_CHAN_ID, (uint32_t)s_dma_chunk);
    DL_DMA_setTransferSize(DMA, DMA_CH0_CHAN_ID, IMU_DMA_CHUNK);
    DL_DMA_enableChannel(DMA, DMA_CH0_CHAN_ID);
}

static void ring_clear(void)
{
    s_wr = 0;
    s_rd = 0;
}

static void ring_push(const uint8_t *p, uint16_t n)
{
    uint16_t i;
    for (i = 0; i < n; i++) {
        uint16_t next = (uint16_t)((s_wr + 1u) % IMU_RING_SIZE);
        if (next == s_rd)
            break;
        s_ring[s_wr] = p[i];
        s_wr = next;
    }
}

static uint16_t ring_avail(void)
{
    if (s_wr >= s_rd)
        return (uint16_t)(s_wr - s_rd);
    return (uint16_t)(IMU_RING_SIZE - s_rd + s_wr);
}

static uint8_t ring_peek(uint16_t off)
{
    return s_ring[(s_rd + off) % IMU_RING_SIZE];
}

static void ring_drop(uint16_t n)
{
    s_rd = (uint16_t)((s_rd + n) % IMU_RING_SIZE);
}

static void parse_attitude_only(const uint8_t *payload)
{
    uint16_t yaw_raw =
        (uint16_t)(((uint16_t)payload[1] << 8) | payload[0]);
    int16_t pitch_raw =
        (int16_t)(((uint16_t)payload[3] << 8) | payload[2]);
    int16_t roll_raw =
        (int16_t)(((uint16_t)payload[5] << 8) | payload[4]);

    IMU601_Attitude.yaw   = yaw_raw   / 100.0f;
    IMU601_Attitude.pitch = pitch_raw / 100.0f;
    IMU601_Attitude.roll  = roll_raw  / 100.0f;
}

void IMU601_Poll(void)
{
    while (ring_avail() >= 12u) {
        uint8_t i, sum;
        uint8_t frame[12];

        if (ring_peek(0) != 0xAAu || ring_peek(1) != 0x55u) {
            ring_drop(1);
            continue;
        }

        for (i = 0; i < 12u; i++)
            frame[i] = ring_peek(i);

        sum = 0;
        for (i = 2; i < 11u; i++)
            sum += frame[i];

        if (sum == frame[11] && frame[4] == 0x06u) {
            parse_attitude_only(&frame[5]);
            IMU601_FrameCount++;
            ring_drop(12);
        } else {
            ring_drop(1);
        }
    }
}

void IMU601_INST_IRQHandler(void)
{
    switch (DL_UART_Main_getPendingInterrupt(IMU601_INST)) {
    case DL_UART_MAIN_IIDX_DMA_DONE_RX:
        IMU601_DmaIrqCount++;
        ring_push(s_dma_chunk, IMU_DMA_CHUNK);
        dma_arm();
        break;
    default:
        break;
    }
}

void IMU601_Calibrate(void)
{
    /* 发送校准命令，模块须水平静止 */
    uart_send(IMU_CMD_CALI, (uint8_t)sizeof(IMU_CMD_CALI));
    delay_ms(IMU_CALI_WAIT_MS);

    /* 丢掉校准过程中的旧帧，重新累计 */
    ring_clear();
    IMU601_FrameCount = 0;
    IMU601_Attitude.yaw = 0.0f;
    IMU601_Attitude.pitch = 0.0f;
    IMU601_Attitude.roll = 0.0f;
}

void IMU601_Init(void)
{
    ring_clear();
    IMU601_FrameCount = 0;
    IMU601_Attitude.yaw = 0.0f;
    IMU601_Attitude.pitch = 0.0f;
    IMU601_Attitude.roll = 0.0f;

    NVIC_EnableIRQ(IMU601_INST_INT_IRQN);
    dma_arm();

    /* 1) 软复位 */
    uart_send(IMU_CMD_RESET, (uint8_t)sizeof(IMU_CMD_RESET));
    delay_ms(IMU_RESET_WAIT_MS);

    /* 2) 每次上电都校准（静止） */
    IMU601_Calibrate();
}
