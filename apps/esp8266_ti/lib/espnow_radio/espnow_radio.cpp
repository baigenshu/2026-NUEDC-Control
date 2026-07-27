#include "espnow_radio.h"

#include <ESP8266WiFi.h>
#include <espnow.h>
#include <string.h>

static EspNowRecvCb s_recv_cb = nullptr;
static EspNowSentCb s_sent_cb = nullptr;
static volatile bool s_send_busy = false;
static uint8_t s_peer_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static bool s_peer_learned = false;
static uint8_t s_channel = 1;

static void on_sent(uint8_t *mac, uint8_t status) {
  s_send_busy = false;
  if (s_sent_cb) {
    s_sent_cb(mac, status);
  }
}

static void on_recv(uint8_t *mac, uint8_t *data, uint8_t len) {
  // Learn unicast peer only from frames with our magic header.
  if (mac != nullptr && !s_peer_learned && data != nullptr && len >= 2 &&
      data[0] == 0xA5 && data[1] == 0x5A) {
    bool is_bcast = true;
    for (int i = 0; i < 6; i++) {
      if (mac[i] != 0xFF) {
        is_bcast = false;
        break;
      }
    }
    if (!is_bcast) {
      espnow_radio_learn_peer(mac, s_channel);
    }
  }
  if (s_recv_cb) {
    s_recv_cb(mac, data, len);
  }
}

bool espnow_radio_begin(uint8_t channel) {
  s_channel = channel;
  s_send_busy = false;
  s_peer_learned = false;
  memset(s_peer_mac, 0xFF, 6);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(50);

  if (channel >= 1 && channel <= 13) {
    wifi_set_channel(channel);
  }

  if (esp_now_init() != 0) {
    return false;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_send_cb(on_sent);
  esp_now_register_recv_cb(on_recv);

  // Always keep broadcast peer for discovery / fallback.
  const uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  espnow_radio_add_peer(bcast, channel);

  return true;
}

bool espnow_radio_add_peer(const uint8_t mac[6], uint8_t channel) {
  if (esp_now_is_peer_exist(const_cast<u8 *>(mac))) {
    esp_now_del_peer(const_cast<u8 *>(mac));
  }
  return esp_now_add_peer(const_cast<u8 *>(mac), ESP_NOW_ROLE_COMBO, channel,
                          nullptr, 0) == 0;
}

void espnow_radio_learn_peer(const uint8_t mac[6], uint8_t channel) {
  if (mac == nullptr) {
    return;
  }
  memcpy(s_peer_mac, mac, 6);
  s_peer_learned = true;
  s_channel = channel;
  (void)espnow_radio_add_peer(s_peer_mac, channel);
}

const uint8_t *espnow_radio_peer_mac() { return s_peer_mac; }

bool espnow_radio_send_busy() { return s_send_busy; }

bool espnow_radio_try_send(const uint8_t mac[6], const uint8_t *data,
                           uint8_t len) {
  if (s_send_busy) {
    return false;
  }
  const uint8_t *dest = mac != nullptr ? mac : s_peer_mac;
  s_send_busy = true;
  if (esp_now_send(const_cast<u8 *>(dest), const_cast<u8 *>(data), len) != 0) {
    s_send_busy = false;
    return false;
  }
  return true;
}

bool espnow_radio_send(const uint8_t mac[6], const uint8_t *data, uint8_t len) {
  return espnow_radio_try_send(mac, data, len);
}

void espnow_radio_on_recv(EspNowRecvCb cb) { s_recv_cb = cb; }

void espnow_radio_on_sent(EspNowSentCb cb) { s_sent_cb = cb; }

void espnow_radio_print_mac() {
  Serial.print(F("MAC: "));
  Serial.println(WiFi.macAddress());
}
