#include "sx_envelope.h"
#include <string.h>
static uint16_t read_be16(const uint8_t *p) { return (uint16_t)(((uint16_t)p[0] << 8) | p[1]); }
uint64_t sx_read_be64(const uint8_t *data)
{
    uint64_t value = 0u;
    unsigned i;
    for (i = 0u; i < 8u; ++i) value = (value << 8) | data[i];
    return value;
}
static int is_device_char(uint8_t c)
{
    return ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-');
}
static int valid_device_field(const uint8_t field[16])
{
    size_t i; int padding_seen = 0, has_char = 0;
    for (i = 0u; i < 16u; ++i) {
        if (field[i] == 0u) padding_seen = 1;
        else {
            if (padding_seen || !is_device_char(field[i])) return 0;
            has_char = 1;
        }
    }
    return has_char;
}
sx_proto_status_t sx_envelope_parse(const uint8_t *data, size_t data_len, sx_envelope_view_t *out_view)
{
    uint16_t payload_len; size_t expected_len; uint8_t type;
    if ((data == NULL) || (out_view == NULL)) return SX_PROTO_ERR_ARGUMENT;
    if ((data_len < SX_ENVELOPE_MIN_SIZE) || (data_len > SX_ENVELOPE_MAX_SIZE)) return SX_PROTO_ERR_LENGTH;
    if ((data[0] != SX_ENVELOPE_MAGIC_0) || (data[1] != SX_ENVELOPE_MAGIC_1)) return SX_PROTO_ERR_FIELD;
    if (data[SX_OFF_PROTOCOL_VERSION] != SX_PROTOCOL_VERSION) return SX_PROTO_ERR_FIELD;
    type = data[SX_OFF_MESSAGE_TYPE];
    if ((type < SX_MSG_CMD) || (type > SX_MSG_STATUS)) return SX_PROTO_ERR_TYPE;
    if (!valid_device_field(&data[SX_OFF_DEVICE_ID])) return SX_PROTO_ERR_FIELD;
    payload_len = read_be16(&data[SX_OFF_PAYLOAD_LEN]);
    if (payload_len > SX_ENVELOPE_MAX_PAYLOAD) return SX_PROTO_ERR_LENGTH;
    expected_len = SX_ENVELOPE_HEADER_SIZE + (size_t)payload_len + SX_ENVELOPE_TAG_SIZE;
    if (data_len != expected_len) return SX_PROTO_ERR_LENGTH;
    out_view->protocol_version = data[SX_OFF_PROTOCOL_VERSION];
    out_view->message_type = type;
    out_view->device_id_raw = &data[SX_OFF_DEVICE_ID];
    out_view->key_id = data[SX_OFF_KEY_ID];
    out_view->sequence = sx_read_be64(&data[SX_OFF_SEQUENCE]);
    out_view->message_id = &data[SX_OFF_MESSAGE_ID];
    out_view->operation_code = read_be16(&data[SX_OFF_OPERATION_CODE]);
    out_view->payload_len = payload_len;
    out_view->payload = &data[SX_OFF_PAYLOAD];
    out_view->tag = &data[SX_ENVELOPE_HEADER_SIZE + payload_len];
    out_view->authenticated_bytes = data;
    out_view->authenticated_len = SX_ENVELOPE_HEADER_SIZE + (size_t)payload_len;
    return SX_PROTO_OK;
}
int sx_envelope_device_id_matches(const sx_envelope_view_t *view, const char *expected_device_id)
{
    size_t i, n;
    if ((view == NULL) || (view->device_id_raw == NULL) || (expected_device_id == NULL)) return 0;
    n = strlen(expected_device_id);
    if ((n == 0u) || (n > 16u)) return 0;
    for (i = 0u; i < 16u; ++i) {
        uint8_t expected = (i < n) ? (uint8_t)expected_device_id[i] : 0u;
        if (view->device_id_raw[i] != expected) return 0;
    }
    return 1;
}
