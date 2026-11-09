#ifndef SX_UART_FRAME_H
#define SX_UART_FRAME_H
#include<stddef.h>
#include <stdint.h>
#include "sentinel_x_protocol_v1.h"

typedef enum {
    SX_PROTO_OK = 0,
    SX_PROTO_ERR_ARGUMENT,
    SX_PROTO_ERR_LENGTH,
    SX_PROTO_ERR_SOF,
    SX_PROTO_ERR_TYPE,
    SX_PROTO_ERR_CRC,
    SX_PROTO_ERR_FIELD
} sx_proto_status_t;

typedef struct {
    uint8_t frame_type;
    uint16_t payload_len;
    const uint8_t *payload; /* pointer into caller-owned input */
} sx_uart_frame_view_t;

sx_proto_status_t sx_uart_frame_parse(const uint8_t *frame, size_t frame_len,
                                      sx_uart_frame_view_t *out_view);
size_t sx_uart_frame_build(uint8_t frame_type, const uint8_t *payload,
                           uint16_t payload_len, uint8_t *out_frame,
                           size_t out_capacity);
#endif
