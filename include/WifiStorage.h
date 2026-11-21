// Define el formato de las credenciales Wi-Fi y la interfaz para guardarlas en LittleFS.
#pragma once

#include <stddef.h>
#include <stdint.h>

enum class WifiMode : uint8_t { Station, AccessPoint };

struct WifiAddress
{
    uint32_t ip = 0;
    uint32_t mask = 0;
    uint32_t gateway = 0;

    bool manual() const { return ip != 0; }
    bool valid(WifiMode mode) const;
};

// El archivo almacena esta estructura con campos de longitud fija terminados en cero.
struct WifiCredentials
{
    static constexpr size_t maxPasswordLength = 64;
    char ssid[33];
    char password[maxPasswordLength + 1];
    WifiMode mode = WifiMode::Station;
    WifiAddress stationAddress{};
    WifiAddress accessPointAddress{};
};

namespace WifiStorage
{
// Monta el sistema de archivos.
bool begin();
// Lee y valida las credenciales guardadas.
bool load(WifiCredentials& credentials);
// Escribe las credenciales confirmadas para el siguiente arranque.
bool save(const WifiCredentials& credentials);
}
