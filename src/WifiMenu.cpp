// Gestiona la configuración Wi-Fi desde el monitor serie.
#include "WifiMenu.h"

#include <ESP8266WiFi.h>

#include "EventLog.h"
#include "Tmc5160.h"
#include "WifiConnection.h"
#include "WifiStorage.h"

namespace
{
// Distingue si se espera un número de red, una clave o ningún dato del menú.
enum class State { Inactive, SelectNetwork, EnterPassword };

State state = State::Inactive;
int networkCount = 0;
int selectedNetwork = 0;
String serialInput;
bool inputTooLong = false;
bool previousWasCr = false;

// Entrega a la conexión la red elegida y libera la lista del escaneo.
void connectToNetwork(const char* password)
{
    const String ssid = WiFi.SSID(selectedNetwork);
    WifiConnection::connect(ssid.c_str(), password, WiFi.channel(selectedNetwork),
                            WiFi.BSSID(selectedNetwork));
    WiFi.scanDelete();
    state = State::Inactive;
}

// Interpreta la línea según el estado del menú y valida el número de red antes de usarlo.
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

// Preasigna memoria para la contraseña.
void WifiMenu::begin()
{
    serialInput.reserve(WifiCredentials::maxPasswordLength);
}

// Deja el motor deshabilitado durante el escaneo y muestra señal, canal y tipo de red.
void WifiMenu::showNetworks()
{
    if (WiFi.scanComplete() == WIFI_SCAN_RUNNING)
    {
        Serial.println("Hay una busqueda Wi-Fi en curso. Envia R cuando termine.");
        return;
    }
    Tmc5160::disable("menú Wi-Fi por puerto serie");
    WifiConnection::disconnect();
    EventLog::add("Buscando redes Wi-Fi desde el menú serie.");
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

// Limita la longitud y trata CRLF como un único final de línea.
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
