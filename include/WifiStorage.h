#pragma once

#include <stddef.h>

struct WifiCredentials
{
    static constexpr size_t maxPasswordLength = 64;
    char ssid[33];
    char password[maxPasswordLength + 1];
};

namespace WifiStorage
{
bool begin();
bool load(WifiCredentials& credentials);
bool save(const WifiCredentials& credentials);
}
