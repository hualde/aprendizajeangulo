# ESP32-C3 Control CAN

Este proyecto implementa un servidor web en un ESP32-C3 para controlar la comunicación CAN (Controller Area Network). Está diseñado para interactuar con un sistema de dirección de un vehículo.

## Características

- Configura el ESP32-C3 como un punto de acceso WiFi.
- Implementa un servidor web simple con una interfaz de usuario.
- Envía tramas CAN específicas basadas en la interacción del usuario.
- Incluye una cuenta regresiva de 30 segundos para la calibración del volante.

## Requisitos de Hardware

- Placa ESP32-C3
- Transceptor CAN compatible

## Configuración

1. Conecte el transceptor CAN a los pines GPIO 18 (TX) y 19 (RX) del ESP32-C3.
2. Compile y cargue el código en su ESP32-C3 utilizando el ESP-IDF.

## Uso

1. El ESP32-C3 creará un punto de acceso WiFi llamado "ESP32_CAN_AP" (sin contraseña).
2. Conéctese a esta red WiFi con su dispositivo.
3. Abra un navegador web y vaya a la dirección IP del ESP32-C3 (generalmente 192.168.4.1).
4. Siga las instrucciones en la página web para calibrar el sistema de dirección.

## Estructura del Proyecto

- `main/main.c`: Punto de entrada principal, configura el TWAI (CAN) y inicia el servidor web.
- `main/web_server.c`: Implementa el servidor web y la lógica de control CAN.
- `main/web_server.h`: Declaraciones de funciones para el servidor web.

## Notas

- Este proyecto está configurado para una velocidad de CAN de 500 kbit/s.
- Asegúrese de que su vehículo sea compatible con las tramas CAN enviadas por este dispositivo.

## Advertencia

Este proyecto interactúa directamente con el sistema de dirección del vehículo. Úselo bajo su propio riesgo y solo si entiende completamente las implicaciones y los riesgos asociados.