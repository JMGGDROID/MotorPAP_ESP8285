#include "MotorWebServer.h"

#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>

#include "MotorPanel.h"
#include "Tmc5160.h"

namespace
{
ESP8266WebServer server(80);
bool running = false;
unsigned long jogId = 0;
bool clockwise = true;
constexpr char commandHeader[] = "X-MotorPAP-Command";

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
            !Tmc5160::changeJogDirection(positive == clockwise))
        {
            server.send(409, "text/plain", "El movimiento no admite un cambio de sentido.");
            return;
        }
        server.send(204);
        return;
    }
    const uint32_t speed = requestSpeed();
    if (speed == 0)
    {
        return;
    }

    char* end = nullptr;
    uint32_t rampMs = Tmc5160::defaultRampMs;
    if (server.hasArg("rampMs"))
    {
        const String argument = server.arg("rampMs");
        rampMs = strtoul(argument.c_str(), &end, 10);
        if (*end != '\0' || rampMs < Tmc5160::minRampMs || rampMs > Tmc5160::maxRampMs)
        {
            server.send(400, "text/plain", "Configura un tiempo de aceleracion de 0,10 a 600 segundos.");
            return;
        }
    }
    else if (server.hasArg("accelerationRpm"))
    {
        const String argument = server.arg("accelerationRpm");
        const unsigned long rpm = strtoul(argument.c_str(), &end, 10);
        if (*end != '\0' || rpm < 1 || rpm > Tmc5160::maxJogRpm())
        {
            server.send(400, "text/plain", "Aceleracion fuera del rango de RPM/s de esta resolucion.");
            return;
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
            return;
        }
        rampMs = uint64_t(speed) * 1000 / pulses;
    }

    uint32_t decelerationMs = Tmc5160::defaultRampMs;
    if (server.hasArg("decelerationMs"))
    {
        const String argument = server.arg("decelerationMs");
        decelerationMs = strtoul(argument.c_str(), &end, 10);
        if (*end != '\0' || decelerationMs < Tmc5160::minRampMs ||
            decelerationMs > Tmc5160::maxRampMs)
        {
            server.send(400, "text/plain", "Configura un tiempo de deceleracion de 0,10 a 600 segundos.");
            return;
        }
    }

    if (Tmc5160::silentInitializing())
    {
        server.send(409, "text/plain", "StealthChop se esta ajustando. Espera a que termine.");
        return;
    }
    if (!Tmc5160::startJog(positive == clockwise, speed, max(rampMs, Tmc5160::minRampMs),
                           decelerationMs))
    {
        server.send(409, "text/plain", "Habilita el driver y comprueba que este detenido y conectado.");
        return;
    }

    ++jogId;
    server.send(200, "text/plain", String(jogId));
}
}

void MotorWebServer::begin()
{
    server.collectHeaders(commandHeader);
    server.on("/", HTTP_GET, []() {
        server.send_P(200, PSTR("text/html; charset=utf-8"), motorPanelHtml);
    });
    server.on("/api/status", HTTP_GET, []() {
        const auto driver = Tmc5160::status();
        char response[680];
        snprintf(response, sizeof(response),
                 "{\"ip\":\"%s\",\"rssi\":%d,\"uptime\":%lu,"
                 "\"driverResponding\":%s,\"driverDisabled\":%s,\"driverEnabled\":%s,"
                 "\"stepDirMode\":%s,\"moving\":%s,\"decelerating\":%s,\"stopping\":%s,"
                 "\"clockwise\":%s,\"jogClockwise\":%s,\"requestedClockwise\":%s,\"reversing\":%s,"
                 "\"microsteps\":%u,\"maxRpm\":%u,\"maxPulseSpeed\":%u,"
                 "\"minAcceleration\":%u,\"maxAcceleration\":%u,"
                 "\"runCurrentMa\":%u,\"holdCurrentMa\":%u,\"silentMode\":%s,"
                 "\"silentInitializing\":%s,\"silentActive\":%s,"
                 "\"diagnosticsAvailable\":%s,\"overtemperature\":%s,\"temperatureWarning\":%s,"
                 "\"undervoltage\":%s,\"driverError\":%s,\"shortCircuit\":%s,\"stallGuardResult\":%d}",
                 WiFi.localIP().toString().c_str(), WiFi.RSSI(), millis() / 1000,
                 driver.responding ? "true" : "false", driver.outputDisabled ? "true" : "false",
                 Tmc5160::isEnabled() ? "true" : "false",
                 driver.stepDirMode ? "true" : "false", Tmc5160::isMoving() ? "true" : "false",
                 Tmc5160::isDecelerating() ? "true" : "false",
                 Tmc5160::isStopping() ? "true" : "false",
                 clockwise ? "true" : "false",
                 Tmc5160::jogDirection() ? "true" : "false",
                 Tmc5160::requestedJogDirection() ? "true" : "false",
                 Tmc5160::isReversing() ? "true" : "false",
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
    server.on("/api/jog/negative", HTTP_POST, []() { jog(false); });
    server.on("/api/jog/positive", HTTP_POST, []() { jog(true); });
    server.on("/api/direction", HTTP_POST, []() {
        if (server.header(commandHeader) != "1")
        {
            server.send(403, "text/plain", "Comando rechazado.");
            return;
        }
        const String direction = server.arg("value");
        if (direction != "cw" && direction != "ccw")
        {
            server.send(400, "text/plain", "Sentido no valido.");
            return;
        }
        const bool positive = direction == "cw";
        if (Tmc5160::isMoving() || server.hasArg("id"))
        {
            if (server.arg("id") != String(jogId) || !Tmc5160::changeJogDirection(positive))
            {
                server.send(409, "text/plain", "El movimiento no admite un cambio de sentido.");
                return;
            }
        }
        clockwise = positive;
        server.send(204);
    });
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
        sendMotionLimits();
    });
    server.on("/api/driver", HTTP_POST, []() {
        if (server.header(commandHeader) != "1")
        {
            server.send(403, "text/plain", "Comando rechazado.");
            return;
        }
        const String enabled = server.arg("enabled");
        if (enabled == "0")
        {
            Tmc5160::disable();
        }
        else if (enabled == "1")
        {
            if (!Tmc5160::enable())
            {
                server.send(409, "text/plain", "No se pudo habilitar el driver. Revisa SPI y EN.");
                return;
            }
        }
        else
        {
            server.send(400, "text/plain", "Estado del driver no valido.");
            return;
        }
        server.send(204);
    });
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
        snprintf(response, sizeof(response), "{\"runCurrentMa\":%u,\"holdCurrentMa\":%u}",
                 unsigned(Tmc5160::runCurrentMa()), unsigned(Tmc5160::holdCurrentMa()));
        server.send(200, "application/json", response);
    });
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
        server.send(200, "application/json", value == "1" ? "{\"silentMode\":true}"
                                                           : "{\"silentMode\":false}");
    });
    server.on("/api/jog/keepalive", HTTP_POST, []() {
        if (server.header(commandHeader) != "1")
        {
            server.send(403, "text/plain", "Comando rechazado.");
            return;
        }
        if (server.arg("id") != String(jogId) || !Tmc5160::renewJog())
        {
            server.send(409, "text/plain", "El movimiento ha terminado.");
            return;
        }
        server.send(204);
    });
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
        server.send(204);
    });
    server.on("/api/stop", HTTP_POST, []() {
        if (server.header(commandHeader) != "1")
        {
            server.send(403, "text/plain", "Comando rechazado.");
            return;
        }
        Tmc5160::stop();
        server.send(204);
    });
}

void MotorWebServer::update(bool wifiOnline)
{
    if (!wifiOnline)
    {
        if (running)
        {
            Tmc5160::disable();
            server.stop();
            running = false;
        }
        return;
    }

    if (!running)
    {
        server.begin();
        running = true;
        Serial.print("MotorPAP disponible en http://");
        Serial.print(WiFi.localIP());
        Serial.println("/ (envia R para cambiar de red)");
    }

    server.handleClient();
}
