#pragma once
#include <Arduino.h>
#include <WiFiUdp.h>
#include "CoapTypes.h"
#include "CoapMessage.h"

class CoapClient {
public:
  CoapClient();
  void begin(const char* serverIp, uint16_t serverPort);
  // Envía POST CON /sensors/env con content-format JSON y espera ACK con reintentos
  bool postJson(const String& uriPath1, const String& uriPath2,
                const uint8_t* json, size_t len,
                uint32_t ackTimeoutMs, uint8_t maxRetransmit);

private:
  WiFiUDP _udp;
  IPAddress _serverAddr;
  uint16_t _serverPort;

  uint16_t _nextMessageId();
  void _randomToken(uint8_t* t, uint8_t& tlen);
};
