#pragma once

#include <Arduino.h>

// Historial circular de diagnóstico en RAM. Se usa desde el bucle principal.
namespace EventLog
{
void add(const char* format, ...) __attribute__((format(printf, 1, 2)));
bool snapshot(String& output);
}
