#include "CoapClient.h"
#include "Config.h"

CoapClient::CoapClient() : _serverPort(0) {}

void CoapClient::begin(const char* serverIp, uint16_t serverPort) {
  _serverAddr.fromString(serverIp);
  _serverPort = serverPort;
  _udp.begin(0); // puerto efímero
}

uint16_t CoapClient::_nextMessageId() {
  static uint16_t mid = 1000;
  return ++mid;
}

void CoapClient::_randomToken(uint8_t* t, uint8_t& tlen) {
  tlen = 4; // TKL=4
  for (uint8_t i=0; i<tlen; ++i) t[i] = (uint8_t)random(0, 256);
}

bool CoapClient::postJson(const String& uriPath1, const String& uriPath2,
                          const uint8_t* json, size_t len,
                          uint32_t ackTimeoutMs, uint8_t maxRetransmit) {
  CoapMessage msg;
  msg.begin(CoapType::CON, CoapCode::POST);
  uint16_t mid = _nextMessageId();
  msg.setMessageId(mid);
  uint8_t token[COAP_MAX_TOKEN_LEN]; uint8_t tLen = 0;
  _randomToken(token, tLen);
  msg.setToken(token, tLen);

  // Uri-Path "sensors" y "env"
  msg.addOption(OPT_URI_PATH, (const uint8_t*)uriPath1.c_str(), (uint8_t)uriPath1.length());
  msg.addOption(OPT_URI_PATH, (const uint8_t*)uriPath2.c_str(), (uint8_t)uriPath2.length());

  // Content-Format: 50 (application/json)
  uint8_t cf = (uint8_t)COAP_CONTENT_FORMAT_JSON;
  msg.addOption(OPT_CONTENT_FORMAT, &cf, 1);

  msg.setPayload(json, len);

  uint8_t buffer[COAP_MAX_MSG_LEN];
  size_t mlen = msg.build(buffer, sizeof(buffer));
  if (mlen == 0) return false;

  uint8_t tries = 0;
  uint32_t timeout = ackTimeoutMs;

  while (true) {
    // Enviar
    _udp.beginPacket(_serverAddr, _serverPort);
    _udp.write(buffer, mlen);
    _udp.endPacket();

    // Esperar ACK
    uint32_t start = millis();
    while ((millis() - start) < timeout) {
      int packetSize = _udp.parsePacket();
      if (packetSize > 0) {
        uint8_t rx[COAP_MAX_MSG_LEN];
        int rlen = _udp.read(rx, sizeof(rx));
        if (rlen >= 4 && CoapMessage::isAckFor(rx, (size_t)rlen, mid)) {
          return true; // ACK correcto
        }
      }
      delay(5);
    }

    // Retransmitir si corresponde
    if (tries >= maxRetransmit) {
      return false;
    }
    tries++;
    timeout = timeout * 2; // backoff exponencial simple
  }
}
