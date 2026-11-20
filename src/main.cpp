// Punto de entrada del firmware. Coordina el motor, el menú serie y el servicio web.
#include <Arduino.h>

#include "MotorWebServer.h"
#include "Tmc5160.h"
#include "WifiConnection.h"
#include "WifiMenu.h"

// Inicializa el driver y el puerto serie. Si no hay una red guardada, abre el menú de selección.
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

// Atiende las rampas y el diagnóstico, procesa el puerto serie y sirve el panel mientras haya Wi-Fi.
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
