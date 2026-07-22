#ifndef TRACK_PROTO_H
#define TRACK_PROTO_H

#include <stdint.h>

/*
 * MaixCAM → 云台 跟踪帧（固定 15 字节）
 *
 *  0..1  0xAA 0x55
 *  2     type = 0x01
 *  3     flags bit0 = found
 *  4..5  err_x  int16 LE  像素，右正
 *  6..7  err_y  int16 LE  像素，下正
 *  8..9  pitch  int16 LE  °×100
 * 10..11 roll   int16 LE  °×100
 * 12..13 yaw    int16 LE  °×100
 * 14     checksum = sum(bytes[2..13]) & 0xFF
 */

#define TRACK_FRAME_LEN     15U
#define TRACK_FRAME_HDR0    0xAAU
#define TRACK_FRAME_HDR1    0x55U
#define TRACK_FRAME_TYPE    0x01U

typedef struct {
    uint8_t  found;    /* bit0 = TARGET_VALID */
    int16_t  err_x;    /* 像素，右正 */
    int16_t  err_y;    /* 像素，下正 */
    float    pitch;    /* ° */
    float    roll;     /* ° */
    float    yaw;      /* ° */
    uint32_t last_ms;  /* 最近合法帧的时间戳 (millis) */
    volatile uint8_t fresh; /* 1=有新帧，主循环读后清 0 */
} track_cmd_t;

extern volatile track_cmd_t g_track_cmd;

/* 在 UART0 RX 中断里每字节调用 */
void track_proto_on_byte(uint8_t b);

/* 拷贝最新一帧到 out；若有新帧返回 1 并清 fresh */
uint8_t track_proto_take(track_cmd_t *out);

#endif /* TRACK_PROTO_H */
