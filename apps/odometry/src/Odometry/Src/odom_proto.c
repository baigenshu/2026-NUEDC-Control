#include "odom_proto.h"
#include "ti_msp_dl_config.h"

/*
 * Binary uplink on DEBUG UART0 (PA10/PA11 @115200)
 * Human debug text also uses this UART — avoid concurrent floods.
 */

#define HOST_UART   DEBUG_UART_INST

static uint8_t s_seq;
static uint8_t s_rx[8];
static uint8_t s_rx_n;

static void host_putc(uint8_t c)
{
    DL_UART_Main_transmitDataBlocking(HOST_UART, c);
}

static void host_write(const uint8_t *p, uint8_t n)
{
    uint8_t i;
    for (i = 0; i < n; i++)
        host_putc(p[i]);
}

static uint8_t crc8_sum(const uint8_t *p, uint8_t n)
{
    uint8_t s = 0, i;
    for (i = 0; i < n; i++)
        s += p[i];
    return s;
}

static void put_f32(uint8_t *dst, float v)
{
    union { float f; uint8_t b[4]; } u;
    u.f = v;
    dst[0] = u.b[0];
    dst[1] = u.b[1];
    dst[2] = u.b[2];
    dst[3] = u.b[3];
}

void OdomProto_Init(void)
{
    /* UART already configured in SYSCFG_DL_DEBUG_UART_init */
    s_seq = 0;
    s_rx_n = 0;
}

void OdomProto_SendState(const OdomState_t *s)
{
    uint8_t f[ODOM_FRAME_SIZE];

    f[0] = ODOM_FRAME_MAGIC0;
    f[1] = ODOM_FRAME_MAGIC1;
    f[2] = ODOM_FRAME_VER;
    f[3] = (uint8_t)(s->status & 0xFFu);
    put_f32(&f[4],  s->x);
    put_f32(&f[8],  s->y);
    put_f32(&f[12], s->theta);
    put_f32(&f[16], s->v);
    put_f32(&f[20], s->omega);
    f[24] = (uint8_t)(s->status & 0xFFu);
    f[25] = (uint8_t)((s->status >> 8) & 0xFFu);
    f[26] = s_seq++;
    f[27] = crc8_sum(&f[2], 25);
    host_write(f, ODOM_FRAME_SIZE);
}

void OdomProto_PollRx(void)
{
    while (!DL_UART_Main_isRXFIFOEmpty(HOST_UART)) {
        uint8_t b = (uint8_t)DL_UART_Main_receiveData(HOST_UART);
        if (s_rx_n < sizeof(s_rx))
            s_rx[s_rx_n++] = b;
        else
            s_rx_n = 0;
        (void)s_rx;
    }
}
