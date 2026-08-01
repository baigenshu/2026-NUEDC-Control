/**
 * @file vision_uart.c
 * @brief VISION_UART (UART0) 收包状态机：球位 0x02、定点 0x12、启停 0x13
 */
#include "vision_uart.h"
#include "ti_msp_dl_config.h"

typedef enum {
    RX_IDLE = 0,
    RX_GOT_AA,
    RX_BODY, /* 收 type 起共 BALL_FRAME_BODY_LEN 字节，再收 csum */
} rx_state_t;

static volatile uint32_t s_ms;
static volatile uint32_t s_last_ball_ms;
static volatile uint32_t s_ball_cnt;
static volatile uint32_t s_drop_cnt;

static rx_state_t s_st;
static uint8_t    s_buf[BALL_FRAME_LEN];
static uint8_t    s_n; /* 已写入 s_buf 的字节数 */

static ball_frame_t        s_ball;
static volatile bool       s_ball_new;
static ball_setpoint_cmd_t s_sp;
static volatile bool       s_sp_new;
static ball_control_cmd_t  s_control;
static volatile bool       s_control_new;

static bool frame_checksum_ok(const uint8_t *raw)
{
    uint8_t sum = 0u;
    unsigned i;

    if (!raw || raw[0] != BALL_FRAME_MAGIC0 || raw[1] != BALL_FRAME_MAGIC1)
        return false;
    for (i = BALL_FRAME_BODY_OFF; i < BALL_FRAME_CSUM_OFF; i++)
        sum = (uint8_t)(sum + raw[i]);
    return sum == raw[BALL_FRAME_CSUM_OFF];
}

static bool parse_setpoint(const uint8_t *raw, ball_setpoint_cmd_t *out)
{
    if (!raw || !out)
        return false;
    if (raw[2] != BALL_CMD_TYPE_SETPOINT)
        return false;
    if (!frame_checksum_ok(raw))
        return false;

    /* body: type flags target_mm i16 + pad… — 单位整 mm */
    out->target_mm =
        (int16_t)((uint16_t)raw[4] | ((uint16_t)raw[5] << 8));
    out->valid = true;
    return true;
}

static bool parse_control(const uint8_t *raw, ball_control_cmd_t *out)
{
    if (!raw || !out || raw[2] != BALL_FRAME_TYPE_CONTROL)
        return false;
    if (!frame_checksum_ok(raw) || raw[3] > BALL_CONTROL_ACTION_PRESET)
        return false;

    out->action = raw[3];
    out->valid = true;
    return true;
}

static void deliver_frame(void)
{
    ball_frame_t f;
    ball_setpoint_cmd_t sp;
    ball_control_cmd_t control;

    if (s_buf[2] == BALL_FRAME_TYPE) {
        if (ball_frame_parse(s_buf, &f)) {
            s_ball = f;
            s_ball_new = true;
            s_last_ball_ms = s_ms;
            s_ball_cnt++;
        } else {
            s_drop_cnt++;
        }
        return;
    }

    if (s_buf[2] == BALL_CMD_TYPE_SETPOINT) {
        sp.valid = false;
        if (parse_setpoint(s_buf, &sp)) {
            s_sp = sp;
            s_sp_new = true;
        } else {
            s_drop_cnt++;
        }
        return;
    }

    if (s_buf[2] == BALL_FRAME_TYPE_CONTROL) {
        control.valid = false;
        if (parse_control(s_buf, &control)) {
            s_control = control;
            s_control_new = true;
        } else {
            s_drop_cnt++;
        }
        return;
    }

    /* 0x01 云台等：整帧丢弃 */
    s_drop_cnt++;
}

static void rx_byte(uint8_t b)
{
    switch (s_st) {
    case RX_IDLE:
        if (b == BALL_FRAME_MAGIC0) {
            s_buf[0] = b;
            s_n = 1;
            s_st = RX_GOT_AA;
        }
        break;

    case RX_GOT_AA:
        if (b == BALL_FRAME_MAGIC1) {
            s_buf[1] = b;
            s_n = 2;
            s_st = RX_BODY;
        } else if (b == BALL_FRAME_MAGIC0) {
            s_buf[0] = b;
            s_n = 1;
            /* 保持 GOT_AA */
        } else {
            s_n = 0;
            s_st = RX_IDLE;
        }
        break;

    case RX_BODY:
        s_buf[s_n++] = b;
        if (s_n >= BALL_FRAME_LEN) {
            deliver_frame();
            s_n = 0;
            s_st = RX_IDLE;
        }
        break;

    default:
        s_n = 0;
        s_st = RX_IDLE;
        break;
    }
}

void VisionUart_Init(void)
{
    s_ms = 0;
    s_last_ball_ms = 0;
    s_ball_cnt = 0;
    s_drop_cnt = 0;
    s_st = RX_IDLE;
    s_n = 0;
    s_ball_new = false;
    s_sp_new = false;
    s_control_new = false;
    s_ball.valid = false;
    s_sp.valid = false;
    s_control.valid = false;

    /* UART 已在 SYSCFG_DL_VISION_UART_init 中使能 */
    NVIC_EnableIRQ(VISION_UART_INST_INT_IRQN);
    DL_UART_Main_enableInterrupt(VISION_UART_INST, DL_UART_MAIN_INTERRUPT_RX);
}

void VisionUart_Poll(void)
{
    while (!DL_UART_Main_isRXFIFOEmpty(VISION_UART_INST)) {
        uint8_t b = (uint8_t)DL_UART_Main_receiveData(VISION_UART_INST);
        rx_byte(b);
    }
}

void VisionUart_OnMsTick(void)
{
    s_ms++;
}

bool VisionUart_TakeBallFrame(ball_frame_t *out)
{
    if (!out || !s_ball_new)
        return false;
    *out = s_ball;
    s_ball_new = false;
    return true;
}

bool VisionUart_TakeSetpoint(ball_setpoint_cmd_t *out)
{
    if (!out || !s_sp_new)
        return false;
    *out = s_sp;
    s_sp_new = false;
    return true;
}

bool VisionUart_TakeControl(ball_control_cmd_t *out)
{
    if (!out || !s_control_new)
        return false;
    *out = s_control;
    s_control_new = false;
    return true;
}

uint32_t VisionUart_MsSinceBall(void)
{
    uint32_t now = s_ms;
    uint32_t last = s_last_ball_ms;
    if (s_ball_cnt == 0u)
        return 0x7FFFFFFFu;
    return now - last;
}

bool VisionUart_BallLinkOk(void)
{
    return VisionUart_MsSinceBall() <= (uint32_t)BALL_UART_TIMEOUT_MS;
}

uint32_t VisionUart_GetBallFrameCount(void)
{
    return s_ball_cnt;
}

uint32_t VisionUart_GetDropCount(void)
{
    return s_drop_cnt;
}

void VISION_UART_INST_IRQHandler(void)
{
    switch (DL_UART_Main_getPendingInterrupt(VISION_UART_INST)) {
    case DL_UART_MAIN_IIDX_RX:
        while (!DL_UART_Main_isRXFIFOEmpty(VISION_UART_INST)) {
            uint8_t b = (uint8_t)DL_UART_Main_receiveData(VISION_UART_INST);
            rx_byte(b);
        }
        break;
    default:
        break;
    }
}
