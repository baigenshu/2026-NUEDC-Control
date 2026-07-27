#include <Arduino.h>
#include <string.h>

#include "config.h"
#include "espnow_radio.h"
#include "link_proto.h"

static void blink_once() {
  digitalWrite(LED_PIN, LOW);
  delay(40);
  digitalWrite(LED_PIN, HIGH);
}

static void on_recv(const uint8_t *mac, const uint8_t *data, uint8_t len) {
  (void)mac;

  if (len < offsetof(LinkMsg, payload)) {
    Serial.println(F("short frame"));
    return;
  }

  LinkMsg msg;
  memset(&msg, 0, sizeof(msg));
  const uint8_t copy_len = len > sizeof(LinkMsg) ? sizeof(LinkMsg) : len;
  memcpy(&msg, data, copy_len);

  Serial.print(F("rx type="));
  Serial.print(msg.type);
  Serial.print(F(" seq="));
  Serial.print(msg.seq);
  Serial.print(F(" ms="));
  Serial.print(msg.millis_stamp);
  Serial.print(F(" len="));
  Serial.println(msg.len);

  blink_once();
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  Serial.begin(SERIAL_BAUD);
  delay(200);
  Serial.println();
  Serial.println(F("ESP-NOW receiver"));

  if (!espnow_radio_begin(WIFI_CHANNEL)) {
    Serial.println(F("espnow init failed"));
    while (true) {
      delay(1000);
    }
  }

  espnow_radio_print_mac();
  espnow_radio_on_recv(on_recv);

  if (!espnow_radio_add_peer(PEER_MAC, WIFI_CHANNEL)) {
    Serial.println(F("add peer failed"));
  } else {
    Serial.println(F("peer ready (broadcast/demo)"));
  }
}

void loop() {
  delay(10);
}
