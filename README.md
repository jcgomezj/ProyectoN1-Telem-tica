# ProyectoN1-Telematica

## Introducción
Este proyecto implementa un **cliente CoAP** (Constrained Application Protocol) ejecutado en un **ESP32 simulado en Wokwi**, que envía lecturas reales de **temperatura y humedad** obtenidas de un sensor **DHT22 virtual** hacia un servidor CoAP desplegado en **AWS EC2**. 

---

## Desarrollo e implementación
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

