#pragma once

#include <stdint.h>

namespace Tmc5160
{
constexpr uint16_t minJogSpeed = 100;
constexpr uint16_t motorStepsPerRevolution = 200;
constexpr uint32_t minRampMs = 100;
constexpr uint32_t defaultRampMs = 200;
constexpr uint32_t maxRampMs = 600000;
constexpr uint16_t minRunCurrentMa = 100;
constexpr uint16_t maxRunCurrentMa = 1200;
constexpr uint16_t defaultRunCurrentMa = 800;

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

void begin();
void update();
Status status();
void testMotor();
bool startJog(bool positive, uint32_t speed, uint32_t rampMs, uint32_t decelerationMs);
bool changeJogSpeed(uint32_t speed);
bool changeJogDirection(bool positive);
bool renewJog();
bool isMoving();
bool isDecelerating();
bool isStopping();
bool isReversing();
bool jogDirection();
bool requestedJogDirection();
uint16_t microsteps();
uint16_t maxJogRpm();
uint32_t maxJogSpeed();
uint32_t accelerationMilliPulses(uint16_t rpmPerSecond);
uint32_t minJogAcceleration();
uint32_t maxJogAcceleration();
bool selectMicrosteps(uint16_t count);
uint16_t runCurrentMa();
uint16_t holdCurrentMa();
bool selectRunCurrent(uint16_t milliamps);
bool silentMode();
bool silentInitializing();
bool selectSilentMode(bool enabled);
bool enable();
bool isEnabled();
void disable();
void stop();
}
