# MotorPAP · ESP8285 y TMC5160

Firmware para controlar un motor paso a paso desde un navegador en la red local. El ESP8285 configura y consulta el TMC5160 por SPI y genera los pulsos `STEP` con el temporizador Timer1. El panel web permite mover el motor en ambos sentidos, ajustar la velocidad durante el movimiento y consultar el estado del driver.

## Conexiones

Los nombres `D0` a `D8` corresponden al entorno `d1_mini_pro` de PlatformIO. Si tu placa usa números GPIO, sigue la columna central.

| Pin | GPIO | Señal del TMC5160 |
| --- | --- | --- |
| D0 | 16 | `STEP`, pulsos de movimiento |
| D1 | 5 | `CS`, selección SPI activa en bajo |
| D2 | 4 | `EN`, habilitación activa en bajo |
| D5 | 14 | `SCK`, reloj SPI |
| D6 | 12 | `MISO`, datos del driver al ESP |
| D7 | 13 | `MOSI`, datos del ESP al driver |
| D8 | 15 | `DIR`, sentido de giro |
| GND | — | Masa común |

Configura el módulo TMC5160 para aceptar señales externas `STEP/DIR` y habilitar la comunicación SPI. El firmware usa SPI en modo 3 a 500 kHz. Alimenta el motor a través del driver según las especificaciones del módulo y del motor. El cálculo de corriente del firmware presupone resistencias de sensado de **0,075 Ω** por fase. Comprueba ese valor en tu módulo antes de habilitarlo.

## Compilar y cargar

El proyecto usa PlatformIO con el framework Arduino para ESP8266. El entorno `d1_mini_pro` usa el script `eagle.flash.1m128.ld` para una flash de 1 MB con espacio reservado para el sistema de archivos. `platformio.ini` configura `COM7` para la carga y el monitor serie, ambos a 115200 baudios. Ajusta los puertos si tu adaptador aparece con otro nombre.

Desde la raíz del proyecto, con PlatformIO Core disponible en la terminal:

```sh
pio run -e d1_mini_pro
pio run -e d1_mini_pro -t upload
pio device monitor -e d1_mini_pro
```

## Conectar al Wi-Fi y abrir el panel

1. Abre el monitor serie a **115200 baudios**, con envío de fin de línea, y reinicia la placa.
2. Si todavía necesita configurar una red, el ESP8285 lista las redes de **2,4 GHz** que detecta. Envía el número de la red y pulsa Enter. Para una red protegida, introduce la clave cuando se solicite.
3. Cuando se conecta, el firmware muestra una dirección como `http://192.168.1.42/`. Ábrela desde un dispositivo en la misma red.
4. Para volver a buscar redes, envía `R` por el monitor serie cuando no estés introduciendo una clave. Al abrir el menú, el firmware deshabilita el driver.

Las credenciales se guardan en LittleFS después de establecer la conexión y se reutilizan al arrancar. Si la conexión o la reconexión falla durante 25 segundos, vuelve a aparecer el menú de redes.

## Mover el motor

1. Con el driver en **OFF**, selecciona la resolución de micropasos, configura la corriente de marcha y elige si quieres activar **StealthChop2**.
2. Pon el driver en **ON**. El firmware comprueba por SPI que el TMC5160 responde, que está en modo `STEP/DIR` y que se puede habilitar. Con StealthChop2 activado, espera a que termine el ajuste inicial antes de mover el motor.
3. Ajusta la velocidad y los tiempos de aceleración y deceleración. Pulsa **Mover +** o **Mover −** para iniciar el giro continuo (jog). **Mover +** sigue el sentido seleccionado en el panel y **Mover −** usa el opuesto. Puedes cambiar de velocidad durante el giro. Si inviertes el sentido, el firmware frena hasta detenerse y acelera en la dirección nueva.
4. Pulsa **STOP** para frenar con la rampa configurada y mantener el driver habilitado. Cambia a **OFF** para cortar los pulsos y deshabilitar la salida inmediatamente.

El panel renueva periódicamente el permiso de movimiento. Si el firmware pasa unos **3 segundos** sin recibir una renovación durante un jog, corta los pulsos y deshabilita el driver. También deshabilita el driver al detectar que ha perdido la conexión Wi-Fi mientras el servidor está activo.

| Ajuste | Valores |
| --- | --- |
| Micropasos por paso | De 1 a 256 en potencias de dos. Valor inicial de 16. |
| Velocidad | Desde 100 micropasos/s. Límite de 550 RPM hasta 16 micropasos por paso y de 100 RPM con resoluciones superiores. |
| Aceleración y deceleración | Cada rampa admite de 0,10 a 600 s. Valor inicial de 0,20 s. |
| Corriente de marcha | De 0,10 a 1,20 A RMS en incrementos de 0,01 A. Valor inicial de 0,80 A RMS. |
| Corriente de mantenimiento | Calculada a partir del ajuste de marcha, con un límite de 0,40 A RMS. |

La conversión a RPM supone un motor de **200 pasos completos por vuelta**. Los ajustes del motor se restablecen al reiniciar el ESP. Las funciones disponibles de movimiento son el jog y la parada. Los controles de inicio, posicionamiento y gráfica están reservados en la interfaz.

## Diagnóstico

- El panel muestra si el TMC5160 responde, el estado de `EN`, la temperatura, la subtensión, los cortocircuitos y los fallos del driver. La lectura de StallGuard está disponible a partir de 10 RPM, con el driver habilitado, en movimiento y fuera de StealthChop.
- El monitor serie informa de la identificación SPI y muestra el registro `IOIN`. Si el panel indica **Sin identificar**, revisa `CS`, el cableado SPI y la alimentación del módulo. Si el driver aparece detectado pero no se habilita, comprueba `EN` y el modo `STEP/DIR`.

## Organización del código

| Archivo | Función |
| --- | --- |
| [`src/main.cpp`](src/main.cpp) | Inicialización y bucle principal. |
| [`src/Tmc5160.cpp`](src/Tmc5160.cpp) | Configuración SPI, diagnóstico, corriente y generación de pulsos y rampas. |
| [`src/MotorWebServer.cpp`](src/MotorWebServer.cpp) | Servidor HTTP en el puerto 80 y rutas `/api/` para consultar y controlar el motor. |
| [`include/MotorPanel.h`](include/MotorPanel.h) | HTML, CSS y JavaScript del panel integrado en el firmware. |
| [`src/WifiMenu.cpp`](src/WifiMenu.cpp) | Escaneo y selección de redes desde el puerto serie. |
| [`src/WifiConnection.cpp`](src/WifiConnection.cpp) | Conexión Wi-Fi en modo estación y gestión de la reconexión. |
| [`src/WifiStorage.cpp`](src/WifiStorage.cpp) | Lectura y escritura de las credenciales en LittleFS. |
