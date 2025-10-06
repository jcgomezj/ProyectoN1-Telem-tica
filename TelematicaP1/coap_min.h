#ifndef COAP_MIN_H
#define COAP_MIN_H
#include <stdint.h>
#include <stddef.h>

#define COAP_TYPE_CON 0
#define COAP_TYPE_ACK 2

#define COAP_GET    1
#define COAP_POST   2
#define COAP_PUT    3
#define COAP_DELETE 4

#define COAP_2_01_CREATED 0x41
#define COAP_2_04_CHANGED 0x44
#define COAP_2_05_CONTENT 0x45
#define COAP_4_00_BADREQ  0x80
#define COAP_4_04_NOTFND  0x84

size_t coap_build_reply(uint8_t *out, size_t cap,
                        uint8_t type, uint8_t code, uint16_t mid,
                        const uint8_t *token, uint8_t tkl,
                        const char *payload);

#endif
