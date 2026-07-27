#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum LinkMsgType : uint8_t {
  LINK_MSG_PING = 0,
  LINK_MSG_PONG = 1,
  LINK_MSG_DATA = 2,
};

static const uint8_t LINK_PAYLOAD_MAX = 32;

struct __attribute__((packed)) LinkMsg {
  uint8_t type;
  uint8_t len;
  uint32_t seq;
  uint32_t millis_stamp;
  uint8_t payload[LINK_PAYLOAD_MAX];
};

inline void link_msg_init_ping(LinkMsg *msg, uint32_t seq, uint32_t ms) {
  memset(msg, 0, sizeof(LinkMsg));
  msg->type = LINK_MSG_PING;
  msg->len = 0;
  msg->seq = seq;
  msg->millis_stamp = ms;
}

inline void link_msg_init_data(LinkMsg *msg, uint32_t seq, uint32_t ms,
                               const uint8_t *data, uint8_t data_len) {
  memset(msg, 0, sizeof(LinkMsg));
  msg->type = LINK_MSG_DATA;
  msg->seq = seq;
  msg->millis_stamp = ms;
  if (data != nullptr && data_len > 0) {
    if (data_len > LINK_PAYLOAD_MAX) {
      data_len = LINK_PAYLOAD_MAX;
    }
    msg->len = data_len;
    memcpy(msg->payload, data, data_len);
  }
}

inline size_t link_msg_wire_size(const LinkMsg *msg) {
  size_t n = offsetof(LinkMsg, payload) + msg->len;
  if (n > sizeof(LinkMsg)) {
    n = sizeof(LinkMsg);
  }
  return n;
}
