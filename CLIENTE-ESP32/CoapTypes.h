#pragma once
#include <Arduino.h>

//Tipos y constantes CoAP mínimos
enum class CoapType : uint8_t {
  CON = 0, NON = 1, ACK = 2, RST = 3
};

enum class CoapCode : uint8_t {
  EMPTY = 0x00,
  //Métodos
  GET = 0x01, POST = 0x02, PUT = 0x03, DELETE_ = 0x04,
  CREATED_201 = 0x41, DELETED_202 = 0x42, VALID_203 = 0x43,
  CHANGED_204 = 0x44, CONTENT_205 = 0x45
};

//Opciones CoAP
static const uint16_t OPT_URI_PATH      = 11;
static const uint16_t OPT_CONTENT_FORMAT= 12;

//Tamaños
static const size_t COAP_MAX_TOKEN_LEN = 8;
static const size_t COAP_MAX_MSG_LEN   = 512;
