#include "Tmc5160.h"

#include <Arduino.h>
#include <SPI.h>
#include <math.h>

namespace
{
constexpr uint8_t chipSelectPin = D1;
constexpr uint8_t enablePin = D2;
constexpr uint8_t stepPin = D0;
constexpr uint8_t directionPin = D8;
constexpr uint8_t ioinRegister = 0x04;
constexpr uint8_t globalConfigRegister = 0x00;
constexpr uint8_t globalStatusRegister = 0x01;
constexpr uint8_t globalScalerRegister = 0x0B;
constexpr uint8_t currentRegister = 0x10;
constexpr uint8_t powerDownRegister = 0x11;
constexpr uint8_t pwmThresholdRegister = 0x13;
constexpr uint8_t microstepCounterRegister = 0x6A;
constexpr uint8_t chopperRegister = 0x6C;
constexpr uint8_t driverStatusRegister = 0x6F;
constexpr uint8_t pwmConfigRegister = 0x70;
constexpr uint32_t enableQuietMask = 1UL << 2;
constexpr uint32_t quietActiveMask = 1UL << 14;
constexpr uint32_t standstillMask = 1UL << 31;
constexpr uint32_t temperatureWarningMask = 1UL << 26;
constexpr uint32_t overtemperatureMask = 1UL << 25;
constexpr uint32_t shortCircuitMask = (3UL << 27) | (3UL << 12);
constexpr uint32_t driverErrorMask = 1UL << 1;
constexpr uint32_t undervoltageMask = 1UL << 2;
constexpr uint16_t stallGuardMinRpm = 10;
constexpr uint32_t quietPwmConfig = 0xC40C001E; // Autoescala y autoajuste de StealthChop2.
constexpr unsigned long quietSetupMs = 180;
constexpr uint32_t chopperBase = 0x10410153; // Sin el campo MRES; TOFF=3.
constexpr uint8_t microstepResolutionShift = 24;
constexpr uint8_t expectedVersion = 0x30;
constexpr uint32_t enableInputMask = 1UL << 4;
constexpr uint32_t stepDirModeMask = 1UL << 6;
constexpr unsigned long pollInterval = 1000;
constexpr uint16_t jogPulses = 160;
constexpr uint32_t timerFrequency = 80000000; // Timer1 sin divisor.
constexpr uint32_t commandTimeoutSeconds = 3;
constexpr uint32_t keepaliveTicks = timerFrequency * commandTimeoutSeconds;
constexpr unsigned long rampInterval = 2;
constexpr uint32_t startupTick = timerFrequency / 1000;
constexpr uint32_t maxTimerTicks = 0x7FFFFF;
constexpr uint32_t fullScaleRmsMicroamps = 3064129; // 0,325 V / 0,075 ohm / sqrt(2).
constexpr uint32_t currentScale = 256U * 32U;
constexpr uint16_t holdLimitMa = 400;
const SPISettings driverSpi(500000, MSBFIRST, SPI_MODE3);

struct CurrentSettings
{
    uint16_t scaler;
    uint8_t run;
    uint8_t hold;
};

uint32_t currentProductLimit(uint16_t milliamps)
{
    return uint64_t(milliamps) * 1000 * currentScale / fullScaleRmsMicroamps;
}

CurrentSettings calculateCurrent(uint16_t milliamps)
{
    const uint32_t limit = currentProductLimit(milliamps);
    CurrentSettings best{128, 0, 0};
    uint32_t bestProduct = 0;
    for (uint8_t run = 0; run < 32; ++run)
    {
        const uint32_t scaler = min(256U, limit / (run + 1));
        const uint32_t product = scaler * (run + 1);
        if (scaler >= 128 && product > bestProduct)
        {
            best.scaler = static_cast<uint16_t>(scaler);
            best.run = run;
            bestProduct = product;
        }
    }
    best.hold = min(uint32_t(best.run), currentProductLimit(holdLimitMa) / best.scaler - 1);
    return best;
}

uint16_t selectedRunCurrentMa = Tmc5160::defaultRunCurrentMa;
CurrentSettings selectedCurrent = calculateCurrent(selectedRunCurrentMa);
bool selectedSilentMode = false;
bool activeSilentMode = false;
bool quietSetupPending = false;
unsigned long quietSetupStarted = 0;
uint32_t quietHoldConfig = 0;
bool enableOutput(const CurrentSettings& current, bool quiet);

volatile uint32_t halfPeriodsRemaining = 0;
volatile uint32_t ticksUntilTimeout = 0;
volatile uint32_t activeHalfPeriod = 0;
volatile uint32_t requestedHalfPeriod = 0;
volatile uint32_t firstStepTicksRemaining = 0;
volatile bool waitingForFirstStep = false;
volatile bool stepHigh = false;
volatile bool moving = false;
volatile bool outputCommanded = false;
volatile bool continuousJog = false;
uint32_t targetSpeed = 0;
uint32_t selectedJogSpeed = 0;
bool activeDirection = false;
bool requestedDirection = false;
bool reversing = false;
uint32_t jogRampMs = 0;
uint32_t jogDecelerationMs = Tmc5160::defaultRampMs;
uint32_t rampFromSpeed = 0;
uint32_t rampDurationMs = 0;
unsigned long rampStarted = 0;
unsigned long lastRampUpdate = 0;
bool rampFinished = false;
uint8_t microstepResolution = 4; // MRES=4 corresponde a 16 micropasos.
Tmc5160::Status currentStatus{};
unsigned long lastPoll = 0;
bool firstPoll = true;
bool serialTestRunning = false;
uint32_t serialTestStartCounter = 0;

void IRAM_ATTR stopPulses(bool disableOutput)
{
    if (disableOutput)
    {
        GPOS = 1UL << enablePin;
        outputCommanded = false;
    }
    timer1_disable();
    GP16O &= ~1U;
    stepHigh = false;
    moving = false;
}

void IRAM_ATTR onStepTimer()
{
    if (!moving)
    {
        return;
    }

    if (continuousJog)
    {
        if (ticksUntilTimeout <= activeHalfPeriod)
        {
            stopPulses(true);
            return;
        }
        ticksUntilTimeout -= activeHalfPeriod;
    }

    if (waitingForFirstStep)
    {
        if (firstStepTicksRemaining > startupTick)
        {
            firstStepTicksRemaining -= startupTick;
            return;
        }
        firstStepTicksRemaining = 0;
        if (requestedHalfPeriod == 0)
        {
            return;
        }

        waitingForFirstStep = false;
        activeHalfPeriod = requestedHalfPeriod;
        timer1_write(activeHalfPeriod);
    }

    // D0 es GPIO16, con un registro de salida propio.
    if (stepHigh)
    {
        GP16O &= ~1U;
    }
    else
    {
        GP16O |= 1U;
    }
    stepHigh = !stepHigh;
    if (!continuousJog && --halfPeriodsRemaining == 0)
    {
        stopPulses(true);
        return;
    }

    if (!stepHigh && activeHalfPeriod != requestedHalfPeriod)
    {
        activeHalfPeriod = requestedHalfPeriod;
        timer1_write(activeHalfPeriod);
    }
}

uint32_t transferRegister(uint8_t address, uint32_t data = 0)
{
    digitalWrite(chipSelectPin, LOW);
    SPI.transfer(address);

    uint32_t value = 0;
    for (int shift = 24; shift >= 0; shift -= 8)
    {
        value = (value << 8) | SPI.transfer(static_cast<uint8_t>(data >> shift));
    }
    digitalWrite(chipSelectPin, HIGH);
    delayMicroseconds(1);
    return value;
}

uint32_t readRegister(uint8_t address)
{
    SPI.beginTransaction(driverSpi);
    // La respuesta llega en la trama siguiente.
    transferRegister(address);
    const uint32_t value = transferRegister(address);
    SPI.endTransaction();
    return value;
}

void writeRegister(uint8_t address, uint32_t value)
{
    SPI.beginTransaction(driverSpi);
    transferRegister(address | 0x80, value);
    SPI.endTransaction();
}

uint32_t halfPeriodForSpeed(uint32_t speed)
{
    const uint32_t frequency = 2UL * speed;
    return (timerFrequency + frequency - 1) / frequency;
}

bool startMotion(bool positive, uint32_t speed, uint32_t rampMs);

uint32_t pulseSpeed()
{
    noInterrupts();
    const uint32_t period = activeHalfPeriod;
    const bool pulsing = moving && !waitingForFirstStep;
    interrupts();
    return pulsing ? (timerFrequency + period) / (2UL * period) : 0;
}

uint32_t firstStepDelay(uint32_t speed, uint32_t durationMs)
{
    // Desde reposo, el primer impulso llega cuando la rampa ha recorrido un micropaso.
    return static_cast<uint32_t>(ceil(sqrt(2000.0 * durationMs / speed)) * startupTick);
}

void setSpeedRamp(uint32_t from, uint32_t to, uint32_t durationMs)
{
    rampFromSpeed = from;
    targetSpeed = to;
    rampDurationMs = durationMs;
    rampStarted = lastRampUpdate = millis();
    rampFinished = false;
}

bool restartJogFromRest()
{
    const uint32_t firstDelay = firstStepDelay(selectedJogSpeed, jogRampMs);
    noInterrupts();
    if (!moving)
    {
        interrupts();
        return false;
    }

    timer1_disable();
    GP16O &= ~1U;
    stepHigh = false;
    if (requestedDirection)
    {
        GPOS = 1UL << directionPin;
    }
    else
    {
        GPOC = 1UL << directionPin;
    }
    activeDirection = requestedDirection;
    waitingForFirstStep = true;
    firstStepTicksRemaining = firstDelay;
    activeHalfPeriod = startupTick;
    requestedHalfPeriod = 0;
    timer1_enable(TIM_DIV1, TIM_EDGE, TIM_LOOP);
    timer1_write(activeHalfPeriod);
    interrupts();

    reversing = false;
    setSpeedRamp(0, selectedJogSpeed, jogRampMs);
    return true;
}

void updateRamp(unsigned long now)
{
    if (!moving || !continuousJog || rampFinished ||
        now - lastRampUpdate < rampInterval)
    {
        return;
    }
    lastRampUpdate = now;
    const unsigned long elapsed = now - rampStarted;
    rampFinished = elapsed >= rampDurationMs;
    if (rampFinished && targetSpeed == 0)
    {
        if (reversing)
        {
            restartJogFromRest();
            return;
        }
        noInterrupts();
        stopPulses(false);
        interrupts();
        return;
    }
    const uint32_t speed = rampFinished ? targetSpeed
        : (uint64_t(rampFromSpeed) * (rampDurationMs - elapsed) +
           uint64_t(targetSpeed) * elapsed) / rampDurationMs;
    if (speed > 0)
    {
        const uint32_t period = halfPeriodForSpeed(speed);
        if (period <= maxTimerTicks)
        {
            requestedHalfPeriod = period;
        }
    }
}

bool validStepDir(uint32_t ioin)
{
    return (ioin >> 24) == expectedVersion && (ioin & stepDirModeMask) != 0;
}

uint32_t chopperConfig()
{
    return chopperBase | (static_cast<uint32_t>(microstepResolution) << microstepResolutionShift);
}
}

void Tmc5160::begin()
{
    digitalWrite(enablePin, HIGH);
    pinMode(enablePin, OUTPUT);
    digitalWrite(chipSelectPin, HIGH);
    pinMode(chipSelectPin, OUTPUT);
    digitalWrite(stepPin, LOW);
    pinMode(stepPin, OUTPUT);
    digitalWrite(directionPin, LOW);
    pinMode(directionPin, OUTPUT);
    SPI.begin(); // D5 SCK, D6 MISO y D7 MOSI.
    timer1_attachInterrupt(onStepTimer);
}

void Tmc5160::update()
{
    if (serialTestRunning && !isMoving())
    {
        const uint32_t counter = readRegister(microstepCounterRegister);
        Serial.printf("PRUEBA08 terminada: MSCNT %lu -> %lu.\n",
                      static_cast<unsigned long>(serialTestStartCounter),
                      static_cast<unsigned long>(counter));
        serialTestRunning = false;
    }

    const unsigned long now = millis();
    if (quietSetupPending && outputCommanded && now - quietSetupStarted >= quietSetupMs)
    {
        writeRegister(currentRegister, quietHoldConfig);
        quietSetupPending = false;
    }
    updateRamp(now);
    if (!firstPoll && now - lastPoll < pollInterval)
    {
        return;
    }
    lastPoll = now;

    const uint32_t ioin = readRegister(ioinRegister);

    const bool responding = (ioin >> 24) == expectedVersion;
    const bool outputDisabled = responding && (ioin & enableInputMask) != 0;
    const uint32_t driverStatus = responding ? readRegister(driverStatusRegister) : UINT32_MAX;
    const uint32_t globalStatus = responding ? readRegister(globalStatusRegister) : UINT32_MAX;
    const bool hasDiagnostics = driverStatus != UINT32_MAX && globalStatus <= 7;
    const bool silentActive = hasDiagnostics && (driverStatus & quietActiveMask) != 0;
    const bool stallGuardAvailable = hasDiagnostics && outputCommanded && !outputDisabled &&
        (driverStatus & (quietActiveMask | standstillMask)) == 0 &&
        pulseSpeed() * 60 >= uint32_t(stallGuardMinRpm) * motorStepsPerRevolution * microsteps();
    const int16_t stallGuardResult = stallGuardAvailable
        ? static_cast<int16_t>(driverStatus & 0x3FFU) : -1;

    const Status nextStatus{
        responding, outputDisabled, responding && (ioin & stepDirModeMask) != 0,
        silentActive, hasDiagnostics, hasDiagnostics && (driverStatus & overtemperatureMask) != 0,
        hasDiagnostics && (driverStatus & temperatureWarningMask) != 0,
        hasDiagnostics && (globalStatus & undervoltageMask) != 0,
        hasDiagnostics && (globalStatus & driverErrorMask) != 0,
        hasDiagnostics && (driverStatus & shortCircuitMask) != 0,
        stallGuardResult
    };
    if (!nextStatus.responding || !nextStatus.stepDirMode ||
        (outputCommanded && nextStatus.outputDisabled))
    {
        disable();
    }
    if (firstPoll || nextStatus.responding != currentStatus.responding ||
        nextStatus.outputDisabled != currentStatus.outputDisabled)
    {
        Serial.printf("TMC5160 SPI: %s | IOIN=0x%08lX\n",
                      nextStatus.responding ? "identificado" : "sin identificacion",
                      static_cast<unsigned long>(ioin));
    }
    currentStatus = nextStatus;
    firstPoll = false;
}

Tmc5160::Status Tmc5160::status()
{
    return currentStatus;
}

void Tmc5160::testMotor()
{
    const uint32_t counter = readRegister(microstepCounterRegister);
    if (!outputCommanded && enableOutput(calculateCurrent(defaultRunCurrentMa), false))
    {
        if (startMotion(false, 100, 0))
        {
            serialTestStartCounter = counter;
            serialTestRunning = true;
            Serial.println("PRUEBA08 iniciada: 0,8 A RMS nominales, 160 pulsos a 100 Hz.");
            return;
        }
        disable();
    }
    Serial.println("PRUEBA08 cancelada: driver ocupado, habilitado o configuracion incorrecta.");
}

namespace
{
bool startMotion(bool positive, uint32_t speed, uint32_t rampMs)
{
    if (moving || !outputCommanded || quietSetupPending || speed < Tmc5160::minJogSpeed ||
        speed > Tmc5160::maxJogSpeed())
    {
        return false;
    }

    const uint32_t ioin = readRegister(ioinRegister);
    if (!validStepDir(ioin) || (ioin & enableInputMask) != 0 ||
        readRegister(chopperRegister) != chopperConfig() ||
        ((readRegister(globalConfigRegister) & enableQuietMask) != 0) != activeSilentMode)
    {
        Tmc5160::disable();
        return false;
    }

    digitalWrite(directionPin, positive ? HIGH : LOW);
    activeDirection = requestedDirection = positive;
    reversing = false;
    selectedJogSpeed = speed;
    continuousJog = rampMs != 0;
    jogRampMs = rampMs;
    setSpeedRamp(0, speed, rampMs);
    halfPeriodsRemaining = 2UL * jogPulses;
    ticksUntilTimeout = keepaliveTicks;
    waitingForFirstStep = continuousJog;
    firstStepTicksRemaining = continuousJog ? firstStepDelay(speed, rampMs) : 0;
    activeHalfPeriod = continuousJog ? startupTick : halfPeriodForSpeed(speed);
    requestedHalfPeriod = continuousJog ? 0 : activeHalfPeriod;
    moving = true;
    timer1_enable(TIM_DIV1, TIM_EDGE, TIM_LOOP);
    timer1_write(activeHalfPeriod);
    return true;
}
}

namespace
{
bool enableOutput(const CurrentSettings& current, bool quiet)
{
    const uint32_t ioin = readRegister(ioinRegister);
    if (!validStepDir(ioin))
    {
        Tmc5160::disable();
        return false;
    }
    if (outputCommanded)
    {
        if ((ioin & enableInputMask) == 0)
        {
            return true;
        }
        Tmc5160::disable();
        return false;
    }
    if ((ioin & enableInputMask) == 0)
    {
        return false;
    }

    const uint32_t globalConfig = readRegister(globalConfigRegister);
    if (globalConfig == UINT32_MAX)
    {
        return false;
    }
    const uint32_t nextGlobalConfig = quiet ? globalConfig | enableQuietMask
                                            : globalConfig & ~enableQuietMask;
    const uint32_t runConfig = (6UL << 16) | (uint32_t(current.run) << 8);

    SPI.beginTransaction(driverSpi);
    transferRegister(globalScalerRegister | 0x80, current.scaler == 256 ? 0 : current.scaler);
    transferRegister(currentRegister | 0x80, runConfig | (quiet ? current.run : current.hold));
    transferRegister(chopperRegister | 0x80, chopperConfig());
    if (quiet)
    {
        transferRegister(powerDownRegister | 0x80, 10);
        transferRegister(pwmThresholdRegister | 0x80, 0);
        transferRegister(pwmConfigRegister | 0x80, quietPwmConfig);
    }
    transferRegister(globalConfigRegister | 0x80, nextGlobalConfig);
    SPI.endTransaction();

    if (readRegister(chopperRegister) != chopperConfig() ||
        readRegister(globalConfigRegister) != nextGlobalConfig ||
        readRegister(microstepCounterRegister) > 1023)
    {
        return false;
    }

    digitalWrite(enablePin, LOW);
    const uint32_t enabledIoin = readRegister(ioinRegister);
    if (!validStepDir(enabledIoin) || (enabledIoin & enableInputMask) != 0)
    {
        Tmc5160::disable();
        return false;
    }
    activeSilentMode = quiet;
    quietSetupPending = quiet;
    quietSetupStarted = millis();
    quietHoldConfig = runConfig | current.hold;
    outputCommanded = true;
    currentStatus.outputDisabled = false;
    return true;
}
}

bool Tmc5160::enable()
{
    return enableOutput(selectedCurrent, selectedSilentMode);
}

bool Tmc5160::startJog(bool positive, uint32_t speed, uint32_t rampMs, uint32_t decelerationMs)
{
    if (rampMs < minRampMs || rampMs > maxRampMs ||
        decelerationMs < minRampMs || decelerationMs > maxRampMs ||
        !startMotion(positive, speed, rampMs))
    {
        return false;
    }
    jogDecelerationMs = decelerationMs;
    return true;
}

namespace
{
bool retargetJogSpeed(uint32_t speed)
{
    const uint32_t delayFromRest = waitingForFirstStep ? firstStepDelay(speed, jogRampMs) : 0;
    noInterrupts();
    if (!moving)
    {
        interrupts();
        return false;
    }
    const uint32_t period = activeHalfPeriod;
    const bool pulsing = !waitingForFirstStep;
    if (pulsing)
    {
        requestedHalfPeriod = period;
    }
    else
    {
        requestedHalfPeriod = 0;
        firstStepTicksRemaining = delayFromRest;
    }
    interrupts();

    const uint32_t current = pulsing ? (timerFrequency + period) / (2UL * period) : 0;
    setSpeedRamp(current, speed, speed < current ? jogDecelerationMs : jogRampMs);
    return true;
}
}

bool Tmc5160::changeJogSpeed(uint32_t speed)
{
    if (!moving || !continuousJog || isStopping() || speed < minJogSpeed || speed > maxJogSpeed())
    {
        return false;
    }
    if (speed == selectedJogSpeed)
    {
        return true;
    }

    selectedJogSpeed = speed;
    return reversing || retargetJogSpeed(speed);
}

bool Tmc5160::changeJogDirection(bool positive)
{
    if (!moving || !continuousJog || isStopping())
    {
        return false;
    }
    if (positive == requestedDirection)
    {
        return true;
    }

    requestedDirection = positive;
    if (positive == activeDirection)
    {
        reversing = false;
        return retargetJogSpeed(selectedJogSpeed);
    }

    const uint32_t speed = pulseSpeed();
    if (speed == 0)
    {
        return restartJogFromRest();
    }

    noInterrupts();
    requestedHalfPeriod = activeHalfPeriod;
    interrupts();
    reversing = true;
    setSpeedRamp(speed, 0, jogDecelerationMs);
    return true;
}

bool Tmc5160::renewJog()
{
    noInterrupts();
    const bool renewed = moving && continuousJog;
    if (renewed)
    {
        ticksUntilTimeout = keepaliveTicks;
    }
    interrupts();
    return renewed;
}

bool Tmc5160::isMoving()
{
    return moving;
}

bool Tmc5160::isDecelerating()
{
    return moving && !rampFinished && targetSpeed < rampFromSpeed;
}

bool Tmc5160::isStopping()
{
    return moving && continuousJog && targetSpeed == 0 && !reversing;
}

bool Tmc5160::isReversing()
{
    return moving && reversing;
}

bool Tmc5160::jogDirection()
{
    return activeDirection;
}

bool Tmc5160::requestedJogDirection()
{
    return requestedDirection;
}
bool Tmc5160::isEnabled()
{
    return outputCommanded;
}


uint16_t Tmc5160::microsteps()
{
    return 256U >> microstepResolution;
}

uint16_t Tmc5160::maxJogRpm()
{
    return microsteps() <= 16 ? 550 : 100;
}

uint32_t Tmc5160::maxJogSpeed()
{
    return uint32_t(maxJogRpm()) * motorStepsPerRevolution * microsteps() / 60;
}

uint32_t Tmc5160::accelerationMilliPulses(uint16_t rpmPerSecond)
{
    return uint64_t(rpmPerSecond) * motorStepsPerRevolution * microsteps() * 1000 / 60;
}

uint32_t Tmc5160::minJogAcceleration()
{
    return (accelerationMilliPulses(1) + 999) / 1000;
}

uint32_t Tmc5160::maxJogAcceleration()
{
    return maxJogSpeed();
}

bool Tmc5160::selectMicrosteps(uint16_t count)
{
    if (outputCommanded || moving || count == 0 || count > 256 ||
        (count & (count - 1)) != 0)
    {
        return false;
    }

    uint8_t resolution = 0;
    for (uint16_t steps = 256; steps > count; steps >>= 1)
    {
        ++resolution;
    }
    microstepResolution = resolution;
    return true;
}

uint16_t Tmc5160::runCurrentMa()
{
    return selectedRunCurrentMa;
}

uint16_t Tmc5160::holdCurrentMa()
{
    const uint32_t product = uint32_t(selectedCurrent.scaler) * (selectedCurrent.hold + 1);
    return (uint64_t(product) * fullScaleRmsMicroamps + 500 * currentScale) /
           (1000 * currentScale);
}

bool Tmc5160::selectRunCurrent(uint16_t milliamps)
{
    if (outputCommanded || moving || milliamps < minRunCurrentMa ||
        milliamps > maxRunCurrentMa || milliamps % 10 != 0)
    {
        return false;
    }
    selectedRunCurrentMa = milliamps;
    selectedCurrent = calculateCurrent(milliamps);
    return true;
}

bool Tmc5160::silentMode()
{
    return selectedSilentMode;
}

bool Tmc5160::silentInitializing()
{
    return outputCommanded && quietSetupPending;
}

bool Tmc5160::selectSilentMode(bool enabled)
{
    if (outputCommanded || moving)
    {
        return false;
    }
    selectedSilentMode = enabled;
    return true;
}

void Tmc5160::stop()
{
    if (!moving || isStopping())
    {
        return;
    }

    if (reversing)
    {
        reversing = false;
        return;
    }

    const uint32_t speed = pulseSpeed();
    if (continuousJog && speed > 0)
    {
        noInterrupts();
        requestedHalfPeriod = activeHalfPeriod;
        interrupts();
        setSpeedRamp(speed, 0, jogDecelerationMs);
        return;
    }

    noInterrupts();
    stopPulses(false);
    interrupts();
}

void Tmc5160::disable()
{
    noInterrupts();
    stopPulses(true);
    interrupts();
}
