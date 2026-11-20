// Interfaz pública del driver TMC5160. Define las unidades, límites y operaciones que usa el firmware.
#pragma once

#include <stdint.h>

namespace Tmc5160
{
// Velocidad en micropasos/s, tiempos en milisegundos y corriente RMS en miliamperios.
constexpr uint16_t minJogSpeed = 100;
constexpr uint16_t motorStepsPerRevolution = 200;
constexpr uint32_t minRampMs = 100;
constexpr uint32_t defaultRampMs = 200;
constexpr uint32_t maxRampMs = 600000;
constexpr uint16_t minRunCurrentMa = 100;
constexpr uint16_t maxRunCurrentMa = 1200;
constexpr uint16_t defaultRunCurrentMa = 800;

// Diagnóstico de la última lectura SPI. Un resultado StallGuard negativo indica que no está disponible.
struct Status
{
    bool responding;
    bool outputDisabled;
    bool stepDirMode;
    bool silentActive;
    bool diagnosticsAvailable;
    bool overtemperature;
    bool temperatureWarning;
    bool undervoltage;
    bool driverError;
    bool shortCircuit;
    int16_t stallGuardResult;
};

// Inicialización, mantenimiento desde loop() y consulta del diagnóstico.
void begin();
void update();
Status status();
// Operaciones del movimiento manual. renewJog() mantiene activa la sesión mientras llegan órdenes.
bool startJog(bool positive, uint32_t speed, uint32_t rampMs, uint32_t decelerationMs);
bool changeJogSpeed(uint32_t speed);
bool changeJogDirection(bool positive);
bool renewJog();
// Estado del movimiento, la rampa y el sentido efectivo o solicitado.
bool isMoving();
bool isDecelerating();
bool isStopping();
bool isReversing();
bool jogDirection();
bool requestedJogDirection();
// Resolución y límites que dependen de los micropasos configurados.
uint16_t microsteps();
uint16_t maxJogRpm();
uint32_t maxJogSpeed();
// Convierte RPM/s a milésimas de micropaso por segundo cuadrado.
uint32_t accelerationMilliPulses(uint16_t rpmPerSecond);
uint32_t minJogAcceleration();
uint32_t maxJogAcceleration();
// Cambia la resolución de micropasos con la salida deshabilitada.
bool selectMicrosteps(uint16_t count);
// Corrientes RMS de marcha y mantenimiento en miliamperios.
uint16_t runCurrentMa();
uint16_t holdCurrentMa();
bool selectRunCurrent(uint16_t milliamps);
// Consulta y selección del modo silencioso, incluido su ajuste inicial.
bool silentMode();
bool silentInitializing();
bool selectSilentMode(bool enabled);
// Habilita la salida con la configuración seleccionada.
bool enable();
bool isEnabled();
// Corta los pulsos y deshabilita la salida.
void disable();
// Solicita una parada con rampa y mantiene la sujeción.
void stop();
}
