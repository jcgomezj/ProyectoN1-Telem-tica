#pragma once
#include <Arduino.h>
#include "CoapTypes.h"

struct CoapOptionKV {
  uint16_t number;
  uint8_t  value[64];
  uint8_t  length;
};

class CoapMessage {
public:
  CoapMessage();

  void begin(CoapType type, CoapCode code);
  void setMessageId(uint16_t mid);
  void setToken(const uint8_t* t, uint8_t tlen);

  bool addOption(uint16_t number, const uint8_t* val, uint8_t len);
  bool setPayload(const uint8_t* data, size_t len);

  // Construye el buffer final listo para enviar por UDP
  // Devuelve longitud en bytes o 0 si error.
  size_t build(uint8_t* outBuf, size_t outLen);

  // Simple parser para detectar ACK y MID
  static bool isAckFor(const uint8_t* buf, size_t len, uint16_t expectedMid);

private:
  CoapType  m_type;
  CoapCode  m_code;
  uint8_t   m_token[COAP_MAX_TOKEN_LEN];
  uint8_t   m_tokenLen;
  uint16_t  m_messageId;

  CoapOptionKV m_options[4];
  uint8_t      m_optCount;

  const uint8_t* m_payload;
  size_t         m_payloadLen;

  bool encodeOption(uint8_t*& p, size_t& remaining, uint16_t& runningDelta,
                    uint16_t number, const uint8_t* val, uint8_t len);
};
