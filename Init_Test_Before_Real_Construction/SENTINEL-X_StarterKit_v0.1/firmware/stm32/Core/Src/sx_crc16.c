#include "sx_crc16.h"
#include "sentinel_x_protocol_v1.h"

uint16_t sx_crc16_ccitt_false(const uint8_t *data, size_t length)
{
    uint16_t crc = SX_CRC16_CCITT_FALSE_INIT;
    size_t i;
    unsigned bit;
    if ((data == NULL) && (length != 0u)) return 0u;
    for (i = 0u; i < length; ++i) {
        crc ^= (uint16_t)((uint16_t)data[i] << 8);
        for (bit = 0u; bit < 8u; ++bit) {
            crc = ((crc & 0x8000u) != 0u)
                ? (uint16_t)((crc << 1) ^ SX_CRC16_CCITT_FALSE_POLY)
                : (uint16_t)(crc << 1);
        }
    }
    return (uint16_t)(crc ^ SX_CRC16_CCITT_FALSE_XOROUT);
}
