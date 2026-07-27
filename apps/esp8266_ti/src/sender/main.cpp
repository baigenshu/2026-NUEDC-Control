#include <Arduino.h>

#include "config.h"
#include "espnow_radio.h"
#include "link_proto.h"

static uint32_t s_seq = 0;
static uint32_t s_last_send_ms = 0;

static void on_sent(const uint8_t *mac, uint8_t status) {
  (void)mac;
  Serial.print(F("send status="));
  Serial.println(status == 0 ? F("ok") : F("fail"));
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  Serial.begin(SERIAL_BAUD);
  delay(200);
  Serial.println();
  Serial.println(F("ESP-NOW sender"));

  if (!espnow_radio_begin(WIFI_CHANNEL)) {
    Serial.println(F("espnow init failed"));
    while (true) {
      delay(1000);
    }
  }

  espnow_radio_print_mac();
  espnow_radio_on_sent(on_sent);

  if (!espnow_radio_add_peer(PEER_MAC, WIFI_CHANNEL)) {
    Serial.println(F("add peer failed"));
  } else {
    Serial.println(F("peer ready (broadcast/demo)"));
  }
}

void loop() {
  const uint32_t now = millis();
  if (now - s_last_send_ms < SEND_INTERVAL_MS) {
    return;
  }
  s_last_send_ms = now;

  LinkMsg msg;
  link_msg_init_ping(&msg, s_seq++, now);

  const bool ok =
      espnow_radio_send(PEER_MAC, reinterpret_cast<const uint8_t *>(&msg),
                        static_cast<uint8_t>(link_msg_wire_size(&msg)));

  Serial.print(F("ping seq="));
  Serial.print(msg.seq);
  Serial.println(ok ? F(" queued") : F(" send err"));

  digitalWrite(LED_PIN, LOW);
  delay(30);
  digitalWrite(LED_PIN, HIGH);
}
