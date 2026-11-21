// Interfaz del módulo que establece y supervisa la conexión Wi-Fi en modo estación.
#pragma once

#include "WifiStorage.h"

namespace WifiConnection
{
// Intenta conectar con las credenciales guardadas y devuelve si pudo iniciar el intento.
bool begin();
// Cancela la conexión actual antes de seleccionar otra red.
void disconnect();
// Inicia una conexión nueva. El canal y el BSSID identifican el punto de acceso elegido.
void connect(const char* ssid, const char* password, int32_t channel, const uint8_t* bssid);
// Programa el cambio después de enviar la respuesta web y conserva la red anterior para recuperarla.
bool scheduleChange(WifiMode mode, const char* ssid, const char* password);
WifiAddress address(WifiMode mode);
bool setAddress(WifiMode mode, const WifiAddress& address);
bool isChanging();
bool isAccessPoint();
// Devuelve true si vence el plazo y hay que mostrar el selector.
// Avanza la gestión de la conexión. Devuelve true si se agotó el plazo y debe mostrarse el menú.
bool update();
// Indica si el módulo considera activa la conexión.
bool isOnline();
}
