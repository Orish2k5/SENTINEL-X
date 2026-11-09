#include "sx_uart_frame.h"
#include "sx_crc16.h"

static uint16_t read_be16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}
static void write_be16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value >> 8);
    p[1] = (uint8_t)value;
}
static sx_proto_status_t validate_payload(uint8_t type, const uint8_t *p, uint16_t n)
{
    if (type == SX_UART_FRAME_AUTH_ENVELOPE) {
        return ((n >= SX_ENVELOPE_MIN_SIZE) && (n <= SX_ENVELOPE_MAX_SIZE))
            ? SX_PROTO_OK : SX_PROTO_ERR_LENGTH;
    }
    if (type == SX_UART_FRAME_LINK_STATUS) {
        if ((n != 7u) || (p == NULL)) return SX_PROTO_ERR_LENGTH;
        if ((p[0] > 1u) || (p[1] > 1u) || (p[2] != 0u)) return SX_PROTO_ERR_FIELD;
        return SX_PROTO_OK;
    }
    return SX_PROTO_ERR_TYPE;
}

sx_proto_status_t sx_uart_frame_parse(const uint8_t *frame, size_t frame_len,
                                      sx_uart_frame_view_t *out_view)
{
    uint16_t payload_len, supplied_crc, actual_crc;
    size_t expected_len;
    sx_proto_status_t status;
    if ((frame == NULL) || (out_view == NULL)) return SX_PROTO_ERR_ARGUMENT;
    if ((frame_len < 7u) || (frame_len > SX_UART_MAX_FRAME_SIZE)) return SX_PROTO_ERR_LENGTH;
    if ((frame[0] != SX_UART_SOF_0) || (frame[1] != SX_UART_SOF_1)) return SX_PROTO_ERR_SOF;
    payload_len = read_be16(&frame[3]);
    expected_len = 7u + (size_t)payload_len;
    if ((expected_len != frame_len) || (expected_len > SX_UART_MAX_FRAME_SIZE)) return SX_PROTO_ERR_LENGTH;
    status = validate_payload(frame[2], &frame[5], payload_len);
    if (status != SX_PROTO_OK) return status;
    supplied_crc = read_be16(&frame[frame_len - 2u]);
    actual_crc = sx_crc16_ccitt_false(&frame[2], 3u + (size_t)payload_len);
    if (supplied_crc != actual_crc) return SX_PROTO_ERR_CRC;
    out_view->frame_type = frame[2];
    out_view->payload_len = payload_len;
    out_view->payload = &frame[5];
    return SX_PROTO_OK;
}

size_t sx_uart_frame_build(uint8_t frame_type, const uint8_t *payload,
                           uint16_t payload_len, uint8_t *out_frame,
                           size_t out_capacity)
{
    size_t frame_len = 7u + (size_t)payload_len;
    uint16_t crc;
    size_t i;
    if ((out_frame == NULL) || ((payload == NULL) && (payload_len != 0u))) return 0u;
    if ((frame_len > out_capacity) || (frame_len > SX_UART_MAX_FRAME_SIZE)) return 0u;
    if (validate_payload(frame_type, payload, payload_len) != SX_PROTO_OK) return 0u;
    out_frame[0] = SX_UART_SOF_0;
    out_frame[1] = SX_UART_SOF_1;
    out_frame[2] = frame_type;
    write_be16(&out_frame[3], payload_len);
    for (i = 0u; i < payload_len; ++i) out_frame[5u + i] = payload[i];
    crc = sx_crc16_ccitt_false(&out_frame[2], 3u + (size_t)payload_len);
    write_be16(&out_frame[5u + payload_len], crc);
    return frame_len;
}
