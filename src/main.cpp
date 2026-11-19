#include <Arduino.h>

#include "MotorWebServer.h"
#include "Tmc5160.h"
#include "WifiConnection.h"
#include "WifiMenu.h"

void setup()
{
    Tmc5160::begin();
    Serial.begin(115200);
    Serial.println("\nMotorPAP - ESP8285 iniciado");
    Tmc5160::update();

    WifiMenu::begin();
    MotorWebServer::begin();
    if (!WifiConnection::begin())
    {
        WifiMenu::showNetworks();
    }
}

void loop()
{
    Tmc5160::update();
    WifiMenu::update();
    if (WifiConnection::update())
    {
        WifiMenu::showNetworks();
    }
    MotorWebServer::update(WifiConnection::isOnline());
}
