#ifndef SX_ENVELOPE_H
#define SX_ENVELOPE_H
#include <stddef.h>
#include <stdint.h>
#include "sx_uart_frame.h"

typedef struct {
    uint8_t protocol_version;
    uint8_t message_type;
    const uint8_t *device_id_raw;
    uint8_t key_id;
    uint64_t sequence;
    const uint8_t *message_id;
    uint16_t operation_code;
    uint16_t payload_len;
    const uint8_t *payload;
    const uint8_t *tag;
    const uint8_t *authenticated_bytes;
    size_t authenticated_len;
} sx_envelope_view_t;

sx_proto_status_t sx_envelope_parse(const uint8_t *data, size_t data_len,
                                    sx_envelope_view_t *out_view);
int sx_envelope_device_id_matches(const sx_envelope_view_t *view,
                                  const char *expected_device_id);
uint64_t sx_read_be64(const uint8_t *data);
#endif
