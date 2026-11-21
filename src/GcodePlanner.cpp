#include "GcodePlanner.h"
#include "Tmc5160.h"

#include <Arduino.h>
#include <cmath>
#include <new>

namespace GcodePlanner
{
namespace
{
struct Move
{
    int32_t targetUnits;
    uint32_t speed;
    uint32_t entrySpeed;
    uint16_t line;
};

Move* moves = nullptr;
unsigned capacity = 0;
unsigned count = 0;
unsigned current = 0;
unsigned groupEnd = 0;
unsigned currentLine = 0;
int64_t groupStart = 0;
int64_t segmentStart = 0;
int direction = 0;
uint32_t ramp = 0;
uint32_t deceleration = 0;
uint32_t uploadRevision = 0;
unsigned long lastActivity = 0;
State programState = State::idle;

int sign(int64_t delta)
{
    return (delta > 0) - (delta < 0);
}

double pulses(int64_t distance)
{
    return double(distance) * Tmc5160::microsteps() / Tmc5160::positionUnitsPerStep;
}

double reachable(uint32_t boundarySpeed, uint32_t speed, double distance, uint32_t time)
{
    return std::sqrt(double(boundarySpeed) * boundarySpeed + 2000.0 * speed * distance / time);
}

void finish(State result)
{
    programState = result;
    discard();
}

void startGroup(int64_t position)
{
    groupStart = segmentStart = position;
    direction = current < count ? sign(moves[current].targetUnits - position) : 0;
    if (direction == 0)
    {
        finish(State::complete);
        return;
    }

    groupEnd = current + 1;
    while (groupEnd < count && sign(int64_t(moves[groupEnd].targetUnits) -
                                   moves[groupEnd - 1].targetUnits) == direction)
    {
        ++groupEnd;
    }

    // Calcula las velocidades de entrada que permiten frenar antes de la próxima
    // reducción de avance, inversión de sentido o fin de programa.
    uint32_t exitSpeed = 0;
    for (unsigned i = groupEnd; i-- > current;)
    {
        auto& move = moves[i];
        const int64_t origin = i == current ? groupStart : moves[i - 1].targetUnits;
        move.entrySpeed = min(double(move.speed),
            reachable(exitSpeed, move.speed, pulses(direction * (move.targetUnits - origin)), deceleration));
        exitSpeed = move.entrySpeed;
    }
    moves[current].entrySpeed = 0;
    for (unsigned i = current + 1; i < groupEnd; ++i)
    {
        const auto& previous = moves[i - 1];
        const int64_t origin = i == current + 1 ? groupStart : moves[i - 2].targetUnits;
        moves[i].entrySpeed = min(double(moves[i].entrySpeed), min(double(previous.speed),
            reachable(previous.entrySpeed, previous.speed,
                      pulses(direction * (previous.targetUnits - origin)), ramp)));
    }

    currentLine = moves[current].line;
    if (!Tmc5160::movePlanned(moves[groupEnd - 1].targetUnits / Tmc5160::positionUnitsPerStep,
                              moves[current].speed, ramp, deceleration))
    {
        finish(State::interrupted);
    }
}
}

bool prepare(unsigned size, uint32_t revision)
{
    if (programState == State::running || size == 0 || size > maxMoves) return false;
    discard();
    moves = new (std::nothrow) Move[size];
    if (!moves) return false;
    capacity = size;
    uploadRevision = revision;
    lastActivity = millis();
    return true;
}

bool append(int32_t steps, uint32_t speed, uint16_t sourceLine)
{
    if (programState == State::running || count >= capacity ||
        steps < -Tmc5160::maxPositionSteps || steps > Tmc5160::maxPositionSteps ||
        speed < Tmc5160::minJogSpeed || speed > Tmc5160::maxJogSpeed() ||
        sourceLine == 0 || (count && sourceLine <= moves[count - 1].line))
    {
        return false;
    }
    moves[count++] = {steps * Tmc5160::positionUnitsPerStep, speed, 0, sourceLine};
    lastActivity = millis();
    return true;
}

unsigned loaded() { return count; }

bool start(uint32_t revision, uint32_t rampMs, uint32_t decelerationMs)
{
    const auto driver = Tmc5160::status();
    if (programState == State::running || count == 0 || count != capacity || revision != uploadRevision ||
        !Tmc5160::isEnabled() || Tmc5160::isMoving() || Tmc5160::silentInitializing() ||
        !driver.responding || driver.overtemperature || driver.driverError || driver.shortCircuit ||
        rampMs < Tmc5160::minRampMs || rampMs > Tmc5160::maxRampMs ||
        decelerationMs < Tmc5160::minRampMs || decelerationMs > Tmc5160::maxRampMs)
    {
        return false;
    }

    const int64_t position = Tmc5160::position().units;
    const uint32_t maxSpeed = Tmc5160::maxJogSpeed();
    for (unsigned i = 0; i < count; ++i)
    {
        if (moves[i].speed > maxSpeed) return false;
    }

    unsigned used = 0;
    int64_t previous = position;
    for (unsigned i = 0; i < count; ++i)
    {
        if (moves[i].targetUnits != previous)
        {
            previous = moves[i].targetUnits;
            moves[used++] = moves[i];
        }
    }

    count = used;
    current = 0;
    currentLine = 0;
    ramp = rampMs;
    deceleration = decelerationMs;
    lastActivity = millis();
    programState = State::running;
    startGroup(position);
    return programState != State::interrupted;
}

void discard()
{
    if (programState == State::running) return;
    delete[] moves;
    moves = nullptr;
    capacity = count = 0;
}

void cancel()
{
    if (programState == State::running) finish(State::interrupted);
    else discard();
}

void update()
{
    if (programState != State::running)
    {
        if (moves && millis() - lastActivity >= 10000) discard();
        return;
    }
    const auto driver = Tmc5160::status();
    if (!Tmc5160::isEnabled() || !driver.responding || driver.overtemperature ||
        driver.driverError || driver.shortCircuit || millis() - lastActivity >= 3000)
    {
        Tmc5160::disable("programa G interrumpido por diagnóstico o falta de renovación");
        return;
    }
    if (Tmc5160::isMoving()) return;

    const auto position = Tmc5160::position().units;
    if (position != moves[groupEnd - 1].targetUnits)
    {
        finish(State::interrupted);
        return;
    }
    current = groupEnd;
    startGroup(position);
}

bool renew()
{
    if (programState != State::running || millis() - lastActivity >= 3000) return false;
    lastActivity = millis();
    Tmc5160::renewJog();
    return true;
}

State state() { return programState; }
unsigned line() { return currentLine; }

uint32_t profileSpeed(int64_t positionUnits)
{
    while (current + 1 < groupEnd && direction * (positionUnits - moves[current].targetUnits) >= 0)
    {
        segmentStart = moves[current++].targetUnits;
    }
    const auto& move = moves[current];
    currentLine = move.line;
    const double travelled = pulses(direction * (positionUnits - segmentStart));
    const double remaining = max(0.0, pulses(direction * (move.targetUnits - positionUnits)));
    const uint32_t exitSpeed = current + 1 < groupEnd ? moves[current + 1].entrySpeed : 0;
    const double accelerating = reachable(move.entrySpeed, move.speed, max(1.0, travelled), ramp);
    const double braking = reachable(exitSpeed, move.speed, remaining, deceleration);
    return min(double(move.speed), min(accelerating, braking));
}
}
