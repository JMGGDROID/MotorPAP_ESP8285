#include "WifiMenu.h"

#include <ESP8266WiFi.h>

#include "Tmc5160.h"
#include "WifiConnection.h"
#include "WifiStorage.h"

namespace
{
enum class State { Inactive, SelectNetwork, EnterPassword };

State state = State::Inactive;
int networkCount = 0;
int selectedNetwork = 0;
String serialInput;
bool inputTooLong = false;
bool previousWasCr = false;

void connectToNetwork(const char* password)
{
    const String ssid = WiFi.SSID(selectedNetwork);
    WifiConnection::connect(ssid.c_str(), password, WiFi.channel(selectedNetwork),
                            WiFi.BSSID(selectedNetwork));
    WiFi.scanDelete();
    state = State::Inactive;
}

void handleSerialLine()
{
    if (inputTooLong)
    {
        Serial.println("Entrada demasiado larga. Vuelve a intentarlo.");
        return;
    }

    if (state == State::EnterPassword)
    {
        if (serialInput.isEmpty())
        {
            state = State::SelectNetwork;
            Serial.println("Seleccion cancelada. Envia un numero de la lista, o R para volver a buscar.");
            return;
        }
        connectToNetwork(serialInput.c_str());
        return;
    }

    serialInput.trim();
    if (state == State::Inactive && serialInput.equalsIgnoreCase("PRUEBA08"))
    {
        Tmc5160::testMotor();
        return;
    }
    if (serialInput.equalsIgnoreCase("r"))
    {
        WifiMenu::showNetworks();
        return;
    }
    if (state != State::SelectNetwork || serialInput.isEmpty())
    {
        return;
    }

    char* end = nullptr;
    const long choice = strtol(serialInput.c_str(), &end, 10);
    if (*end != '\0' || choice < 1 || choice > networkCount)
    {
        Serial.println("Envia un numero de la lista, o R para volver a buscar.");
        return;
    }

    selectedNetwork = static_cast<int>(choice - 1);
    if (WiFi.encryptionType(selectedNetwork) == ENC_TYPE_NONE)
    {
        connectToNetwork("");
        return;
    }

    state = State::EnterPassword;
    Serial.print("Introduce la clave de ");
    Serial.print(WiFi.SSID(selectedNetwork));
    Serial.println(" y pulsa Enter. Pulsa solo Enter para volver a la lista.");
}
}

void WifiMenu::begin()
{
    serialInput.reserve(WifiCredentials::maxPasswordLength);
}

void WifiMenu::showNetworks()
{
    Tmc5160::disable();
    WifiConnection::disconnect();
    state = State::SelectNetwork;
    Serial.println("\nBuscando redes Wi-Fi de 2,4 GHz...");
    networkCount = WiFi.scanNetworks();

    if (networkCount <= 0)
    {
        Serial.println("No se encontraron redes. Envia R para volver a buscar.");
        return;
    }

    for (int index = 0; index < networkCount; ++index)
    {
        Serial.printf("%2d. %s | %ld dBm | canal %ld | %s\n",
                      index + 1, WiFi.SSID(index).c_str(),
                      static_cast<long>(WiFi.RSSI(index)),
                      static_cast<long>(WiFi.channel(index)),
                      WiFi.encryptionType(index) == ENC_TYPE_NONE ? "Abierta" : "Protegida");
    }
    Serial.println("Envia el numero de la red y pulsa Enter, o R para volver a buscar.");
}

void WifiMenu::update()
{
    while (Serial.available() > 0)
    {
        const char character = static_cast<char>(Serial.read());
        if (character == '\r' || character == '\n')
        {
            if (character == '\r' || !previousWasCr)
            {
                handleSerialLine();
                serialInput = "";
                inputTooLong = false;
            }
            previousWasCr = character == '\r';
            continue;
        }

        previousWasCr = false;
        if (serialInput.length() < WifiCredentials::maxPasswordLength)
        {
            serialInput += character;
        }
        else
        {
            inputTooLong = true;
        }
    }
}
