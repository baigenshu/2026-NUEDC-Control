/**
 * @file ball_proto.h
 * @brief 视觉钢珠位置 UART 协议（type=0x02）· 与 maixcam/opencv 对齐
 *
 * 物理层：115200 8N1 · 3.3V · 视觉 TX → 主控 RX
 * 帧长：13 字节定长 · 小端
 * 位置字段：i16，单位 **1 mm**（整毫米，抑亚毫米抖）
 * 文档：apps/balance/docs/vision_proto.md
 */
#ifndef BALL_PROTO_H
#define BALL_PROTO_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- framing ---------- */
#define BALL_FRAME_MAGIC0           (0xAAu)
#define BALL_FRAME_MAGIC1           (0x55u)
#define BALL_FRAME_TYPE             (0x02u)   /* 球位；0x01=云台红点，本板忽略 */
#define BALL_FRAME_TYPE_TRACK       (0x01u)
#define BALL_FRAME_TYPE_SETPOINT    (0x12u)   /* 停球定点命令，布局见 vision_proto */
#define BALL_FRAME_TYPE_CONTROL     (0x13u)   /* 启停：flags/action=0 停止，1 校零启动 */

#define BALL_FRAME_LEN              (13u)
#define BALL_FRAME_BODY_LEN         (10u)     /* type..mode，不含 magic/csum */
#define BALL_FRAME_BODY_OFF         (2u)      /* body 起始下标 */
#define BALL_FRAME_CSUM_OFF         (12u)

/* flags */
#define BALL_FLAG_FOUND             (0x01u)

/* 检测 mode（遥测） */
#define BALL_MODE_BRI               (0u)
#define BALL_MODE_DRK               (1u)
#define BALL_MODE_AUT               (2u)

/* 业务门限（可按实装改） */
#ifndef BALL_CONF_MIN
#define BALL_CONF_MIN               (30u)
#endif

#ifndef BALL_UART_TIMEOUT_MS
#define BALL_UART_TIMEOUT_MS        (100u)
#endif

#ifndef BALL_UART_BAUD
#define BALL_UART_BAUD              (115200u)
#endif

/**
 * 一帧解析结果（主机序）
 * pos_mm：相对 O，单位 **1 mm**（与视觉量化一致）
 */
typedef struct {
    bool     valid;       /* 通过 framing + checksum + type */
    bool     found;       /* flags.bit0 */
    int16_t  pos_mm;      /* 整 mm */
    int16_t  cx;          /* 像素，调试 */
    int16_t  cy;
    uint8_t  conf;        /* 0..100 */
    uint8_t  mode;        /* 0/1/2 */
} ball_frame_t;

/** 控制可用：valid && found && conf >= BALL_CONF_MIN */
static inline bool ball_frame_usable(const ball_frame_t *f)
{
    return f != 0 && f->valid && f->found && (f->conf >= (uint8_t)BALL_CONF_MIN);
}

/** 整 mm → 内部控制 0.01 mm */
static inline int32_t ball_pos_to_mm_x100(int16_t pos_mm)
{
    return (int32_t)pos_mm * 100;
}

/** 0.01 mm → 整 mm（定点命令等） */
static inline int16_t ball_mm_x100_to_mm(int32_t mm_x100)
{
    if (mm_x100 >= 0)
        return (int16_t)((mm_x100 + 50) / 100);
    return (int16_t)((mm_x100 - 50) / 100);
}

/**
 * 校验并解析定长 13 字节帧。
 * @param raw  至少 BALL_FRAME_LEN 字节
 * @param out  输出；失败时 valid=false
 * @return true 表示 type=0x02 且 checksum 正确
 */
static inline bool ball_frame_parse(const uint8_t *raw, ball_frame_t *out)
{
    uint8_t sum;
    unsigned i;

    if (out) {
        out->valid = false;
        out->found = false;
        out->pos_mm = 0;
        out->cx = 0;
        out->cy = 0;
        out->conf = 0;
        out->mode = 0;
    }
    if (!raw || !out)
        return false;
    if (raw[0] != BALL_FRAME_MAGIC0 || raw[1] != BALL_FRAME_MAGIC1)
        return false;
    if (raw[2] != BALL_FRAME_TYPE)
        return false;

    sum = 0;
    for (i = BALL_FRAME_BODY_OFF; i < BALL_FRAME_CSUM_OFF; i++)
        sum = (uint8_t)(sum + raw[i]);
    if (sum != raw[BALL_FRAME_CSUM_OFF])
        return false;

    out->found  = (raw[3] & BALL_FLAG_FOUND) != 0;
    out->pos_mm = (int16_t)((uint16_t)raw[4] | ((uint16_t)raw[5] << 8));
    out->cx     = (int16_t)((uint16_t)raw[6] | ((uint16_t)raw[7] << 8));
    out->cy     = (int16_t)((uint16_t)raw[8] | ((uint16_t)raw[9] << 8));
    out->conf   = raw[10];
    out->mode   = raw[11];
    out->valid  = true;
    return true;
}

/**
 * 计算 body 校验（调试/PC 回放用）
 * body 指向 type 字节，长度 BALL_FRAME_BODY_LEN
 */
static inline uint8_t ball_frame_checksum(const uint8_t *body)
{
    uint8_t sum = 0;
    unsigned i;
    if (!body)
        return 0;
    for (i = 0; i < BALL_FRAME_BODY_LEN; i++)
        sum = (uint8_t)(sum + body[i]);
    return sum;
}

#ifdef __cplusplus
}
#endif

#endif /* BALL_PROTO_H */
