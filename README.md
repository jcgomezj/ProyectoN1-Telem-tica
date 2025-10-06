# ProyectoN1-Telematica

## Introducción

Este proyecto implementa una arquitectura **Cliente-Servidor** basada en el protocolo **CoAP (Constrained Application Protocol)**, utilizando **sockets Berkeley (UDP)**.  
El servidor fue desarrollado completamente en **C**, mientras que el cliente se implementó en **Python**, permitiendo la comunicación con sensores simulados (como el **ESP32 en Wokwi**) o físicos, siguiendo las especificaciones del estándar **RFC 7252**.
Este proyecto implementa un **cliente CoAP** (Constrained Application Protocol) ejecutado en un **ESP32 simulado en Wokwi**, que envía lecturas reales de **temperatura y humedad** obtenidas de un sensor **DHT22 virtual** hacia un servidor CoAP desplegado en **AWS EC2**. 

---

## Desarrollo e implementación

### Servidor (C)

- Desarrollado con **sockets UDP** de la API Berkeley, sin uso de clases ni librerías externas.  
- Soporta los métodos CoAP:
  - `GET`: consulta valores de sensores  
  - `POST`: almacena nuevos registros  
  - `PUT`: actualiza registros existentes  
  - `DELETE`: elimina recursos  

- Implementa los tipos de mensaje: `CON`, `NON`, `ACK`, `RST`.  
- Incluye un **logger** (en consola y archivo `server.log`) para registrar cada solicitud entrante, el tipo de mensaje, el contenido y la respuesta.  
- Los datos se almacenan temporalmente en memoria como pares **clave-valor (key-value)**, donde cada recurso representa un sensor.

Durante el desarrollo se tomaron las siguientes decisiones:

Se priorizó la modularidad y compatibilidad multiplataforma (Windows y Linux).

Se implementó un parser de opciones CoAP personalizado, capaz de extraer el URI-Path y manejar los códigos de respuesta estándar.

Se añadieron threads (POSIX) para permitir la atención concurrente de múltiples clientes.

El logger fue diseñado para registrar eventos con marca de tiempo, simplificando la depuración.

La comunicación con el ESP32 (vía Wokwi o física) se maneja mediante peticiones CoAP estándar.

El cliente fue diseñado con una estructura **modular** dividida en varios archivos, cada uno cumpliendo una función específica:

| Módulo | Descripción |
|---------|--------------|
| `SensorProvider.*` | Lectura del sensor DHT22 (inicialización y obtención de datos). |
| `CoapMessage.*` | Construcción de mensajes CoAP (header, token, opciones y payload JSON). |
| `CoapClient.*` | Envío UDP, manejo de retransmisiones, espera de ACK y fiabilidad del protocolo. |
| `Config.h` | Configuración de red, IP del servidor EC2, tiempos y parámetros del envío. |
| `esp32_coap_client.ino` | Archivo principal que integra los módulos, gestiona el flujo y conexión Wi-Fi. |

La comunicación se realiza por **UDP puerto 5683**, utilizando mensajes **POST Confirmable (CON)** con respuesta **ACK**.  
El payload se envía en formato **JSON**, con los valores `device`, `t`, `h` y `ts`.

El proyecto fue desarrollado y probado completamente en **Wokwi**, empleando un **DHT22 virtual conectado al GPIO15**.

---

## Conclusiones
- Se logró implementar un servidor CoAP totalmente funcional en C, cumpliendo con las especificaciones del protocolo (tipos de mensajes, métodos y manejo de tokens).
- El cliente en Python permite probar la comunicación local o con un ESP32 simulado.
- La simulación en Wokwi permitió probar un cliente CoAP funcional sin requerir hardware físico.  
- El diseño modular facilitó la comprensión del flujo completo del protocolo y la reutilización del código.  
- Implementar CoAP manualmente ayudó a comprender el manejo de encabezados, opciones, confirmaciones y retransmisiones sobre UDP.  
- Integrar el sensor DHT22 demostró la viabilidad del modelo IoT basado en CoAP, combinando eficiencia y simplicidad.  
- Este cliente puede ser fácilmente adaptado a hardware real, cambiando solo la configuración Wi-Fi.

---

## Referencias
- Wokwi Simulator: [https://wokwi.com/](https://wokwi.com/)  
- Adafruit DHT Sensor Library & Unified Sensor Library  
- Beej’s Guide to Network Programming (para comprensión de sockets UDP)  

---

**Autores:** Juan Pablo Posso Villegas - Juan Camilo Gómez Jiménez  
**Plataforma:** Wokwi (ESP32 DevKit v1)  
**Servidor:** AWS EC2 – CoAP UDP 5683  
**Librerías:** DHT sensor library / Adafruit Unified Sensor

