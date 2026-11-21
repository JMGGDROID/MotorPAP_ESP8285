// Controla el TMC5160 por SPI y genera los pulsos STEP/DIR del motor mediante Timer1.
#include "Tmc5160.h"
#include "EventLog.h"
#include "GcodePlanner.h"

#include <Arduino.h>
#include <SPI.h>
#include <math.h>

namespace
{
// Pines de selección SPI, habilitación activa en bajo y control STEP/DIR.
constexpr uint8_t chipSelectPin = D1;
constexpr uint8_t enablePin = D2;
constexpr uint8_t stepPin = D0;
constexpr uint8_t directionPin = D8;
// Direcciones de registros SPI y máscaras de configuración y diagnóstico.
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
constexpr uint32_t chopperOffTimeMask = 0x0f;
constexpr uint32_t stepDirModeMask = 1UL << 6;
// Cadencias del diagnóstico, las rampas y el límite de tiempo sin heartbeat del Jog.
constexpr unsigned long pollInterval = 1000;
constexpr uint32_t timerFrequency = 80000000; // Timer1 sin divisor.
constexpr uint32_t commandTimeoutSeconds = 3;
constexpr uint32_t keepaliveTicks = timerFrequency * commandTimeoutSeconds;
constexpr unsigned long rampInterval = 2;
constexpr uint32_t startupTick = timerFrequency / 1000;
constexpr uint32_t maxTimerTicks = 0x7FFFFF;
// Escala de corriente RMS usada para traducir miliamperios a los registros del driver.
constexpr uint32_t fullScaleRmsMicroamps = 3064129; // 0,325 V / 0,075 ohm / sqrt(2).
constexpr uint32_t currentScale = 256U * 32U;
constexpr uint16_t holdLimitMa = 400;
const SPISettings driverSpi(500000, MSBFIRST, SPI_MODE3);

// Agrupa el escalador global y los niveles de corriente de marcha y mantenimiento.
struct CurrentSettings
{
    uint16_t scaler;
    uint8_t run;
    uint8_t hold;
};

// Convierte el límite en miliamperios a un límite para el producto de las escalas.
uint32_t currentProductLimit(uint16_t milliamps)
{
    return uint64_t(milliamps) * 1000 * currentScale / fullScaleRmsMicroamps;
}

// Calcula los valores de registro que representan las corrientes solicitadas.
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

// Estado compartido entre el temporizador de pasos y el bucle principal.
volatile uint32_t ticksUntilTimeout = 0;
volatile uint32_t activeHalfPeriod = 0;
volatile uint32_t requestedHalfPeriod = 0;
volatile uint32_t firstStepTicksRemaining = 0;
volatile bool waitingForFirstStep = false;
volatile bool stepActive = false;
volatile uint8_t invertedPins = 0;
volatile bool moving = false;
volatile bool outputCommanded = false;
volatile bool jogTimedOut = false;
volatile int64_t positionUnits = 0;
volatile int32_t positionIncrement = 0;
volatile uint32_t positionPulsesLeft = 0;
volatile bool positioning = false;
bool plannedPositioning = false;
volatile bool positionReached = false;
int64_t positionTargetUnits = 0;
int64_t homePositionUnits = 0;
bool hasPositionTarget = false;
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

void IRAM_ATTR writeEnablePin(bool enabled)
{
    if (enabled == bool(invertedPins & Tmc5160::invertEnable))
    {
        GPOS = 1UL << enablePin;
    }
    else
    {
        GPOC = 1UL << enablePin;
    }
}

// D0 es GPIO16, con un registro de salida propio.
void IRAM_ATTR writeStepPin(bool active)
{
    if (active != bool(invertedPins & Tmc5160::invertStep))
    {
        GP16O |= 1U;
    }
    else
    {
        GP16O &= ~1U;
    }
}

void writeDirectionPin(bool positive)
{
    digitalWrite(directionPin, positive != bool(invertedPins & Tmc5160::invertDirection) ? HIGH : LOW);
}

// Detiene el temporizador y devuelve STEP a su nivel de reposo.
void IRAM_ATTR stopPulses(bool disableOutput)
{
    if (disableOutput)
    {
        writeEnablePin(false);
        outputCommanded = false;
    }
    timer1_disable();
    writeStepPin(false);
    stepActive = false;
    moving = false;
    positioning = false;
}

// Supervisa el plazo del Jog y alterna STEP. Aplica el nuevo período al completar un pulso.
void IRAM_ATTR onStepTimer()
{
    if (!moving)
    {
        return;
    }

    if (ticksUntilTimeout <= activeHalfPeriod)
    {
        jogTimedOut = true;
        stopPulses(true);
        return;
    }
    ticksUntilTimeout -= activeHalfPeriod;

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

    if (stepActive)
    {
        writeStepPin(false);
        // Completa el pulso final antes de detener el temporizador.
        if (positioning && positionPulsesLeft == 0)
        {
            positionReached = true;
            stopPulses(false);
            return;
        }
    }
    else
    {
        writeStepPin(true);
        positionUnits += positionIncrement;
        if (positioning)
        {
            --positionPulsesLeft;
        }
    }
    stepActive = !stepActive;
    if (!stepActive && activeHalfPeriod != requestedHalfPeriod)
    {
        activeHalfPeriod = requestedHalfPeriod;
        timer1_write(activeHalfPeriod);
    }
}

// Envía una trama SPI con un byte de dirección y cuatro bytes de datos.
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

// Hace dos transferencias porque el TMC5160 devuelve la lectura en la trama siguiente.
uint32_t readRegister(uint8_t address)
{
    SPI.beginTransaction(driverSpi);
    // La respuesta llega en la trama siguiente.
    transferRegister(address);
    const uint32_t value = transferRegister(address);
    SPI.endTransaction();
    return value;
}

// Activa el bit de escritura de la dirección antes de enviar el valor.
void writeRegister(uint8_t address, uint32_t value)
{
    SPI.beginTransaction(driverSpi);
    transferRegister(address | 0x80, value);
    SPI.endTransaction();
}

// Convierte la velocidad de pulsos por segundo al intervalo entre flancos de STEP.
uint32_t halfPeriodForSpeed(uint32_t speed)
{
    const uint32_t frequency = 2UL * speed;
    return (timerFrequency + frequency - 1) / frequency;
}

// Obtiene la velocidad de pulso actual a partir del período activo.
uint32_t pulseSpeed()
{
    noInterrupts();
    const uint32_t period = activeHalfPeriod;
    const bool pulsing = moving && !waitingForFirstStep;
    interrupts();
    return pulsing ? (timerFrequency + period) / (2UL * period) : 0;
}

// Calcula la espera inicial antes del primer paso al acelerar desde reposo.
uint32_t firstStepDelay(uint32_t speed, uint32_t durationMs)
{
    // Desde reposo, el primer impulso llega cuando la rampa ha recorrido un micropaso.
    return static_cast<uint32_t>(ceil(sqrt(2000.0 * durationMs / speed)) * startupTick);
}

// Guarda las velocidades de origen y destino y la duración de la nueva rampa.
void setSpeedRamp(uint32_t from, uint32_t to, uint32_t durationMs)
{
    rampFromSpeed = from;
    targetSpeed = to;
    rampDurationMs = durationMs;
    rampStarted = lastRampUpdate = millis();
    rampFinished = false;
}

// Cambia el sentido cuando ya no hay pulsos y arranca la rampa hacia la velocidad elegida.
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
    writeStepPin(false);
    stepActive = false;
    writeDirectionPin(requestedDirection);
    activeDirection = requestedDirection;
    positionIncrement = requestedDirection ? abs(positionIncrement) : -abs(positionIncrement);
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

// Interpola la velocidad y aplica el final de rampa, incluida la parada o la inversión.
void updateRamp(unsigned long now)
{
    if (!moving || (rampFinished && !positioning) ||
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
    uint32_t speed = rampFinished ? targetSpeed
        : (uint64_t(rampFromSpeed) * (rampDurationMs - elapsed) +
           uint64_t(targetSpeed) * elapsed) / rampDurationMs;
    if (positioning)
    {
        // v² = 2ad. Limita la velocidad en función de la distancia pendiente.
        constexpr uint32_t minTimerSpeed = (timerFrequency + 2 * maxTimerTicks - 1) / (2 * maxTimerTicks);
        if (plannedPositioning && GcodePlanner::state() == GcodePlanner::State::running)
        {
            speed = max(minTimerSpeed, GcodePlanner::profileSpeed(Tmc5160::position().units));
        }
        else
        {
            const double brakingSpeed = sqrt(2000.0 * selectedJogSpeed * positionPulsesLeft / jogDecelerationMs);
            speed = min(speed, max(minTimerSpeed, static_cast<uint32_t>(brakingSpeed)));
        }
    }
    if (speed > 0)
    {
        const uint32_t period = halfPeriodForSpeed(speed);
        if (period <= maxTimerTicks)
        {
            requestedHalfPeriod = period;
        }
    }
}

// Comprueba la identidad del chip y que acepte señales STEP/DIR.
bool validStepDir(uint32_t ioin)
{
    return (ioin >> 24) == expectedVersion && (ioin & stepDirModeMask) != 0;
}

// Combina el valor base de CHOPCONF con el campo de resolución de micropasos.
uint32_t chopperConfig()
{
    return chopperBase | (static_cast<uint32_t>(microstepResolution) << microstepResolutionShift);
}
}

// Prepara los pines con la salida deshabilitada, arranca SPI y conecta la interrupción de Timer1.
void Tmc5160::begin()
{
    pinMode(enablePin, INPUT_PULLUP);
    digitalWrite(chipSelectPin, HIGH);
    pinMode(chipSelectPin, OUTPUT);
    digitalWrite(stepPin, LOW);
    pinMode(stepPin, OUTPUT);
    digitalWrite(directionPin, LOW);
    pinMode(directionPin, OUTPUT);
    SPI.begin(); // D5 SCK, D6 MISO y D7 MOSI.
    if (validStepDir(readRegister(ioinRegister)))
    {
        writeRegister(chopperRegister, chopperConfig() & ~chopperOffTimeMask);
    }
    digitalWrite(enablePin, HIGH);
    pinMode(enablePin, OUTPUT);
    timer1_attachInterrupt(onStepTimer);
}

// Atiende la rampa, completa el ajuste silencioso y comprueba periódicamente el enlace SPI y los diagnósticos.
void Tmc5160::update()
{
    noInterrupts();
    const bool timedOut = jogTimedOut;
    jogTimedOut = false;
    const bool reached = positionReached;
    positionReached = false;
    interrupts();
    if (timedOut)
    {
        EventLog::add("Driver OFF: movimiento sin renovación (keepalive) durante 3 s.");
    }
    if (reached)
    {
        EventLog::add("Posición objetivo alcanzada.");
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
    // StallGuard requiere movimiento suficiente, salida activa y un modo de regulación compatible.
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
        const char* reason = !nextStatus.responding ? "el TMC5160 dejó de responder por SPI"
                           : !nextStatus.stepDirMode ? "el TMC5160 salió del modo STEP/DIR"
                                                     : "EN apareció alto con la salida habilitada";
        char message[96];
        snprintf(message, sizeof(message), "%s (IOIN=0x%08lX)", reason, static_cast<unsigned long>(ioin));
        disable(message);
    }
    if (hasDiagnostics &&
        (nextStatus.overtemperature != currentStatus.overtemperature ||
         nextStatus.temperatureWarning != currentStatus.temperatureWarning ||
         nextStatus.undervoltage != currentStatus.undervoltage ||
         nextStatus.driverError != currentStatus.driverError ||
         nextStatus.shortCircuit != currentStatus.shortCircuit))
    {
        EventLog::add("Diagnóstico: temperatura=%s, UV=%u, error=%u, cortocircuito=%u.",
                      nextStatus.overtemperature ? "alta" : nextStatus.temperatureWarning ? "preaviso" : "normal",
                      unsigned(nextStatus.undervoltage), unsigned(nextStatus.driverError),
                      unsigned(nextStatus.shortCircuit));
    }
    if (firstPoll || nextStatus.responding != currentStatus.responding ||
        nextStatus.outputDisabled != currentStatus.outputDisabled)
    {
        EventLog::add("TMC5160 SPI: %s (IOIN=0x%08lX).",
                      nextStatus.responding ? "identificado" : "sin identificar",
                      static_cast<unsigned long>(ioin));
        Serial.printf("TMC5160 SPI: %s | IOIN=0x%08lX\n",
                      nextStatus.responding ? "identificado" : "sin identificacion",
                      static_cast<unsigned long>(ioin));
    }
    currentStatus = nextStatus;
    firstPoll = false;
}

// Devuelve la última lectura de diagnóstico.
Tmc5160::Status Tmc5160::status()
{
    return currentStatus;
}

namespace
{
// Configura el sentido, prepara el temporizador y lanza la rampa desde reposo.
bool startMotion(bool positive, uint32_t speed, uint32_t rampMs, uint32_t decelerationMs,
                 uint32_t positionPulses = 0, bool planned = false)
{
    if (moving || (!planned && GcodePlanner::state() == GcodePlanner::State::running) ||
        !outputCommanded || quietSetupPending || speed < Tmc5160::minJogSpeed ||
        speed > Tmc5160::maxJogSpeed() || rampMs < Tmc5160::minRampMs || rampMs > Tmc5160::maxRampMs ||
        decelerationMs < Tmc5160::minRampMs || decelerationMs > Tmc5160::maxRampMs)
    {
        return false;
    }

    const uint32_t ioin = readRegister(ioinRegister);
    if (!validStepDir(ioin) || (ioin & enableInputMask) != 0 ||
        readRegister(chopperRegister) != chopperConfig() ||
        ((readRegister(globalConfigRegister) & enableQuietMask) != 0) != activeSilentMode)
    {
        Tmc5160::disable("falló la comprobación de registros al iniciar el movimiento");
        return false;
    }

    writeDirectionPin(positive);
    activeDirection = requestedDirection = positive;
    reversing = false;
    selectedJogSpeed = speed;
    jogRampMs = rampMs;
    jogDecelerationMs = decelerationMs;
    const int32_t increment = Tmc5160::positionUnitsPerStep / Tmc5160::microsteps();
    positionIncrement = positive ? increment : -increment;
    positionPulsesLeft = positionPulses;
    positioning = positionPulses != 0;
    plannedPositioning = planned;
    setSpeedRamp(0, speed, rampMs);
    ticksUntilTimeout = keepaliveTicks;
    waitingForFirstStep = true;
    firstStepTicksRemaining = firstStepDelay(speed, rampMs);
    activeHalfPeriod = startupTick;
    requestedHalfPeriod = 0;
    moving = true;
    timer1_enable(TIM_DIV1, TIM_EDGE, TIM_LOOP);
    timer1_write(activeHalfPeriod);
    return true;
}
}

namespace
{
// Configura los registros de corriente y regulación y comprueba la habilitación de la salida.
bool enableOutput(const CurrentSettings& current, bool quiet)
{
    const uint32_t ioin = readRegister(ioinRegister);
    if (!validStepDir(ioin))
    {
        Tmc5160::disable("TMC5160 sin respuesta SPI o fuera de modo STEP/DIR al habilitar");
        return false;
    }
    if (outputCommanded)
    {
        if ((ioin & enableInputMask) == 0)
        {
            return true;
        }
        Tmc5160::disable("EN alto durante la comprobación de habilitación");
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

    writeEnablePin(true);
    const uint32_t enabledIoin = readRegister(ioinRegister);
    if (!validStepDir(enabledIoin) || (enabledIoin & enableInputMask) != 0)
    {
        Tmc5160::disable("falló la lectura de confirmación al habilitar");
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

// Aplica la configuración seleccionada y habilita la salida si el driver está preparado.
bool Tmc5160::enable()
{
    return enableOutput(selectedCurrent, selectedSilentMode);
}

// Inicia el movimiento manual y guarda la velocidad y los tiempos de rampa para ajustes posteriores.
bool Tmc5160::startJog(bool positive, uint32_t speed, uint32_t rampMs, uint32_t decelerationMs)
{
    return startMotion(positive, speed, rampMs, decelerationMs);
}

Tmc5160::Position Tmc5160::position()
{
    noInterrupts();
    const Position result{positionUnits, positionTargetUnits, homePositionUnits, hasPositionTarget, positioning};
    interrupts();
    return result;
}

int32_t Tmc5160::signedPulseSpeed()
{
    const auto speed = static_cast<int32_t>(pulseSpeed());
    return activeDirection ? speed : -speed;
}

namespace
{
bool moveToUnits(int64_t goal, uint32_t speed, uint32_t rampMs, uint32_t decelerationMs, bool planned = false)
{
    if (moving || (!planned && GcodePlanner::state() == GcodePlanner::State::running) ||
        !outputCommanded || quietSetupPending)
    {
        return false;
    }
    const int64_t delta = goal - Tmc5160::position().units;
    const int64_t increment = Tmc5160::positionUnitsPerStep / Tmc5160::microsteps();
    const uint64_t pulses = (delta < 0 ? -delta : delta) / increment;
    if (pulses > UINT32_MAX || delta % increment != 0 ||
        (pulses != 0 && !startMotion(delta > 0, speed, rampMs, decelerationMs, static_cast<uint32_t>(pulses), planned)))
    {
        return false;
    }
    positionTargetUnits = goal;
    hasPositionTarget = true;
    return true;
}
}

bool Tmc5160::moveToPosition(int32_t steps, uint32_t speed, uint32_t rampMs, uint32_t decelerationMs)
{
    return steps >= -maxPositionSteps && steps <= maxPositionSteps &&
           moveToUnits(int64_t(steps) * positionUnitsPerStep, speed, rampMs, decelerationMs);
}

bool Tmc5160::movePlanned(int32_t steps, uint32_t speed, uint32_t rampMs, uint32_t decelerationMs)
{
    return steps >= -maxPositionSteps && steps <= maxPositionSteps &&
           moveToUnits(int64_t(steps) * positionUnitsPerStep, speed, rampMs, decelerationMs, true);
}

bool Tmc5160::moveHome(uint32_t speed, uint32_t rampMs, uint32_t decelerationMs)
{
    return moveToUnits(homePositionUnits, speed, rampMs, decelerationMs);
}

bool Tmc5160::setHomePosition()
{
    if (moving)
    {
        return false;
    }
    homePositionUnits = position().units;
    return true;
}

namespace
{
// Recalcula la rampa desde la velocidad efectiva hacia el nuevo objetivo.
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

// Valida la nueva velocidad y la guarda incluso si hay una inversión en curso.
bool Tmc5160::changeJogSpeed(uint32_t speed)
{
    if (!moving || positioning || isStopping() || speed < minJogSpeed || speed > maxJogSpeed())
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

// Decelera antes de invertir el sentido y permite volver al sentido activo durante la maniobra.
bool Tmc5160::changeJogDirection(bool positive)
{
    if (!moving || positioning || isStopping())
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

// Renueva el plazo de seguridad con la interrupción momentáneamente desactivada.
bool Tmc5160::renewJog()
{
    noInterrupts();
    const bool renewed = moving;
    if (renewed)
    {
        ticksUntilTimeout = keepaliveTicks;
    }
    interrupts();
    return renewed;
}

// Consultas de estado que alimentan el panel y la lógica de control.
bool Tmc5160::isMoving()
{
    return moving;
}

bool Tmc5160::isDecelerating()
{
    if (moving && plannedPositioning && GcodePlanner::state() == GcodePlanner::State::running)
    {
        return !waitingForFirstStep && requestedHalfPeriod > activeHalfPeriod;
    }
    return moving && !rampFinished && targetSpeed < rampFromSpeed;
}

bool Tmc5160::isStopping()
{
    return moving && targetSpeed == 0 && !reversing;
}

bool Tmc5160::isReversing()
{
    return moving && reversing;
}

bool Tmc5160::isEnabled()
{
    return outputCommanded;
}


// Resolución seleccionada y conversiones entre RPM, micropasos y límites de aceleración.
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

// Valida y selecciona una resolución mientras la salida está deshabilitada.
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
    if (resolution != microstepResolution)
    {
        positionUnits = 0;
        homePositionUnits = 0;
        hasPositionTarget = false;
        microstepResolution = resolution;
    }
    return true;
}

uint8_t Tmc5160::pinInversions()
{
    return invertedPins;
}

bool Tmc5160::selectPinInversions(uint8_t mask)
{
    if (outputCommanded || moving || (mask & ~allPinInversions) != 0)
    {
        return false;
    }

    const uint8_t previous = invertedPins;
    const uint8_t changed = previous ^ mask;
    if (changed & invertEnable)
    {
        if (!validStepDir(readRegister(ioinRegister)))
        {
            return false;
        }
        // Antes de probar el nivel inactivo de EN, apaga las etapas de potencia por SPI.
        const uint32_t disabledChopper = chopperConfig() & ~chopperOffTimeMask;
        writeRegister(chopperRegister, disabledChopper);
        if (readRegister(chopperRegister) != disabledChopper)
        {
            return false;
        }
        invertedPins = mask;
        writeEnablePin(false);
        const uint32_t ioin = readRegister(ioinRegister);
        if (!validStepDir(ioin) || (ioin & enableInputMask) == 0)
        {
            invertedPins = previous;
            writeEnablePin(false);
            return false;
        }
    }
    invertedPins = mask;
    writeStepPin(false);
    writeDirectionPin(activeDirection);
    if (changed & (invertDirection | invertStep))
    {
        positionUnits = 0;
        homePositionUnits = 0;
        hasPositionTarget = false;
    }
    return true;
}

// Valores de corriente RMS que se muestran en el panel.
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

// Valida y guarda el ajuste de corriente de marcha.
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

// Consultas del modo solicitado y del progreso del ajuste de StealthChop.
bool Tmc5160::silentMode()
{
    return selectedSilentMode;
}

bool Tmc5160::silentInitializing()
{
    return outputCommanded && quietSetupPending;
}

// Selecciona el modo de regulación con la salida deshabilitada.
bool Tmc5160::selectSilentMode(bool enabled)
{
    if (outputCommanded || moving)
    {
        return false;
    }
    selectedSilentMode = enabled;
    return true;
}

// Cancela la maniobra en curso y decelera hasta reposo manteniendo habilitada la salida.
void Tmc5160::stop()
{
    GcodePlanner::cancel();
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
    if (speed > 0)
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

// Deshabilita la salida y cancela la generación de pasos.
void Tmc5160::disable(const char* reason)
{
    GcodePlanner::cancel();
    noInterrupts();
    const bool wasEnabled = outputCommanded;
    stopPulses(true);
    interrupts();
    if (wasEnabled)
    {
        EventLog::add("Driver OFF: %s.", reason);
    }
}
