// Interfaz del servidor HTTP que aloja el panel y recibe las órdenes del motor.
#pragma once

namespace MotorWebServer
{
// Registra las rutas una vez, antes de que la placa se conecte a la red.
void begin();
// Inicia o detiene el servidor según el estado de la red y atiende las peticiones pendientes.
void update(bool wifiOnline);
}
