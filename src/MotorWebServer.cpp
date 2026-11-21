// Implementa el panel HTTP y traduce sus peticiones en operaciones sobre el TMC5160.
#include "MotorWebServer.h"

#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ctype.h>
#include <errno.h>

#include "MotorPanel.h"
#include "EventLog.h"
#include "Tmc5160.h"
#include "GcodePlanner.h"
#include "WifiConnection.h"
#include "WifiStorage.h"

namespace
{
// El servidor escucha en el puerto 80. Cada sesión Jog usa un identificador para asociar sus órdenes.
ESP8266WebServer server(80);
bool running = false;
unsigned long jogId = 0;
unsigned long motionRevision = 0;
unsigned long programUploadId = 0;
constexpr char commandHeader[] = "X-MotorPAP-Command";

bool programArgument(const char* name, unsigned long& value)
{
    const String text = server.arg(name);
    if (text.isEmpty() || text[0] < '0' || text[0] > '9') return false;
    char* end;
    errno = 0;
    value = strtoul(text.c_str(), &end, 10);
    return *end == '\0' && errno != ERANGE;
}

bool readProgramValue(const char*& cursor, long& value, char separator)
{
    if ((*cursor < '0' || *cursor > '9') && *cursor != '-') return false;
    char* end;
    errno = 0;
    value = strtol(cursor, &end, 10);
    if (end == cursor || *end != separator || errno == ERANGE) return false;
    cursor = end + 1;
    return true;
}

void receiveProgram()
{
    if (server.header(commandHeader) != "1")
    {
        server.send(403, "text/plain", "Comando rechazado.");
        return;
    }
    if (server.arg("after") != String(motionRevision) || Tmc5160::isMoving() ||
        GcodePlanner::state() == GcodePlanner::State::running)
    {
        server.send(409, "text/plain; charset=utf-8", "La sesión de movimiento ha cambiado.");
        return;
    }

    unsigned long offset;
    if (!programArgument("offset", offset))
    {
        server.send(400, "text/plain; charset=utf-8", "Desplazamiento de programa no válido.");
        return;
    }
    if (offset == 0)
    {
        unsigned long total;
        if (!programArgument("count", total) || !GcodePlanner::prepare(total, motionRevision))
        {
            server.send(400, "text/plain; charset=utf-8", "Tamaño de programa no válido o memoria insuficiente.");
            return;
        }
        ++programUploadId;
    }
    else if (server.arg("upload") != String(programUploadId) || offset != GcodePlanner::loaded())
    {
        server.send(409, "text/plain; charset=utf-8", "La carga del programa ha caducado o ha cambiado.");
        return;
    }

    const String body = server.arg("plain");
    const char* cursor = body.c_str();
    bool valid = !body.isEmpty() && body.length() <= 1024 && strlen(cursor) == body.length();
    while (valid && *cursor)
    {
        long steps, speed, line;
        valid = readProgramValue(cursor, steps, ',') && readProgramValue(cursor, speed, ',') &&
                readProgramValue(cursor, line, '\n') && speed > 0 && line > 0 && line <= UINT16_MAX &&
                GcodePlanner::append(steps, speed, line);
    }
    if (!valid)
    {
        GcodePlanner::discard();
        server.send(400, "text/plain; charset=utf-8", "Bloque de movimientos no válido.");
        return;
    }

    if (server.arg("start") != "1")
    {
        server.send(200, "text/plain", String(programUploadId));
        return;
    }

    unsigned long rampMs, decelerationMs;
    if (!programArgument("rampMs", rampMs) || !programArgument("decelerationMs", decelerationMs) ||
        server.arg("position") != String(static_cast<long>(Tmc5160::position().units)) ||
        !GcodePlanner::start(motionRevision, rampMs, decelerationMs))
    {
        GcodePlanner::discard();
        server.send(409, "text/plain; charset=utf-8", "No se pudo iniciar el programa. Comprueba la posición y el estado del driver.");
        return;
    }
    ++jogId;
    ++motionRevision;
    EventLog::add("Programa G iniciado.");
    server.send(200, "text/plain", String(jogId));
}
constexpr unsigned long wifiSignalIntervalMs = 10000;
constexpr unsigned long wifiScanRetentionMs = 20000;
bool webScanRequested = false;
unsigned long webScanStarted = 0;

IPAddress wifiAddress()
{
    return WifiConnection::isAccessPoint() ? WiFi.softAPIP() : WiFi.localIP();
}

bool parseIpv4(const String& input, uint32_t& address)
{
    const char* cursor = input.c_str();
    if (input.length() != strlen(cursor)) return false;
    address = 0;
    for (unsigned i = 0; i < 4; ++i)
    {
        unsigned octet = 0;
        unsigned digits = 0;
        while (*cursor >= '0' && *cursor <= '9')
        {
            if (++digits > 3) return false;
            octet = octet * 10 + (*cursor++ - '0');
        }
        if (!digits || octet > 255 || *cursor != (i == 3 ? '\0' : '.')) return false;
        address = (address << 8) | octet;
        ++cursor;
    }
    return true;
}

IPAddress asIp(uint32_t value)
{
    return IPAddress(value >> 24, value >> 16, value >> 8, value);
}

void wifiAddressSettings(bool save)
{
    if (save && server.header(commandHeader) != "1")
    {
        server.send(403, "text/plain", "Comando rechazado.");
        return;
    }
    const String modeArg = server.arg("mode");
    if (modeArg != "station" && modeArg != "ap")
    {
        server.send(400, "text/plain; charset=utf-8", "Modo Wi-Fi no válido.");
        return;
    }
    const bool ap = modeArg == "ap";
    const WifiMode mode = ap ? WifiMode::AccessPoint : WifiMode::Station;
    const bool active = WifiConnection::isOnline() && ap == WifiConnection::isAccessPoint();
    if (!save)
    {
        const auto config = WifiConnection::address(mode);
        const IPAddress apDefault(192, 168, 4, 1);
        const IPAddress ip = config.manual() ? asIp(config.ip) : ap ? apDefault : active ? WiFi.localIP() : IPAddress();
        const IPAddress mask = config.manual() ? asIp(config.mask) :
            ap ? IPAddress(255, 255, 255, 0) : active ? WiFi.subnetMask() : IPAddress();
        const IPAddress gateway = config.manual() ? asIp(config.gateway) :
            ap ? apDefault : active ? WiFi.gatewayIP() : IPAddress();
        char response[128];
        snprintf(response, sizeof(response), "{\"manual\":%s,\"ip\":\"%s\",\"mask\":\"%s\",\"gateway\":\"%s\"}",
                 config.manual() ? "true" : "false", ip.toString().c_str(),
                 mask.toString().c_str(), gateway.toString().c_str());
        server.sendHeader("Cache-Control", "no-store");
        server.send(200, "application/json", response);
        return;
    }

    if (Tmc5160::isEnabled() || Tmc5160::isMoving() || WifiConnection::isChanging() ||
        WiFi.scanComplete() == WIFI_SCAN_RUNNING)
    {
        server.send(409, "text/plain; charset=utf-8",
                    "Pon el driver en OFF y espera a que termine cualquier búsqueda o cambio de red.");
        return;
    }
    WifiAddress config{};
    const String method = server.arg("method");
    if (method != "dhcp" &&
        (method != "manual" || !parseIpv4(server.arg("ip"), config.ip) || !config.manual() ||
         !parseIpv4(server.arg("mask"), config.mask) || !parseIpv4(server.arg("gateway"), config.gateway) ||
         !config.valid(mode)))
    {
        server.send(400, "text/plain; charset=utf-8",
                    "Introduce una IP y puerta de enlace válidas de la misma subred, con una máscara válida.");
        return;
    }
    if (!WifiConnection::setAddress(mode, config))
    {
        server.send(409, "text/plain; charset=utf-8", "No se pudo guardar la configuración IP.");
        return;
    }
    server.send(active ? 202 : 204, "text/plain");
}

void sendWifiScanResults()
{
    server.sendHeader("Cache-Control", "no-store");
    const int count = webScanRequested ? WiFi.scanComplete() : WIFI_SCAN_FAILED;
    if (count == WIFI_SCAN_RUNNING)
    {
        server.send(202, "text/plain");
        return;
    }
    if (count < 0)
    {
        server.send(503, "text/plain", "No se pudo completar la búsqueda. Vuelve a intentarlo.");
        return;
    }

    String results;
    // El SSID se codifica en hexadecimal para conservar sus bytes y delimitar cada campo.
    if (!results.reserve(80 * count + 1))
    {
        server.send(503, "text/plain", "No hay memoria suficiente para mostrar las redes.");
        return;
    }
    constexpr char hex[] = "0123456789abcdef";
    for (int index = 0; index < count; ++index)
    {
        const String ssid = WiFi.SSID(index);
        if (ssid.isEmpty() || ssid.length() >= sizeof(WifiCredentials::ssid))
        {
            continue;
        }
        for (size_t byte = 0; byte < ssid.length(); ++byte)
        {
            const auto value = static_cast<uint8_t>(ssid[byte]);
            results += hex[value >> 4];
            results += hex[value & 0x0f];
        }
        results += '\t';
        results += WiFi.RSSI(index);
        results += WiFi.encryptionType(index) == ENC_TYPE_NONE ? "\t0\n" : "\t1\n";
    }
    server.send(200, "text/plain; charset=utf-8", results);
}

bool validWifiPassword(const String& password)
{
    const size_t length = password.length();
    if (length != strlen(password.c_str()))
    {
        return false;
    }
    if (length < WifiCredentials::maxPasswordLength)
    {
        return length == 0 || length >= 8;
    }
    if (length != WifiCredentials::maxPasswordLength)
    {
        return false;
    }
    for (size_t index = 0; index < length; ++index)
    {
        if (!isxdigit(static_cast<unsigned char>(password[index])))
        {
            return false;
        }
    }
    return true;
}

bool decodeScanSsid(const String& encoded, String& ssid)
{
    if (encoded.length() % 2 != 0 || encoded.length() >= 2 * sizeof(WifiCredentials::ssid))
    {
        return false;
    }
    ssid = "";
    if (!ssid.reserve(encoded.length() / 2))
    {
        return false;
    }
    for (size_t index = 0; index < encoded.length(); index += 2)
    {
        char pair[]{encoded[index], encoded[index + 1], '\0'};
        if (!isxdigit(static_cast<unsigned char>(pair[0])) || !isxdigit(static_cast<unsigned char>(pair[1])))
        {
            return false;
        }
        const char byte = static_cast<char>(strtoul(pair, nullptr, 16));
        if (byte == '\0')
        {
            return false;
        }
        ssid += byte;
    }
    return true;
}

// Toma una muestra en la primera consulta y reutiliza el valor durante diez segundos.
int32_t wifiSignal()
{
    static bool hasSample = false;
    static unsigned long sampledAt = 0;
    static int32_t signal = 0;
    const unsigned long now = millis();
    if (!hasSample || now - sampledAt >= wifiSignalIntervalMs)
    {
        signal = WiFi.RSSI();
        sampledAt = now;
        hasSample = true;
    }
    return signal;
}

// Devuelve al navegador los límites que corresponden a la resolución configurada.
void sendMotionLimits()
{
    char response[160];
    snprintf(response, sizeof(response),
             "{\"microsteps\":%u,\"maxRpm\":%u,\"maxPulseSpeed\":%u,"
             "\"minAcceleration\":%u,\"maxAcceleration\":%u}",
             unsigned(Tmc5160::microsteps()), unsigned(Tmc5160::maxJogRpm()),
             unsigned(Tmc5160::maxJogSpeed()), unsigned(Tmc5160::minJogAcceleration()),
             unsigned(Tmc5160::maxJogAcceleration()));
    server.send(200, "application/json", response);
}

// Valida la velocidad en micropasos por segundo y responde con HTTP 400 si queda fuera del rango.
uint32_t requestSpeed()
{
    const String speedArgument = server.arg("speed");
    char* end = nullptr;
    const unsigned long speed = strtoul(speedArgument.c_str(), &end, 10);
    if (*end != '\0' || speed < Tmc5160::minJogSpeed || speed > Tmc5160::maxJogSpeed())
    {
        char message[112];
        snprintf(message, sizeof(message), "Velocidad valida: %u a %u micropasos/s (tope: %u RPM).",
                 unsigned(Tmc5160::minJogSpeed), unsigned(Tmc5160::maxJogSpeed()),
                 unsigned(Tmc5160::maxJogRpm()));
        server.send(400, "text/plain", message);
        return 0;
    }
    return speed;
}

// Valida las rampas comunes a Jog y posicionamiento.
bool requestRamps(uint32_t speed, uint32_t& rampMs, uint32_t& decelerationMs)
{
    char* end = nullptr;
    rampMs = Tmc5160::defaultRampMs;
    if (server.hasArg("rampMs"))
    {
        const String argument = server.arg("rampMs");
        rampMs = strtoul(argument.c_str(), &end, 10);
        if (*end != '\0' || rampMs < Tmc5160::minRampMs || rampMs > Tmc5160::maxRampMs)
        {
            server.send(400, "text/plain", "Configura un tiempo de aceleracion de 0,10 a 600 segundos.");
            return false;
        }
    }
    else if (server.hasArg("accelerationRpm"))
    {
        const String argument = server.arg("accelerationRpm");
        const unsigned long rpm = strtoul(argument.c_str(), &end, 10);
        if (*end != '\0' || rpm < 1 || rpm > Tmc5160::maxJogRpm())
        {
            server.send(400, "text/plain", "Aceleracion fuera del rango de RPM/s de esta resolucion.");
            return false;
        }
        const uint32_t acceleration = Tmc5160::accelerationMilliPulses(static_cast<uint16_t>(rpm));
        rampMs = uint64_t(speed) * 1000000 / acceleration;
    }
    else if (server.hasArg("acceleration"))
    {
        const String argument = server.arg("acceleration");
        const unsigned long pulses = strtoul(argument.c_str(), &end, 10);
        if (*end != '\0' || pulses < Tmc5160::minJogAcceleration() ||
            pulses > Tmc5160::maxJogAcceleration())
        {
            server.send(400, "text/plain", "Aceleracion fuera del rango de micropasos/s2 de esta resolucion.");
            return false;
        }
        rampMs = uint64_t(speed) * 1000 / pulses;
    }

    decelerationMs = Tmc5160::defaultRampMs;
    if (server.hasArg("decelerationMs"))
    {
        const String argument = server.arg("decelerationMs");
        decelerationMs = strtoul(argument.c_str(), &end, 10);
        if (*end != '\0' || decelerationMs < Tmc5160::minRampMs ||
            decelerationMs > Tmc5160::maxRampMs)
        {
            server.send(400, "text/plain", "Configura un tiempo de deceleracion de 0,10 a 600 segundos.");
            return false;
        }
    }

    rampMs = max(rampMs, Tmc5160::minRampMs);
    return true;
}

// Inicia un Jog o cambia el sentido del existente cuando la petición incluye su identificador.
void jog(bool positive)
{
    if (server.header(commandHeader) != "1")
    {
        server.send(403, "text/plain", "Comando rechazado.");
        return;
    }
    if (server.hasArg("id"))
    {
        if (server.arg("id") != String(jogId) ||
            !Tmc5160::changeJogDirection(positive))
        {
            server.send(409, "text/plain", "El movimiento no admite un cambio de sentido.");
            return;
        }
        EventLog::add("Jog #%lu: sentido solicitado %s.", jogId, positive ? "CW" : "CCW");
        server.send(204);
        return;
    }
    const uint32_t speed = requestSpeed();
    uint32_t rampMs;
    uint32_t decelerationMs;
    if (speed == 0 || !requestRamps(speed, rampMs, decelerationMs))
    {
        return;
    }
    if (Tmc5160::silentInitializing())
    {
        server.send(409, "text/plain", "StealthChop se esta ajustando. Espera a que termine.");
        return;
    }
    if (!Tmc5160::startJog(positive, speed, rampMs, decelerationMs))
    {
        server.send(409, "text/plain", "Habilita el driver y comprueba que este detenido y conectado.");
        return;
    }

    ++jogId;
    ++motionRevision;
    EventLog::add("Jog #%lu: %s a %lu micropasos/s, acel. %lu ms, decel. %lu ms.",
                  jogId, positive ? "CW" : "CCW",
                  static_cast<unsigned long>(speed), static_cast<unsigned long>(rampMs),
                  static_cast<unsigned long>(decelerationMs));
    server.send(200, "text/plain", String(jogId));
}
}

// Registra la página y los endpoints de lectura y control.
void MotorWebServer::begin()
{
    server.collectHeaders(commandHeader);
    server.on("/", HTTP_GET, []() {
        server.send_P(200, PSTR("text/html; charset=utf-8"), motorPanelHtml);
    });
    server.on("/api/wifi", HTTP_GET, []() {
        server.sendHeader("Cache-Control", "no-store");
        if (WifiConnection::isAccessPoint())
        {
            server.sendHeader("X-Wifi-Mode", "ap");
            server.send(200, "text/plain; charset=utf-8", WiFi.softAPSSID());
        }
        else if (WiFi.status() == WL_CONNECTED)
        {
            server.sendHeader("X-Wifi-Mode", "station");
            server.send(200, "text/plain; charset=utf-8", WiFi.SSID());
        }
        else
        {
            server.send(204, "text/plain");
        }
    });
    server.on("/api/wifi/address", HTTP_GET, []() { wifiAddressSettings(false); });
    server.on("/api/wifi/address", HTTP_POST, []() { wifiAddressSettings(true); });
    server.on("/api/wifi/scan", HTTP_GET, sendWifiScanResults);
    server.on("/api/wifi/scan", HTTP_POST, []() {
        if (server.header(commandHeader) != "1")
        {
            server.send(403, "text/plain", "Comando rechazado.");
            return;
        }
        if (Tmc5160::isEnabled() || Tmc5160::isMoving() || WifiConnection::isChanging())
        {
            server.send(409, "text/plain; charset=utf-8",
                        "Pon el driver en OFF y espera a que termine cualquier cambio de red.");
            return;
        }
        if (WiFi.scanComplete() != WIFI_SCAN_RUNNING && WiFi.scanNetworks(true) == WIFI_SCAN_FAILED)
        {
            server.send(503, "text/plain", "No se pudo iniciar la búsqueda.");
            return;
        }
        webScanRequested = true;
        webScanStarted = millis();
        server.send(202, "text/plain");
    });
    server.on("/api/wifi", HTTP_POST, []() {
        if (server.header(commandHeader) != "1")
        {
            server.send(403, "text/plain", "Comando rechazado.");
            return;
        }
        if (Tmc5160::isEnabled() || Tmc5160::isMoving())
        {
            server.send(409, "text/plain", "Detén el motor y pon el driver en OFF antes de cambiar de red.");
            return;
        }
        if (WiFi.scanComplete() == WIFI_SCAN_RUNNING)
        {
            server.send(409, "text/plain; charset=utf-8", "Espera a que termine la búsqueda de redes.");
            return;
        }
        String ssid = server.arg("ssid");
        const String mode = server.hasArg("mode") ? server.arg("mode") : "station";
        if (mode != "station" && mode != "ap")
        {
            server.send(400, "text/plain; charset=utf-8", "Modo Wi-Fi no válido.");
            return;
        }
        const bool accessPoint = mode == "ap";
        const String encodedSsid = server.arg("ssidHex");
        const String password = server.arg("password");
        if ((!encodedSsid.isEmpty() && !decodeScanSsid(encodedSsid, ssid)) ||
            !server.hasArg("password") || ssid.isEmpty() ||
            ssid.length() >= sizeof(WifiCredentials::ssid) || ssid.length() != strlen(ssid.c_str()) ||
            !validWifiPassword(password) || (accessPoint && (password.length() < 8 || password.length() > 63)))
        {
            server.send(400, "text/plain; charset=utf-8", accessPoint
                        ? "SSID de 1 a 32 bytes y clave de 8 a 63 bytes para el punto de acceso."
                        : "SSID de 1 a 32 bytes. Clave de 8 a 63 bytes o 64 dígitos hexadecimales. Déjala vacía para una red abierta.");
            return;
        }
        if (!WifiConnection::scheduleChange(accessPoint ? WifiMode::AccessPoint : WifiMode::Station,
                                            ssid.c_str(), password.c_str()))
        {
            server.send(409, "text/plain; charset=utf-8", "Ya hay un cambio de red en curso.");
            return;
        }
        server.send(202, "text/plain");
    });
    server.on("/api/log", HTTP_GET, []() {
        String output;
        server.sendHeader("Cache-Control", "no-store");
        if (!EventLog::snapshot(output))
        {
            server.send(503, "text/plain; charset=utf-8", "Registro temporalmente no disponible.");
            return;
        }
        server.send(200, "text/plain; charset=utf-8", output);
    });
    // Entrega señal, tiempo activo, límites, estado del motor y diagnóstico en un JSON.
    server.on("/api/status", HTTP_GET, []() {
        const auto driver = Tmc5160::status();
        const auto position = Tmc5160::position();
        char response[1040];
        const bool accessPoint = WifiConnection::isAccessPoint();
        const unsigned long now = millis();
        snprintf(response, sizeof(response),
                 "{\"ip\":\"%s\",\"rssi\":%d,\"uptime\":%lu,\"accessPoint\":%s,"
                 "\"positionUnits\":%lld,\"positionUnitsPerStep\":%ld,\"positionTarget\":%.8f,\"homeUnits\":%lld,"
                 "\"hasPositionTarget\":%s,\"positioning\":%s,"
                 "\"sampleMs\":%lu,\"signedPulseSpeed\":%ld,\"motionId\":%lu,\"motionRevision\":%lu,"
                 "\"programState\":%u,\"programLine\":%u,"
                 "\"driverResponding\":%s,\"driverDisabled\":%s,\"driverEnabled\":%s,"
                 "\"stepDirMode\":%s,\"moving\":%s,\"decelerating\":%s,\"stopping\":%s,"
                 "\"reversing\":%s,"
                 "\"pinInversions\":%u,\"microsteps\":%u,\"maxRpm\":%u,\"maxPulseSpeed\":%u,"
                 "\"minAcceleration\":%u,\"maxAcceleration\":%u,"
                 "\"runCurrentMa\":%u,\"holdCurrentMa\":%u,\"silentMode\":%s,"
                 "\"silentInitializing\":%s,\"silentActive\":%s,"
                 "\"diagnosticsAvailable\":%s,\"overtemperature\":%s,\"temperatureWarning\":%s,"
                 "\"undervoltage\":%s,\"driverError\":%s,\"shortCircuit\":%s,\"stallGuardResult\":%d}",
                 wifiAddress().toString().c_str(), accessPoint ? 0 : wifiSignal(), now / 1000,
                 accessPoint ? "true" : "false",
                 static_cast<long long>(position.units), static_cast<long>(Tmc5160::positionUnitsPerStep),
                 double(position.targetUnits) / Tmc5160::positionUnitsPerStep, static_cast<long long>(position.homeUnits),
                 position.hasTarget ? "true" : "false", position.positioning ? "true" : "false",
                 now, static_cast<long>(Tmc5160::signedPulseSpeed()), jogId, motionRevision,
                 unsigned(GcodePlanner::state()), GcodePlanner::line(),
                 driver.responding ? "true" : "false", driver.outputDisabled ? "true" : "false",
                 Tmc5160::isEnabled() ? "true" : "false",
                 driver.stepDirMode ? "true" : "false", Tmc5160::isMoving() ? "true" : "false",
                 Tmc5160::isDecelerating() ? "true" : "false",
                 Tmc5160::isStopping() ? "true" : "false",
                 Tmc5160::isReversing() ? "true" : "false",
                 unsigned(Tmc5160::pinInversions()),
                 unsigned(Tmc5160::microsteps()), unsigned(Tmc5160::maxJogRpm()),
                 unsigned(Tmc5160::maxJogSpeed()), unsigned(Tmc5160::minJogAcceleration()),
                 unsigned(Tmc5160::maxJogAcceleration()), unsigned(Tmc5160::runCurrentMa()),
                 unsigned(Tmc5160::holdCurrentMa()), Tmc5160::silentMode() ? "true" : "false",
                 Tmc5160::silentInitializing() ? "true" : "false",
                 Tmc5160::isEnabled() && driver.silentActive ? "true" : "false",
                 driver.diagnosticsAvailable ? "true" : "false",
                 driver.overtemperature ? "true" : "false", driver.temperatureWarning ? "true" : "false",
                 driver.undervoltage ? "true" : "false", driver.driverError ? "true" : "false",
                 driver.shortCircuit ? "true" : "false", int(driver.stallGuardResult));
        server.sendHeader("Cache-Control", "no-store");
        server.send(200, "application/json", response);
    });
    server.on("/api/program", HTTP_POST, receiveProgram);
    // Los dos botones Jog usan la misma lógica con sentidos opuestos.
    server.on("/api/jog/negative", HTTP_POST, []() { jog(false); });
    server.on("/api/jog/positive", HTTP_POST, []() { jog(true); });
    server.on("/api/position/home", HTTP_POST, []() {
        if (server.header(commandHeader) != "1")
        {
            server.send(403, "text/plain", "Comando rechazado.");
            return;
        }
        if (!Tmc5160::setHomePosition())
        {
            server.send(409, "text/plain; charset=utf-8", "Detén el motor antes de establecer el inicio.");
            return;
        }
        EventLog::add("Posición de inicio establecida: %.8f pasos.",
                      double(Tmc5160::position().homeUnits) / Tmc5160::positionUnitsPerStep);
        server.send(204);
    });
    server.on("/api/position", HTTP_POST, []() {
        if (server.header(commandHeader) != "1")
        {
            server.send(403, "text/plain", "Comando rechazado.");
            return;
        }
        if (server.hasArg("after") && server.arg("after") != String(motionRevision))
        {
            server.send(409, "text/plain; charset=utf-8", "Otra orden de movimiento cambió la sesión.");
            return;
        }
        const bool home = server.arg("home") == "1";
        long steps = 0;
        if (!home)
        {
            const String argument = server.arg("steps");
            char* end = nullptr;
            errno = 0;
            steps = strtol(argument.c_str(), &end, 10);
            if (end == argument.c_str() || *end != '\0' || errno == ERANGE ||
                steps < -Tmc5160::maxPositionSteps || steps > Tmc5160::maxPositionSteps)
            {
                server.send(400, "text/plain; charset=utf-8", "Introduce una posición entera entre −1.000.000 y 1.000.000 pasos.");
                return;
            }
        }
        const uint32_t speed = requestSpeed();
        uint32_t rampMs;
        uint32_t decelerationMs;
        if (speed == 0 || !requestRamps(speed, rampMs, decelerationMs))
        {
            return;
        }
        const bool accepted = home ? Tmc5160::moveHome(speed, rampMs, decelerationMs)
                                   : Tmc5160::moveToPosition(steps, speed, rampMs, decelerationMs);
        if (!accepted)
        {
            server.send(409, "text/plain; charset=utf-8",
                        "No se pudo iniciar el posicionamiento. Comprueba que el driver esté listo y el motor detenido.");
            return;
        }
        ++jogId;
        ++motionRevision;
        EventLog::add("%s: %.8f pasos, velocidad máxima %lu micropasos/s.",
                      home ? "Regreso a inicio" : "Objetivo de posición",
                      double(Tmc5160::position().targetUnits) / Tmc5160::positionUnitsPerStep,
                      static_cast<unsigned long>(speed));
        server.send(200, "text/plain", String(jogId));
    });
    server.on("/api/pins", HTTP_POST, []() {
        if (server.header(commandHeader) != "1")
        {
            server.send(403, "text/plain", "Comando rechazado.");
            return;
        }
        const String value = server.arg("value");
        if (value.length() != 1 || value[0] < '0' || value[0] > '0' + Tmc5160::allPinInversions)
        {
            server.send(400, "text/plain; charset=utf-8", "Selección de polaridad no válida.");
            return;
        }
        if (Tmc5160::isEnabled() || Tmc5160::isMoving())
        {
            server.send(409, "text/plain; charset=utf-8", "Pon el driver en OFF antes de cambiar la polaridad.");
            return;
        }
        if (!Tmc5160::selectPinInversions(static_cast<uint8_t>(value[0] - '0')))
        {
            server.send(409, "text/plain; charset=utf-8",
                        "El TMC5160 no confirmó EN en estado inactivo. Usa EN normal con cableado directo.");
            return;
        }
        const unsigned pins = Tmc5160::pinInversions();
        EventLog::add("Polaridad invertida: DIR=%s, EN=%s, CLK/STEP=%s.",
                      (pins & Tmc5160::invertDirection) ? "sí" : "no",
                      (pins & Tmc5160::invertEnable) ? "sí" : "no",
                      (pins & Tmc5160::invertStep) ? "sí" : "no");
        char response[32];
        snprintf(response, sizeof(response), "{\"pinInversions\":%u}", pins);
        server.send(200, "application/json", response);
    });
    // Valida la resolución y devuelve los límites de movimiento recalculados.
    server.on("/api/microsteps", HTTP_POST, []() {
        if (server.header(commandHeader) != "1")
        {
            server.send(403, "text/plain", "Comando rechazado.");
            return;
        }
        const String value = server.arg("value");
        char* end = nullptr;
        const unsigned long count = strtoul(value.c_str(), &end, 10);
        if (*end != '\0' || count == 0 || count > 256 || (count & (count - 1)) != 0)
        {
            server.send(400, "text/plain", "Selecciona una resolucion de micropasos valida.");
            return;
        }
        if (!Tmc5160::selectMicrosteps(static_cast<uint16_t>(count)))
        {
            server.send(409, "text/plain", "Pon el driver en OFF antes de cambiar los micropasos.");
            return;
        }
        EventLog::add("Resolución: %lu micropasos por paso.", count);
        sendMotionLimits();
    });
    // Procesa el interruptor de habilitación de la salida del driver.
    server.on("/api/driver", HTTP_POST, []() {
        if (server.header(commandHeader) != "1")
        {
            server.send(403, "text/plain", "Comando rechazado.");
            return;
        }
        const String enabled = server.arg("enabled");
        if (enabled == "0")
        {
            ++motionRevision;
            Tmc5160::disable("orden del panel");
        }
        else if (enabled == "1")
        {
            if (WifiConnection::isChanging() || WiFi.scanComplete() == WIFI_SCAN_RUNNING)
            {
                server.send(409, "text/plain; charset=utf-8", "Espera a que termine la búsqueda o el cambio de red Wi-Fi.");
                return;
            }
            if (!Tmc5160::enable())
            {
                EventLog::add("Falló la orden de habilitar el driver. Revisar SPI, STEP/DIR y EN.");
                server.send(409, "text/plain", "No se pudo habilitar el driver. Revisa SPI y EN.");
                return;
            }
            EventLog::add("Driver ON.");
        }
        else
        {
            server.send(400, "text/plain", "Estado del driver no valido.");
            return;
        }
        server.send(204);
    });
    // Recibe la corriente de marcha en miliamperios y delega su validación al driver.
    server.on("/api/current", HTTP_POST, []() {
        if (server.header(commandHeader) != "1")
        {
            server.send(403, "text/plain", "Comando rechazado.");
            return;
        }
        const String argument = server.arg("ma");
        char* end = nullptr;
        const unsigned long milliamps = strtoul(argument.c_str(), &end, 10);
        if (*end != '\0' || milliamps < Tmc5160::minRunCurrentMa ||
            milliamps > Tmc5160::maxRunCurrentMa || milliamps % 10 != 0)
        {
            server.send(400, "text/plain", "Corriente valida: 0,10 a 1,20 A RMS, en pasos de 0,01 A.");
            return;
        }
        if (!Tmc5160::selectRunCurrent(static_cast<uint16_t>(milliamps)))
        {
            server.send(409, "text/plain", "Pon el driver en OFF antes de cambiar la corriente.");
            return;
        }
        char response[64];
        EventLog::add("Corriente RMS: marcha %lu mA, mantenimiento %u mA.",
                      milliamps, unsigned(Tmc5160::holdCurrentMa()));
        snprintf(response, sizeof(response), "{\"runCurrentMa\":%u,\"holdCurrentMa\":%u}",
                 unsigned(Tmc5160::runCurrentMa()), unsigned(Tmc5160::holdCurrentMa()));
        server.send(200, "application/json", response);
    });
    // Permite solicitar el modo silencioso del driver.
    server.on("/api/silent-mode", HTTP_POST, []() {
        if (server.header(commandHeader) != "1")
        {
            server.send(403, "text/plain", "Comando rechazado.");
            return;
        }
        const String value = server.arg("value");
        if (value != "0" && value != "1")
        {
            server.send(400, "text/plain", "Modo silencioso no valido.");
            return;
        }
        if (!Tmc5160::selectSilentMode(value == "1"))
        {
            server.send(409, "text/plain", "Pon el driver en OFF antes de cambiar el modo.");
            return;
        }
        EventLog::add("Modo seleccionado: %s.", value == "1" ? "StealthChop2" : "SpreadCycle");
        server.send(200, "application/json", value == "1" ? "{\"silentMode\":true}"
                                                           : "{\"silentMode\":false}");
    });
    // Renueva el plazo del movimiento si el identificador coincide con el Jog actual.
    server.on("/api/jog/keepalive", HTTP_POST, []() {
        if (server.header(commandHeader) != "1")
        {
            server.send(403, "text/plain", "Comando rechazado.");
            return;
        }
        const bool program = GcodePlanner::state() == GcodePlanner::State::running;
        if (server.arg("id") != String(jogId) || !(program ? GcodePlanner::renew() : Tmc5160::renewJog()))
        {
            server.send(409, "text/plain", "El movimiento ha terminado.");
            return;
        }
        server.send(204);
    });
    // Cambia la velocidad del Jog vigente sin iniciar otro movimiento.
    server.on("/api/jog/speed", HTTP_POST, []() {
        if (server.header(commandHeader) != "1")
        {
            server.send(403, "text/plain", "Comando rechazado.");
            return;
        }
        const uint32_t speed = requestSpeed();
        if (speed == 0)
        {
            return;
        }
        if (server.arg("id") != String(jogId) || !Tmc5160::changeJogSpeed(speed))
        {
            server.send(409, "text/plain", "El movimiento no admite un cambio de velocidad.");
            return;
        }
        EventLog::add("Jog #%lu: velocidad solicitada %lu micropasos/s.",
                      jogId, static_cast<unsigned long>(speed));
        server.send(204);
    });
    // Solicita una parada con la deceleración configurada.
    server.on("/api/stop", HTTP_POST, []() {
        if (server.header(commandHeader) != "1")
        {
            server.send(403, "text/plain", "Comando rechazado.");
            return;
        }
        ++motionRevision;
        Tmc5160::stop();
        EventLog::add("Orden STOP desde el panel.");
        server.send(204);
    });
}

// Arranca HTTP al conectar. Si cae la red, deshabilita el motor y detiene HTTP.
void MotorWebServer::update(bool wifiOnline)
{
    static IPAddress servedAddress;
    if (!wifiOnline)
    {
        // El menú serie puede haber tomado el control del escaneo.
        webScanRequested = false;
        if (running)
        {
            EventLog::add("Conexión Wi-Fi perdida. Servidor web detenido.");
            Tmc5160::disable("conexión Wi-Fi perdida");
            server.stop();
            running = false;
        }
        return;
    }
    if (WifiConnection::isAccessPoint() &&
        WiFi.getMode() == WIFI_AP_STA && WiFi.scanComplete() != WIFI_SCAN_RUNNING)
    {
        // El escaneo activa la estación temporalmente. El punto de acceso queda activo.
        WiFi.enableSTA(false);
    }
    if (webScanRequested && millis() - webScanStarted >= wifiScanRetentionMs &&
        WiFi.scanComplete() != WIFI_SCAN_RUNNING)
    {
        WiFi.scanDelete();
        webScanRequested = false;
    }

    const IPAddress address = wifiAddress();
    if (!running || servedAddress != address)
    {
        if (!running)
        {
            server.begin();
        }
        running = true;
        servedAddress = address;
        EventLog::add("Wi-Fi activa (%s). Panel en http://%s/.",
                      WifiConnection::isAccessPoint() ? "punto de acceso" : "estación",
                      address.toString().c_str());
        Serial.print("MotorPAP disponible en http://");
        Serial.print(address);
        Serial.println("/ (envia R para cambiar de red)");
    }

    server.handleClient();
}
