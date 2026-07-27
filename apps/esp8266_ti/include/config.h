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

// Flush serial chunk after this idle gap (ms).
#ifndef BRIDGE_IDLE_MS
#define BRIDGE_IDLE_MS 8
#endif

// Max bytes per ESP-NOW packet (must be <= LINK_PAYLOAD_MAX).
#ifndef BRIDGE_MAX_CHUNK
#define BRIDGE_MAX_CHUNK 200
#endif

// Broadcast peer for demo. Replace with peer STA MAC for unicast (more reliable).
// Read MAC from Serial at boot (printed by both firmwares).
static const uint8_t PEER_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
