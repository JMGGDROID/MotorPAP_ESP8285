// Guarda y recupera la red Wi-Fi en el sistema de archivos LittleFS de la placa.
#include "WifiStorage.h"

#include <LittleFS.h>

namespace
{
// El nombre identifica la versión del formato almacenado.
constexpr char credentialsPath[] = "/wifi-v1";
constexpr char temporaryCredentialsPath[] = "/wifi-v1.tmp";
bool storageReady = false;
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
    return file && file.size() == sizeof(credentials) &&
           file.read(reinterpret_cast<uint8_t*>(&credentials), sizeof(credentials)) == sizeof(credentials) &&
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
