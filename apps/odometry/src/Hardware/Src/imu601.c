#include "imu601.h"
#include "bsp_systick.h"

/*
 * UART1 RX → DMA CH0 → ring buffer → main-loop frame parse
 * Chunk size 32B, re-arm on DMA_DONE_RX
 */

#define IMU_RING_SIZE   256u
#define IMU_DMA_CHUNK   32u

volatile IMU601_Attitude_t IMU601_Attitude;
volatile uint32_t IMU601_FrameCount;

static uint8_t s_ring[IMU_RING_SIZE];
static volatile uint16_t s_wr;
static volatile uint16_t s_rd;
static uint8_t s_dma_chunk[IMU_DMA_CHUNK];

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

static void ring_push(const uint8_t *p, uint16_t n)
{
    uint16_t i;
    for (i = 0; i < n; i++) {
        uint16_t next = (uint16_t)((s_wr + 1u) % IMU_RING_SIZE);
        if (next == s_rd)
            break; /* overrun: drop */
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

        /* hunt AA 55 */
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
            ring_drop(1); /* bad frame, resync */
        }
    }
}

void IMU601_INST_IRQHandler(void)
{
    switch (DL_UART_Main_getPendingInterrupt(IMU601_INST)) {
    case DL_UART_MAIN_IIDX_DMA_DONE_RX:
        ring_push(s_dma_chunk, IMU_DMA_CHUNK);
        dma_arm();
        break;
    default:
        break;
    }
}

void IMU601_Init(void)
{
    static const uint8_t IMU_reset[] = {
        0xAA, 0x55, 0x60, 0x12, 0x00, 0x72
    };
    static const uint8_t IMU_cali[] = {
        0xAA, 0x55, 0x60, 0x14, 0x04, 0x66, 0xE6, 0xB4, 0x43, 0xBB
    };

    s_wr = 0;
    s_rd = 0;
    IMU601_FrameCount = 0;
    IMU601_Attitude.yaw = 0.0f;
    IMU601_Attitude.pitch = 0.0f;
    IMU601_Attitude.roll = 0.0f;

    NVIC_EnableIRQ(IMU601_INST_INT_IRQN);
    dma_arm();

    uart_send(IMU_reset, (uint8_t)sizeof(IMU_reset));
    delay_ms(500);
    uart_send(IMU_cali, (uint8_t)sizeof(IMU_cali));
}
