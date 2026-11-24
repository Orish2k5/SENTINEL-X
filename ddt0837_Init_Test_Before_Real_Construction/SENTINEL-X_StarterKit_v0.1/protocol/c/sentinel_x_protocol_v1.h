#ifndef SENTINEL_X_PROTOCOL_V1_H
#define SENTINEL_X_PROTOCOL_V1_H
#include <stdint.h>
/* SENTINEL-X protocol v1 constants; no parser or crypto implementation. */
#define SX_PROTOCOL_VERSION             0x01u
#define SX_ENVELOPE_MAGIC_0             0x53u
#define SX_ENVELOPE_MAGIC_1             0x58u
#define SX_ENVELOPE_HEADER_SIZE         49u
#define SX_ENVELOPE_TAG_SIZE            32u
#define SX_ENVELOPE_MAX_PAYLOAD         128u
#define SX_ENVELOPE_MIN_SIZE            81u
#define SX_ENVELOPE_MAX_SIZE            209u
#define SX_OFF_MAGIC                    0u
#define SX_OFF_PROTOCOL_VERSION         2u
#define SX_OFF_MESSAGE_TYPE             3u
#define SX_OFF_DEVICE_ID                4u
#define SX_SIZE_DEVICE_ID               16u
#define SX_OFF_KEY_ID                   20u
#define SX_OFF_SEQUENCE                 21u
#define SX_SIZE_SEQUENCE                8u
#define SX_OFF_MESSAGE_ID               29u
#define SX_SIZE_MESSAGE_ID              16u
#define SX_OFF_OPERATION_CODE           45u
#define SX_OFF_PAYLOAD_LEN               47u
#define SX_OFF_PAYLOAD                   49u
#define SX_MSG_CMD                      0x01u
#define SX_MSG_ACK                      0x02u
#define SX_MSG_EVENT                    0x03u
#define SX_MSG_POLICY_UPDATE            0x04u
#define SX_MSG_STATUS                   0x05u
#define SX_OP_SET_OUTPUT                0x0001u
#define SX_OP_SET_LEVEL                 0x0002u
#define SX_OP_GET_STATUS                0x0003u
#define SX_OP_POLICY_UPDATE             0x0100u
#define SX_UART_SOF_0                   0xA5u
#define SX_UART_SOF_1                   0x5Au
#define SX_UART_FRAME_AUTH_ENVELOPE     0x01u
#define SX_UART_FRAME_LINK_STATUS       0x02u
#define SX_UART_BAUD_RATE               115200u
#define SX_UART_MAX_FRAME_SIZE          216u
#define SX_CRC16_CCITT_FALSE_INIT       0xFFFFu
#define SX_CRC16_CCITT_FALSE_POLY       0x1021u
#define SX_CRC16_CCITT_FALSE_XOROUT     0x0000u
#endif
