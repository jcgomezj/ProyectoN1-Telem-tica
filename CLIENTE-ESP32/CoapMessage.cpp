#include "CoapMessage.h"

CoapMessage::CoapMessage() : m_type(CoapType::CON), m_code(CoapCode::EMPTY),
  m_tokenLen(0), m_messageId(0), m_optCount(0), m_payload(nullptr), m_payloadLen(0) {}

void CoapMessage::begin(CoapType type, CoapCode code) {
  m_type = type; m_code = code;
  m_tokenLen = 0; m_optCount = 0; m_payload = nullptr; m_payloadLen = 0;
}

void CoapMessage::setMessageId(uint16_t mid) { m_messageId = mid; }

void CoapMessage::setToken(const uint8_t* t, uint8_t tlen) {
  if (t && tlen <= COAP_MAX_TOKEN_LEN) {
    memcpy(m_token, t, tlen);
    m_tokenLen = tlen;
  } else {
    m_tokenLen = 0;
  }
}

bool CoapMessage::addOption(uint16_t number, const uint8_t* val, uint8_t len) {
  if (m_optCount >= 4 || len > 64) return false;
  m_options[m_optCount].number = number;
  m_options[m_optCount].length = len;
  if (len > 0 && val) memcpy(m_options[m_optCount].value, val, len);
  m_optCount++;
  return true;
}

bool CoapMessage::setPayload(const uint8_t* data, size_t len) {
  m_payload = data; m_payloadLen = len;
  return true;
}

bool CoapMessage::encodeOption(uint8_t*& p, size_t& remaining, uint16_t& runningDelta,
                               uint16_t number, const uint8_t* val, uint8_t len) {
  uint16_t delta = number - runningDelta;
  if (delta >= 13 || len >= 13) return false; // implementación mínima (nuestros opts <13)

  if (remaining < (size_t)(1 + len)) return false;
  uint8_t byte = (uint8_t)((delta << 4) | (len & 0x0F));
  *p++ = byte; remaining--;

  if (len > 0) {
    if (remaining < len) return false;
    memcpy(p, val, len);
    p += len; remaining -= len;
  }
  runningDelta = number;
  return true;
}

size_t CoapMessage::build(uint8_t* outBuf, size_t outLen) {
  if (!outBuf || outLen < 4) return 0;

  uint8_t* p = outBuf;
  size_t remaining = outLen;

  // Header: Ver=1 (2b), Type (2b), TKL (4b)
  uint8_t ver = 1;
  uint8_t tkl = m_tokenLen & 0x0F;
  uint8_t first = (uint8_t)((ver << 6) | (((uint8_t)m_type & 0x03) << 4) | tkl);
  *p++ = first; remaining--;

  // Code
  *p++ = (uint8_t)m_code; remaining--;

  // Message ID (big-endian)
  *p++ = (uint8_t)((m_messageId >> 8) & 0xFF); remaining--;
  *p++ = (uint8_t)(m_messageId & 0xFF); remaining--;

  // Token
  if (m_tokenLen > 0) {
    if (remaining < m_tokenLen) return 0;
    memcpy(p, m_token, m_tokenLen);
    p += m_tokenLen; remaining -= m_tokenLen;
  }

  // Opciones (en orden creciente)
  // Ordenamos simple por burbuja (máx 4 opciones)
  for (uint8_t i=0; i<m_optCount; ++i) {
    for (uint8_t j=i+1; j<m_optCount; ++j) {
      if (m_options[j].number < m_options[i].number) {
        CoapOptionKV tmp = m_options[i];
        m_options[i] = m_options[j];
        m_options[j] = tmp;
      }
    }
  }

  uint16_t runningDelta = 0;
  for (uint8_t i=0; i<m_optCount; ++i) {
    if (!encodeOption(p, remaining, runningDelta,
                      m_options[i].number, m_options[i].value, m_options[i].length)) {
      return 0;
    }
  }

  // Payload (si hay)
  if (m_payload && m_payloadLen > 0) {
    if (remaining < (1 + m_payloadLen)) return 0;
    *p++ = 0xFF; // payload marker
    remaining--;
    memcpy(p, m_payload, m_payloadLen);
    p += m_payloadLen;
    remaining -= m_payloadLen;
  }

  return (size_t)(p - outBuf);
}

bool CoapMessage::isAckFor(const uint8_t* buf, size_t len, uint16_t expectedMid) {
  if (!buf || len < 4) return false;
  uint8_t first = buf[0];
  uint8_t ver = (first >> 6) & 0x03;
  uint8_t type = (first >> 4) & 0x03;
  if (ver != 1) return false;
  if (type != (uint8_t)CoapType::ACK) return false;
  uint16_t mid = ((uint16_t)buf[2] << 8) | buf[3];
  return (mid == expectedMid);
}
