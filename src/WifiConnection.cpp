#include "WifiConnection.h"

#include <ESP8266WiFi.h>

#include "WifiStorage.h"

namespace
{
enum class State { Idle, Connecting, Online };

constexpr unsigned long connectionTimeout = 25000;
State state = State::Idle;
unsigned long connectionStarted = 0;
bool saveAfterConnecting = false;
WifiCredentials credentials{};

void startConnection(int32_t channel = 0, const uint8_t* bssid = nullptr)
{
    Serial.print("Conectando a ");
    Serial.println(credentials.ssid);
    const char* password = credentials.password[0] == '\0' ? nullptr : credentials.password;
    WiFi.begin(credentials.ssid, password, channel, bssid);
    connectionStarted = millis();
    state = State::Connecting;
}
}

bool WifiConnection::begin()
{
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    if (!WifiStorage::begin())
    {
        Serial.println("No se pudo abrir la memoria para guardar la red.");
        return false;
    }
    if (!WifiStorage::load(credentials))
    {
        return false;
    }

    Serial.println("Usando la red guardada. Envia R para cambiarla.");
    startConnection();
    return true;
}

void WifiConnection::disconnect()
{
    WiFi.disconnect();
    saveAfterConnecting = false;
    state = State::Idle;
}

void WifiConnection::connect(const char* ssid, const char* password, int32_t channel,
                             const uint8_t* bssid)
{
    credentials = {};
    strlcpy(credentials.ssid, ssid, sizeof(credentials.ssid));
    strlcpy(credentials.password, password, sizeof(credentials.password));
    saveAfterConnecting = true;
    startConnection(channel, bssid);
}

bool WifiConnection::update()
{
    if (state == State::Online && WiFi.status() != WL_CONNECTED)
    {
        state = State::Connecting;
        connectionStarted = millis();
        Serial.println("Wi-Fi desconectada. Esperando la reconexion...");
    }
    if (state != State::Connecting)
    {
        return false;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        if (saveAfterConnecting)
        {
            Serial.println(WifiStorage::save(credentials)
                               ? "Red guardada para el siguiente arranque."
                               : "Conexion activa, pero no se pudo guardar la red.");
            saveAfterConnecting = false;
        }
        state = State::Online;
    }
    else if (millis() - connectionStarted >= connectionTimeout)
    {
        Serial.println("No se pudo conectar. Revisa la clave y la cobertura.");
        return true;
    }

    return false;
}

bool WifiConnection::isOnline()
{
    return state == State::Online;
}
