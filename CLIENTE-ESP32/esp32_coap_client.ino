#include <WiFi.h>
#include "Config.h"
#include "CoapClient.h"
#include "SensorProvider.h"

CoapClient      coap;
SensorProvider  sensors;

uint32_t lastPost = 0;

void connectWiFi() {
  Serial.printf("[WiFi] Conectando a '%s'...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
    if (millis() - t0 > 20000) {
      Serial.println("\n[WiFi] Timeout. Reintentando...");
      t0 = millis();
    }
  }
  Serial.printf("\n[WiFi] OK. IP: %s\n", WiFi.localIP().toString().c_str());
}

String buildJson(const EnvSample& s) {
  // JSON compacto
  String j = "{";
  j += "\"device\":\"" + s.device + "\",";
  j += "\"t\":" + String(s.temperatureC, 2) + ",";
  j += "\"h\":" + String(s.humidity, 2) + ",";
  j += "\"ts\":" + String(s.ts);
  j += "}";
  return j;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n==== ESP32 CoAP Client (Wokwi) ====");

  connectWiFi();

  sensors.begin(DEVICE_NAME);
  coap.begin(COAP_SERVER_IP, COAP_SERVER_PORT);

  Serial.printf("[CoAP] Enviará POST a coap://%s:%u/%s/%s cada %ums\n",
    COAP_SERVER_IP, COAP_SERVER_PORT, COAP_URI_PATH_1, COAP_URI_PATH_2, POST_PERIOD_MS);
}

void loop() {
  if (millis() - lastPost >= POST_PERIOD_MS) {
    lastPost = millis();

    EnvSample s = sensors.read();
    String json = buildJson(s);
    Serial.printf("[CoAP] POST -> %s/%s : %s\n", COAP_URI_PATH_1, COAP_URI_PATH_2, json.c_str());

    bool ok = coap.postJson(COAP_URI_PATH_1, COAP_URI_PATH_2,
                            (const uint8_t*)json.c_str(), json.length(),
                            ACK_TIMEOUT_MS, MAX_RETRANSMIT);

    if (ok) {
      Serial.println("[CoAP] ACK recibido ✔");
    } else {
      Serial.println("[CoAP] Sin ACK tras reintentos ✖");
    }
  }


  delay(10);
}
