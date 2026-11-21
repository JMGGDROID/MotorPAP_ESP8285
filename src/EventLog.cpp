#include "EventLog.h"

#include <core_esp8266_features.h>
#include <stdarg.h>

namespace
{
constexpr size_t capacity = 48;
struct Entry
{
    uint64_t atMs;
    char message[112];
};

Entry entries[capacity];
size_t nextEntry = 0;
size_t entryCount = 0;
}

void EventLog::add(const char* format, ...)
{
    Entry& entry = entries[nextEntry];
    entry.atMs = micros64() / 1000;
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(entry.message, sizeof(entry.message), format, arguments);
    va_end(arguments);
    nextEntry = (nextEntry + 1) % capacity;
    if (entryCount < capacity)
    {
        ++entryCount;
    }
}

bool EventLog::snapshot(String& output)
{
    if (!output.reserve(64 + entryCount * (sizeof(entries[0].message) + 20)))
    {
        return false;
    }
    output = "";
    const uint64_t nowMs = micros64() / 1000;
    const size_t oldestEntry = (nextEntry + capacity - entryCount) % capacity;
    for (size_t offset = 0; offset < entryCount; ++offset)
    {
        const Entry& entry = entries[(oldestEntry + offset) % capacity];
        // El navegador resta la antigüedad a su reloj y aplica su zona horaria.
        char age[20];
        snprintf(age, sizeof(age), "%llu\t", static_cast<unsigned long long>(nowMs - entry.atMs));
        output += age;
        output += entry.message;
        output += '\n';
    }
    return true;
}
