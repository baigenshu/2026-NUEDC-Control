#ifndef __ODOM_PROTO_H
#define __ODOM_PROTO_H

#include "odom.h"
#include <stdint.h>

/*
 * Host frame (little-endian), total 28 bytes:
 *   0-1  magic 0xAA 0x55
 *   2    ver   = 1
 *   3    flags (bit0 slip)
 *   4-7  x     float m
 *   8-11 y     float m
 *  12-15 theta float rad
 *  16-19 v     float m/s
 *  20-23 omega float rad/s
 *  24    status low
 *  25    status high (reserved)
 *  26    seq
 *  27    crc8  (sum of bytes [2..26] & 0xFF)
 *
 * Host command (6 bytes):
 *   AA 55 | CMD | 00 | 00 | CRC
 *   CMD: 0x01 = RESET_ODOM
 */

#define ODOM_FRAME_MAGIC0   0xAAu
#define ODOM_FRAME_MAGIC1   0x55u
#define ODOM_FRAME_VER      1u
#define ODOM_FRAME_SIZE     28u

#define ODOM_CMD_RESET      0x01u

void OdomProto_Init(void);
void OdomProto_PollRx(void);
void OdomProto_SendState(const OdomState_t *s);

#endif
