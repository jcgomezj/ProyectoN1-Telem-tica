#pragma once

//WiFi Wokwi
#define WIFI_SSID      "Wokwi-GUEST"
#define WIFI_PASSWORD  ""

//Servidor CoAP
#define COAP_SERVER_IP   "192.168.1.100"   // IP DEL SERVIDOR
#define COAP_SERVER_PORT 5683              // Puerto del servidor CoAP

#define COAP_URI_PATH_1  "sensors"
#define COAP_URI_PATH_2  "env"
#define COAP_CONTENT_FORMAT_JSON 50

#define POST_PERIOD_MS       5000   // Enviar cada 5 s
#define ACK_TIMEOUT_MS       2000
#define MAX_RETRANSMIT       4

//Identidad del dispositivo
#define DEVICE_NAME          "esp32-sim"
