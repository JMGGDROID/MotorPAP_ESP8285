#pragma once

#include <stdint.h>

namespace GcodePlanner
{
constexpr unsigned maxMoves = 1000;
enum class State { idle, running, complete, interrupted };

bool prepare(unsigned count, uint32_t revision);
bool append(int32_t steps, uint32_t speed, uint16_t line);
unsigned loaded();
bool start(uint32_t revision, uint32_t rampMs, uint32_t decelerationMs);
void discard();
void cancel();
void update();
bool renew();
State state();
unsigned line();
uint32_t profileSpeed(int64_t positionUnits);
}
