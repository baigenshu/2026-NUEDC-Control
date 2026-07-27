#include <Arduino.h>
#include <string.h>

#include "bridge_ring.h"
#include "config.h"
#include "espnow_radio.h"
#include "link_proto.h"

// Car-side: MCU UART <-> ESP-NOW

static uint32_t s_seq = 0;
static uint8_t s_tx_buf[BRIDGE_MAX_CHUNK];
static uint8_t s_tx_len = 0;
static uint32_t s_last_uart_ms = 0;
static bool s_bridge_ready = false;
static volatile bool s_led_toggle = false;
static LinkMsg s_tx_msg; // static: avoid large stack in loop

static BridgeRing<2048> s_rx_ring;

static void flush_uart_to_radio() {
  if (s_tx_len == 0 || !s_bridge_ready || espnow_radio_send_busy()) {
    return;
  }

  link_msg_init_data(&s_tx_msg, s_seq++, millis(), s_tx_buf, s_tx_len);

  if (espnow_radio_try_send(espnow_radio_peer_mac(),
                            reinterpret_cast<const uint8_t *>(&s_tx_msg),
                            static_cast<uint8_t>(link_msg_wire_size(&s_tx_msg)))) {
    s_tx_len = 0;
    s_led_toggle = true;
  }
  // if busy/fail, keep s_tx_len and retry next loop
}

static void on_recv(const uint8_t *mac, const uint8_t *data, uint8_t len) {
  (void)mac;

  if (!s_bridge_ready) {
    return;
  }

  const uint8_t n = link_msg_payload_len(data, len);
  if (n == 0 || s_rx_ring.free() < n) {
    return;
  }

  s_rx_ring.push(link_msg_payload_ptr(data), n);
  s_led_toggle = true;
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  // Quiet UART: MCU shares this line; no banners after boot ROM.
  Serial.begin(SERIAL_BAUD);
  delay(100);

  if (!espnow_radio_begin(WIFI_CHANNEL)) {
    while (true) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      delay(200);
    }
  }

  espnow_radio_on_recv(on_recv);

  while (Serial.available()) {
    (void)Serial.read();
  }

  s_bridge_ready = true;
  s_last_uart_ms = millis();
}

void loop() {
  // ESP-NOW -> MCU UART
  uint8_t chunk[64];
  for (;;) {
    const size_t n = s_rx_ring.pop(chunk, sizeof(chunk));
    if (n == 0) {
      break;
    }
    Serial.write(chunk, n);
  }

  // MCU UART -> ESP-NOW
  bool got = false;
  while (Serial.available() && s_tx_len < BRIDGE_MAX_CHUNK) {
    s_tx_buf[s_tx_len++] = static_cast<uint8_t>(Serial.read());
    got = true;
  }
  if (got) {
    s_last_uart_ms = millis();
  }
  if (s_tx_len > 0 && (s_tx_len >= BRIDGE_MAX_CHUNK ||
                        (millis() - s_last_uart_ms) >= BRIDGE_IDLE_MS)) {
    flush_uart_to_radio();
  }

  if (s_led_toggle) {
    s_led_toggle = false;
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }

  yield();
}
