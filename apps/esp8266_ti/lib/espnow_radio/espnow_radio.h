#pragma once

#include <stdint.h>

typedef void (*EspNowRecvCb)(const uint8_t *mac, const uint8_t *data, uint8_t len);
typedef void (*EspNowSentCb)(const uint8_t *mac, uint8_t status);

bool espnow_radio_begin(uint8_t channel);
bool espnow_radio_add_peer(const uint8_t mac[6], uint8_t channel);
bool espnow_radio_send(const uint8_t mac[6], const uint8_t *data, uint8_t len);
// Non-blocking: false if previous send still in flight or queue reject.
bool espnow_radio_try_send(const uint8_t mac[6], const uint8_t *data, uint8_t len);
bool espnow_radio_send_busy();
void espnow_radio_on_recv(EspNowRecvCb cb);
void espnow_radio_on_sent(EspNowSentCb cb);
void espnow_radio_print_mac();
void espnow_radio_learn_peer(const uint8_t mac[6], uint8_t channel);
const uint8_t *espnow_radio_peer_mac();
