// Gestiona los intentos de conexión, la reconexión automática y el guardado de redes válidas.
#include "WifiConnection.h"

#include <ESP8266WiFi.h>
#include <user_interface.h>

#include "EventLog.h"
#include "WifiStorage.h"

namespace
{
// Distingue entre inactividad, conexión pendiente y conexión activa.
enum class State { Idle, Connecting, Online };

constexpr unsigned long connectionTimeout = 25000;
constexpr unsigned long webResponseDelayMs = 750;
State state = State::Idle;
unsigned long connectionStarted = 0;
bool saveAfterConnecting = false;
WifiCredentials credentials{};
WifiCredentials pendingCredentials{};
WifiCredentials previousCredentials{};
unsigned long changeRequestedAt = 0;
bool changeScheduled = false;
bool restoreOnFailure = false;

IPAddress ipv4(uint32_t value)
{
    return IPAddress(value >> 24, value >> 16, value >> 8, value);
}

WifiAddress& addressFor(WifiCredentials& config, WifiMode mode)
{
    return mode == WifiMode::AccessPoint ? config.accessPointAddress : config.stationAddress;
}

void schedule(const WifiCredentials& config)
{
    pendingCredentials = config;
    changeRequestedAt = millis();
    changeScheduled = true;
}

// Inicia el intento sin esperar en un bucle y guarda el instante de inicio para controlar el plazo.
bool startConnection(int32_t channel = 0, const uint8_t* bssid = nullptr)
{
    const auto& address = credentials.stationAddress;
    state = State::Idle;
    if (!WiFi.mode(WIFI_STA) ||
        !WiFi.config(ipv4(address.ip), ipv4(address.gateway), ipv4(address.mask)))
    {
        return false;
    }
    Serial.print("Conectando a ");
    Serial.println(credentials.ssid);
    const char* password = credentials.password[0] == '\0' ? nullptr : credentials.password;
    WiFi.begin(credentials.ssid, password, channel, bssid);
    connectionStarted = millis();
    state = State::Connecting;
    return true;
}

void connectionReady()
{
    if (saveAfterConnecting)
    {
        const bool saved = WifiStorage::save(credentials);
        Serial.println(saved ? "Configuracion Wi-Fi guardada."
                             : "Wi-Fi activa, pero no se pudo guardar la configuracion.");
        if (restoreOnFailure)
        {
            EventLog::add(saved ? "Nueva configuración Wi-Fi activa y guardada."
                                : "Nueva configuración Wi-Fi activa. Falló el guardado.");
        }
        saveAfterConnecting = false;
    }
    restoreOnFailure = false;
    previousCredentials = {};
    state = State::Online;
}

bool startConfiguredNetwork()
{
    if (credentials.mode == WifiMode::Station)
    {
        return startConnection();
    }

    const IPAddress address(192, 168, 4, 1);
    const auto& config = credentials.accessPointAddress;
    if (!WiFi.enableAP(false) || !WiFi.mode(WIFI_AP) ||
        !WiFi.softAP(credentials.ssid, credentials.password))
    {
        state = State::Idle;
        return false;
    }
    if (config.manual())
    {
        // En manual los clientes configuran su propia dirección, sin servidor DHCP.
        wifi_softap_dhcps_stop();
        ip_info info{};
        info.ip.addr = ipv4(config.ip).v4();
        info.netmask.addr = ipv4(config.mask).v4();
        info.gw.addr = ipv4(config.gateway).v4();
        if (wifi_softap_dhcps_status() != DHCP_STOPPED || !wifi_set_ip_info(SOFTAP_IF, &info))
        {
            state = State::Idle;
            return false;
        }
    }
    else if (!WiFi.softAPConfig(address, address, IPAddress(255, 255, 255, 0)))
    {
        state = State::Idle;
        return false;
    }
    connectionReady();
    return true;
}

bool recoverPreviousNetwork()
{
    EventLog::add("Falló la nueva configuración Wi-Fi. Intentando recuperar la anterior.");
    restoreOnFailure = false;
    saveAfterConnecting = false;
    credentials = previousCredentials;
    previousCredentials = {};
    WiFi.disconnect();
    return startConfiguredNetwork();
}
}

// Configura el modo estación y busca en LittleFS la red del arranque anterior.
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
    return startConfiguredNetwork();
}

// Cancela el intento y evita guardar credenciales de una conexión abandonada.
void WifiConnection::disconnect()
{
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    saveAfterConnecting = false;
    changeScheduled = false;
    restoreOnFailure = false;
    pendingCredentials = {};
    previousCredentials = {};
    state = State::Idle;
}

// Copia las credenciales al estado del módulo y aplaza su guardado hasta que la conexión funcione.
void WifiConnection::connect(const char* ssid, const char* password, int32_t channel,
                             const uint8_t* bssid)
{
    credentials.mode = WifiMode::Station;
    credentials.stationAddress = {};
    strlcpy(credentials.ssid, ssid, sizeof(credentials.ssid));
    strlcpy(credentials.password, password, sizeof(credentials.password));
    saveAfterConnecting = true;
    startConnection(channel, bssid);
}

bool WifiConnection::scheduleChange(WifiMode mode, const char* ssid, const char* password)
{
    if (!isOnline() || isChanging())
    {
        return false;
    }
    WifiCredentials next = credentials;
    strlcpy(next.ssid, ssid, sizeof(next.ssid));
    strlcpy(next.password, password, sizeof(next.password));
    next.mode = mode;
    schedule(next);
    return true;
}

WifiAddress WifiConnection::address(WifiMode mode)
{
    return addressFor(credentials, mode);
}

bool WifiConnection::setAddress(WifiMode mode, const WifiAddress& address)
{
    if (!isOnline() || isChanging() || !address.valid(mode)) return false;
    WifiCredentials next = credentials;
    addressFor(next, mode) = address;
    if (mode == credentials.mode)
    {
        schedule(next);
    }
    else
    {
        if (!WifiStorage::save(next)) return false;
        credentials = next;
    }
    return true;
}

bool WifiConnection::isChanging()
{
    return changeScheduled || restoreOnFailure;
}

bool WifiConnection::isAccessPoint()
{
    return state == State::Online && credentials.mode == WifiMode::AccessPoint;
}

// Supervisa la reconexión, confirma la red nueva y avisa al bucle principal si vence el plazo.
bool WifiConnection::update()
{
    if (changeScheduled && millis() - changeRequestedAt >= webResponseDelayMs)
    {
        changeScheduled = false;
        restoreOnFailure = true;
        previousCredentials = credentials;
        credentials = pendingCredentials;
        pendingCredentials = {};
        saveAfterConnecting = true;
        EventLog::add("Cambio Wi-Fi desde el panel: %s.",
                      credentials.mode == WifiMode::AccessPoint ? "punto de acceso" : "conexión a una red");
        WiFi.disconnect();
        return !startConfiguredNetwork() && !recoverPreviousNetwork();
    }
    if (state == State::Online && !isAccessPoint() && WiFi.status() != WL_CONNECTED)
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
        connectionReady();
    }
    else if (millis() - connectionStarted >= connectionTimeout)
    {
        Serial.println("No se pudo conectar. Revisa la clave y la cobertura.");
        if (restoreOnFailure)
        {
            return !recoverPreviousNetwork();
        }
        return true;
    }

    return false;
}

// Expone el estado que el servidor usa para arrancar o detenerse.
bool WifiConnection::isOnline()
{
    return state == State::Online;
}
