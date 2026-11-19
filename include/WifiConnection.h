#pragma once

#include <stdint.h>

namespace WifiConnection
{
bool begin();
void disconnect();
void connect(const char* ssid, const char* password, int32_t channel, const uint8_t* bssid);
// Devuelve true si vence el plazo y hay que mostrar el selector.
bool update();
bool isOnline();
}
