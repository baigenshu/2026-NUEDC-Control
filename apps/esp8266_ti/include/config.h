#pragma once

#ifndef LED_PIN
#define LED_PIN 2
#endif

#ifndef WIFI_CHANNEL
#define WIFI_CHANNEL 1
#endif

#ifndef SERIAL_BAUD
#define SERIAL_BAUD 115200
#endif

#ifndef SEND_INTERVAL_MS
#define SEND_INTERVAL_MS 500
#endif

// Demo: broadcast peer. Replace with peer STA MAC for unicast.
// Read MAC from Serial at boot (printed by both firmwares).
static const uint8_t PEER_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
