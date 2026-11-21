// Guarda y recupera la red Wi-Fi en el sistema de archivos LittleFS de la placa.
#include "WifiStorage.h"

#include <LittleFS.h>

namespace
{
// El nombre identifica la versión del formato almacenado.
constexpr char credentialsPath[] = "/wifi-v3";
constexpr char temporaryCredentialsPath[] = "/wifi-v3.tmp";
constexpr char previousCredentialsPath[] = "/wifi-v2";
constexpr char legacyCredentialsPath[] = "/wifi-v1";
bool storageReady = false;
}

bool WifiAddress::valid(WifiMode mode) const
{
    if (!manual()) return mask == 0 && gateway == 0;
    const uint32_t hostMask = ~mask;
    const auto usable = [hostMask](uint32_t value) {
        const uint8_t first = value >> 24;
        const uint32_t host = value & hostMask;
        return first && first != 127 && first < 224 && host && host != hostMask;
    };
    return mask && hostMask >= 3 && (hostMask & (hostMask + 1)) == 0 &&
           usable(ip) && usable(gateway) && (ip & mask) == (gateway & mask) &&
           (mode == WifiMode::AccessPoint || ip != gateway);
}

// Monta LittleFS y registra si las escrituras están disponibles.
bool WifiStorage::begin()
{
    storageReady = LittleFS.begin();
    return storageReady;
}

// Comprueba el tamaño de la estructura, la lectura y la terminación de las cadenas.
bool WifiStorage::load(WifiCredentials& credentials)
{
    File file = LittleFS.open(credentialsPath, "r");
    size_t size = sizeof(credentials);
    if (!file)
    {
        file = LittleFS.open(previousCredentialsPath, "r");
        size = offsetof(WifiCredentials, mode) + sizeof(WifiMode);
        if (!file)
        {
            file = LittleFS.open(legacyCredentialsPath, "r");
            size = offsetof(WifiCredentials, mode);
        }
    }
    credentials = {};
    return file && file.size() == size &&
           file.read(reinterpret_cast<uint8_t*>(&credentials), size) == static_cast<int>(size) &&
           (credentials.mode == WifiMode::Station || credentials.mode == WifiMode::AccessPoint) &&
           credentials.stationAddress.valid(WifiMode::Station) &&
           credentials.accessPointAddress.valid(WifiMode::AccessPoint) &&
           credentials.ssid[0] != '\0' && credentials.ssid[sizeof(credentials.ssid) - 1] == '\0' &&
           credentials.password[sizeof(credentials.password) - 1] == '\0';
}

// Escribe primero un archivo temporal y lo renombra solo después de completar la escritura.
bool WifiStorage::save(const WifiCredentials& credentials)
{
    if (!storageReady)
    {
        return false;
    }

    File file = LittleFS.open(temporaryCredentialsPath, "w");
    const auto* data = reinterpret_cast<const uint8_t*>(&credentials);
    if (!file || file.write(data, sizeof(credentials)) != sizeof(credentials))
    {
        return false;
    }

    file.close();
    return LittleFS.rename(temporaryCredentialsPath, credentialsPath);
}
