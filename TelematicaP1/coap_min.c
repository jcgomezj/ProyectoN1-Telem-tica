#include "coap_min.h"
#include <string.h>

size_t coap_build_reply(uint8_t *out, size_t cap,
                        uint8_t type, uint8_t code, uint16_t mid,
                        const uint8_t *token, uint8_t tkl,
                        const char *payload) {
    size_t pos = 0;
    if (cap < 4) return 0;
    out[pos++] = (1 << 6) | ((type & 3) << 4) | (tkl & 0x0F);
    out[pos++] = code;
    out[pos++] = (mid >> 8) & 0xFF;
    out[pos++] = mid & 0xFF;

    if (tkl) {
        if (pos + tkl > cap) return 0;
        memcpy(&out[pos], token, tkl);
        pos += tkl;
    }

    if (payload && payload[0]) {
        size_t len = strlen(payload);
        if (pos + 1 + len > cap) return 0;
        out[pos++] = 0xFF;
        memcpy(&out[pos], payload, len);
        pos += len;
    }
    return pos;
}
