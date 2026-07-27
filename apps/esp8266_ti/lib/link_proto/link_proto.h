#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum LinkMsgType : uint8_t {
  LINK_MSG_PING = 0,
  LINK_MSG_PONG = 1,
  LINK_MSG_DATA = 2,
};

static const uint8_t LINK_MAGIC0 = 0xA5;
static const uint8_t LINK_MAGIC1 = 0x5A;

// ESP-NOW max ~250 bytes; leave room for header.
static const uint8_t LINK_PAYLOAD_MAX = 200;

struct __attribute__((packed)) LinkMsg {
  uint8_t magic0;
  uint8_t magic1;
  uint8_t type;
  uint8_t len;
  uint32_t seq;
  uint32_t millis_stamp;
  uint8_t payload[LINK_PAYLOAD_MAX];
};

inline void link_msg_init_data(LinkMsg *msg, uint32_t seq, uint32_t ms,
                               const uint8_t *data, uint8_t data_len) {
  memset(msg, 0, sizeof(LinkMsg));
  msg->magic0 = LINK_MAGIC0;
  msg->magic1 = LINK_MAGIC1;
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

inline bool link_msg_header_ok(const uint8_t *data, uint8_t len) {
  if (data == nullptr || len < offsetof(LinkMsg, payload)) {
    return false;
  }
  return data[0] == LINK_MAGIC0 && data[1] == LINK_MAGIC1 &&
         data[2] == LINK_MSG_DATA;
}

inline uint8_t link_msg_payload_len(const uint8_t *data, uint8_t len) {
  if (!link_msg_header_ok(data, len)) {
    return 0;
  }
  const uint8_t hdr = static_cast<uint8_t>(offsetof(LinkMsg, payload));
  uint8_t n = data[3];
  const uint8_t avail = static_cast<uint8_t>(len - hdr);
  if (n > avail) {
    n = avail;
  }
  if (n > LINK_PAYLOAD_MAX) {
    n = LINK_PAYLOAD_MAX;
  }
  return n;
}

inline const uint8_t *link_msg_payload_ptr(const uint8_t *data) {
  return data + offsetof(LinkMsg, payload);
}

inline size_t link_msg_wire_size(const LinkMsg *msg) {
  size_t n = offsetof(LinkMsg, payload) + msg->len;
  if (n > sizeof(LinkMsg)) {
    n = sizeof(LinkMsg);
  }
  return n;
}
