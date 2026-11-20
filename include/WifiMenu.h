// Interfaz del menú serie que permite elegir una red Wi-Fi y escribir su contraseña.
#pragma once

namespace WifiMenu
{
// Reserva el espacio de la línea de entrada.
void begin();
// Deshabilita el motor, desconecta la red actual y lista las redes detectadas.
void showNetworks();
// Recoge los caracteres disponibles y procesa las líneas completas.
void update();
}
