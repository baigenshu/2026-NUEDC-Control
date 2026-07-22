#include "track_proto.h"
#include "delay.h"

volatile track_cmd_t g_track_cmd;

static uint8_t  rx_buf[TRACK_FRAME_LEN];
static uint8_t  rx_idx     = 0;
static uint8_t  last_byte  = 0;
static uint8_t  receiving  = 0;

static int16_t le_i16(const uint8_t *p)
{
    return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static void parse_frame(const uint8_t *buf)
{
    uint8_t sum = 0;
    uint8_t i;

    if (buf[2] != TRACK_FRAME_TYPE)
        return;

    for (i = 2; i < 14; i++)
        sum = (uint8_t)(sum + buf[i]);

    if (sum != buf[14])
        return;

    g_track_cmd.found   = (buf[3] & 0x01U) ? 1U : 0U;
    g_track_cmd.err_x   = le_i16(&buf[4]);
    g_track_cmd.err_y   = le_i16(&buf[6]);
    g_track_cmd.pitch   = (float)le_i16(&buf[8])  / 100.0f;
    g_track_cmd.roll    = (float)le_i16(&buf[10]) / 100.0f;
    g_track_cmd.yaw     = (float)le_i16(&buf[12]) / 100.0f;
    g_track_cmd.last_ms = millis();
    g_track_cmd.fresh   = 1U;
}

void track_proto_on_byte(uint8_t b)
{
    if (!receiving) {
        if (last_byte == TRACK_FRAME_HDR0 && b == TRACK_FRAME_HDR1) {
            rx_buf[0] = TRACK_FRAME_HDR0;
            rx_buf[1] = TRACK_FRAME_HDR1;
            rx_idx    = 2;
            receiving = 1;
        }
        last_byte = b;
        return;
    }

    rx_buf[rx_idx++] = b;
    last_byte        = b;

    if (rx_idx >= TRACK_FRAME_LEN) {
        receiving = 0;
        rx_idx    = 0;
        parse_frame(rx_buf);
    }
}

uint8_t track_proto_take(track_cmd_t *out)
{
    if (out == 0 || g_track_cmd.fresh == 0)
        return 0;

    out->found   = g_track_cmd.found;
    out->err_x   = g_track_cmd.err_x;
    out->err_y   = g_track_cmd.err_y;
    out->pitch   = g_track_cmd.pitch;
    out->roll    = g_track_cmd.roll;
    out->yaw     = g_track_cmd.yaw;
    out->last_ms = g_track_cmd.last_ms;
    out->fresh   = 1;
    g_track_cmd.fresh = 0;
    return 1;
}
