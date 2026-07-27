#pragma once

#include <stddef.h>
#include <stdint.h>

// Single-producer / single-consumer byte ring (ESP-NOW cb -> loop).
template <size_t N> class BridgeRing {
public:
  size_t free() const {
    const size_t h = head_;
    const size_t t = tail_;
    if (h >= t) {
      return N - 1 - (h - t);
    }
    return t - h - 1;
  }

  size_t used() const {
    const size_t h = head_;
    const size_t t = tail_;
    if (h >= t) {
      return h - t;
    }
    return N - (t - h);
  }

  // Producer (e.g. recv callback). Drops on overflow.
  void push(const uint8_t *data, uint8_t len) {
    size_t h = head_;
    for (uint8_t i = 0; i < len; i++) {
      const size_t next = (h + 1) % N;
      if (next == tail_) {
        break;
      }
      buf_[h] = data[i];
      h = next;
    }
    head_ = h;
  }

  // Consumer (loop). Returns bytes written to out (up to max_len).
  size_t pop(uint8_t *out, size_t max_len) {
    size_t t = tail_;
    const size_t h = head_;
    size_t n = 0;
    while (t != h && n < max_len) {
      out[n++] = buf_[t];
      t = (t + 1) % N;
    }
    tail_ = t;
    return n;
  }

private:
  uint8_t buf_[N]{};
  volatile size_t head_ = 0;
  volatile size_t tail_ = 0;
};
