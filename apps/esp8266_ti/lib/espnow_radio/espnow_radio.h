#pragma once

#include <stdint.h>

typedef void (*EspNowRecvCb)(const uint8_t *mac, const uint8_t *data, uint8_t len);
typedef void (*EspNowSentCb)(const uint8_t *mac, uint8_t status);

bool espnow_radio_begin(uint8_t channel);
bool espnow_radio_add_peer(const uint8_t mac[6], uint8_t channel);
bool espnow_radio_send(const uint8_t mac[6], const uint8_t *data, uint8_t len);
void espnow_radio_on_recv(EspNowRecvCb cb);
void espnow_radio_on_sent(EspNowSentCb cb);
void espnow_radio_print_mac();
