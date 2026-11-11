#ifndef SX_UART_FRAME_H
#define SX_UART_FRAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sentinel_x_protocol_v1.h"

#define SX_UART_FRAME_PAYLOAD_CAPACITY 209u
#define SX_UART_FRAME_TIMEOUT_MS 100u /* PROPOSED: confirm with ESP32 sender timing. */

typedef struct {
    uint8_t frame_type;
    uint16_t payload_len;
    uint8_t payload[SX_UART_FRAME_PAYLOAD_CAPACITY];
} sx_uart_frame_t;

typedef enum {
    SX_UART_PARSE_ERROR = -1,
    SX_UART_PARSE_NONE = 0,
    SX_UART_PARSE_COMPLETE = 1
} sx_uart_parse_result_t;

typedef struct {
    uint8_t state;
    uint8_t frame_type;
    uint16_t payload_len;
    uint16_t payload_index;
    uint16_t crc_running;
    uint16_t crc_received;
    uint8_t length_hi;
    uint8_t payload[SX_UART_FRAME_PAYLOAD_CAPACITY];
    uint32_t error_count;
} sx_uart_parser_t;

void sx_uart_parser_init(sx_uart_parser_t *parser);
void sx_uart_parser_reset(sx_uart_parser_t *parser);
bool sx_uart_parser_is_active(const sx_uart_parser_t *parser);
sx_uart_parse_result_t sx_uart_parser_feed(
    sx_uart_parser_t *parser,
    uint8_t byte,
    sx_uart_frame_t *completed_frame);
uint16_t sx_crc16_ccitt_false(const uint8_t *data, size_t length);

#endif /* SX_UART_FRAME_H */
