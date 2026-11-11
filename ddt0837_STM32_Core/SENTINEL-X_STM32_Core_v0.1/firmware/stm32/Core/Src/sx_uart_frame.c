#include "sx_uart_frame.h"

#include <string.h>

enum {
    SX_PARSE_WAIT_SOF_1 = 0,
    SX_PARSE_WAIT_SOF_2,
    SX_PARSE_FRAME_TYPE,
    SX_PARSE_LENGTH_HI,
    SX_PARSE_LENGTH_LO,
    SX_PARSE_PAYLOAD,
    SX_PARSE_CRC_HI,
    SX_PARSE_CRC_LO
};

static uint16_t sx_crc_update(uint16_t crc, uint8_t byte)
{
    uint8_t bit;

    crc ^= (uint16_t)((uint16_t)byte << 8);
    for (bit = 0u; bit < 8u; ++bit) {
        if ((crc & 0x8000u) != 0u) {
            crc = (uint16_t)((crc << 1) ^ SX_CRC16_CCITT_FALSE_POLY);
        } else {
            crc = (uint16_t)(crc << 1);
        }
    }
    return crc;
}

uint16_t sx_crc16_ccitt_false(const uint8_t *data, size_t length)
{
    size_t i;
    uint16_t crc = SX_CRC16_CCITT_FALSE_INIT;

    if ((data == NULL) && (length != 0u)) {
        return 0u;
    }

    for (i = 0u; i < length; ++i) {
        crc = sx_crc_update(crc, data[i]);
    }
    return (uint16_t)(crc ^ SX_CRC16_CCITT_FALSE_XOROUT);
}

void sx_uart_parser_init(sx_uart_parser_t *parser)
{
    if (parser != NULL) {
        (void)memset(parser, 0, sizeof(*parser));
        parser->state = SX_PARSE_WAIT_SOF_1;
        parser->crc_running = SX_CRC16_CCITT_FALSE_INIT;
    }
}

void sx_uart_parser_reset(sx_uart_parser_t *parser)
{
    uint32_t errors;

    if (parser == NULL) {
        return;
    }

    errors = parser->error_count;
    (void)memset(parser, 0, sizeof(*parser));
    parser->state = SX_PARSE_WAIT_SOF_1;
    parser->crc_running = SX_CRC16_CCITT_FALSE_INIT;
    parser->error_count = errors;
}

bool sx_uart_parser_is_active(const sx_uart_parser_t *parser)
{
    return (parser != NULL) && (parser->state != SX_PARSE_WAIT_SOF_1);
}

static sx_uart_parse_result_t sx_parser_error(sx_uart_parser_t *parser)
{
    if (parser != NULL) {
        parser->error_count++;
        sx_uart_parser_reset(parser);
    }
    return SX_UART_PARSE_ERROR;
}

static bool sx_valid_frame_length(uint8_t frame_type, uint16_t payload_len)
{
    if (frame_type == SX_UART_FRAME_AUTH_ENVELOPE) {
        return (payload_len >= 81u) && (payload_len <= SX_UART_FRAME_PAYLOAD_CAPACITY);
    }
    if (frame_type == SX_UART_FRAME_LINK_STATUS) {
        return payload_len == 7u;
    }
    return false;
}

sx_uart_parse_result_t sx_uart_parser_feed(
    sx_uart_parser_t *parser,
    uint8_t byte,
    sx_uart_frame_t *completed_frame)
{
    if ((parser == NULL) || (completed_frame == NULL)) {
        return SX_UART_PARSE_ERROR;
    }

    switch (parser->state) {
    case SX_PARSE_WAIT_SOF_1:
        if (byte == SX_UART_SOF_0) {
            parser->state = SX_PARSE_WAIT_SOF_2;
        }
        break;

    case SX_PARSE_WAIT_SOF_2:
        if (byte == SX_UART_SOF_1) {
            parser->state = SX_PARSE_FRAME_TYPE;
            parser->crc_running = SX_CRC16_CCITT_FALSE_INIT;
        } else if (byte == SX_UART_SOF_0) {
            /* Keep the latest 0xA5 as a possible start of frame. */
            parser->state = SX_PARSE_WAIT_SOF_2;
        } else {
            parser->state = SX_PARSE_WAIT_SOF_1;
        }
        break;

    case SX_PARSE_FRAME_TYPE:
        parser->frame_type = byte;
        parser->crc_running = sx_crc_update(parser->crc_running, byte);
        parser->state = SX_PARSE_LENGTH_HI;
        break;

    case SX_PARSE_LENGTH_HI:
        parser->length_hi = byte;
        parser->crc_running = sx_crc_update(parser->crc_running, byte);
        parser->state = SX_PARSE_LENGTH_LO;
        break;

    case SX_PARSE_LENGTH_LO:
        parser->crc_running = sx_crc_update(parser->crc_running, byte);
        parser->payload_len = (uint16_t)(((uint16_t)parser->length_hi << 8) | byte);
        parser->payload_index = 0u;
        if (!sx_valid_frame_length(parser->frame_type, parser->payload_len)) {
            return sx_parser_error(parser);
        }
        parser->state = SX_PARSE_PAYLOAD;
        break;

    case SX_PARSE_PAYLOAD:
        parser->payload[parser->payload_index] = byte;
        parser->payload_index++;
        parser->crc_running = sx_crc_update(parser->crc_running, byte);
        if (parser->payload_index == parser->payload_len) {
            parser->state = SX_PARSE_CRC_HI;
        }
        break;

    case SX_PARSE_CRC_HI:
        parser->crc_received = (uint16_t)((uint16_t)byte << 8);
        parser->state = SX_PARSE_CRC_LO;
        break;

    case SX_PARSE_CRC_LO:
        parser->crc_received |= byte;
        if (parser->crc_received != parser->crc_running) {
            return sx_parser_error(parser);
        }
        completed_frame->frame_type = parser->frame_type;
        completed_frame->payload_len = parser->payload_len;
        (void)memcpy(completed_frame->payload, parser->payload, parser->payload_len);
        sx_uart_parser_reset(parser);
        return SX_UART_PARSE_COMPLETE;

    default:
        return sx_parser_error(parser);
    }

    return SX_UART_PARSE_NONE;
}
