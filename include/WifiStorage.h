// Define el formato de las credenciales Wi-Fi y la interfaz para guardarlas en LittleFS.
#pragma once

#include <stddef.h>

// El archivo almacena esta estructura con campos de longitud fija terminados en cero.
struct WifiCredentials
{
    static constexpr size_t maxPasswordLength = 64;
    char ssid[33];
    char password[maxPasswordLength + 1];
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
