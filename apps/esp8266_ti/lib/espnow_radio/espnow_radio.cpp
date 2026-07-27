#include "espnow_radio.h"

#include <ESP8266WiFi.h>
#include <espnow.h>

static EspNowRecvCb s_recv_cb = nullptr;
static EspNowSentCb s_sent_cb = nullptr;

static void on_sent(uint8_t *mac, uint8_t status) {
  if (s_sent_cb) {
    s_sent_cb(mac, status);
  }
}

static void on_recv(uint8_t *mac, uint8_t *data, uint8_t len) {
  if (s_recv_cb) {
    s_recv_cb(mac, data, len);
  }
}

bool espnow_radio_begin(uint8_t channel) {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(50);

  if (esp_now_init() != 0) {
    return false;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_send_cb(on_sent);
  esp_now_register_recv_cb(on_recv);

  (void)channel;
  return true;
}

bool espnow_radio_add_peer(const uint8_t mac[6], uint8_t channel) {
  if (esp_now_is_peer_exist(const_cast<u8 *>(mac))) {
    esp_now_del_peer(const_cast<u8 *>(mac));
  }
  return esp_now_add_peer(const_cast<u8 *>(mac), ESP_NOW_ROLE_COMBO, channel,
                          nullptr, 0) == 0;
}

bool espnow_radio_send(const uint8_t mac[6], const uint8_t *data, uint8_t len) {
  return esp_now_send(const_cast<u8 *>(mac), const_cast<u8 *>(data), len) == 0;
}

void espnow_radio_on_recv(EspNowRecvCb cb) { s_recv_cb = cb; }

void espnow_radio_on_sent(EspNowSentCb cb) { s_sent_cb = cb; }

void espnow_radio_print_mac() {
  Serial.print(F("MAC: "));
  Serial.println(WiFi.macAddress());
}
