#ifndef SENTINEL_X_PROTOCOL_V1_H
#define SENTINEL_X_PROTOCOL_V1_H

/*
 * SENTINEL-X protocol constants, version 1.
 * This header defines constants only; it does not implement parsing or cryptography.
 * Source of truth: docs/03-interface-contracts/INTERFACE_CONTRACTS_v1.0.md
 */

#include <stdint.h>

#define SX_PROTOCOL_VERSION             0x01u
#define SX_ENVELOPE_MAGIC_0             0x53u /* 'S' */
#define SX_ENVELOPE_MAGIC_1             0x58u /* 'X' */

#define SX_ENVELOPE_HEADER_SIZE         49u
#define SX_ENVELOPE_TAG_SIZE            32u
#define SX_ENVELOPE_MAX_PAYLOAD         128u
#define SX_ENVELOPE_MAX_SIZE            (SX_ENVELOPE_HEADER_SIZE + SX_ENVELOPE_MAX_PAYLOAD + SX_ENVELOPE_TAG_SIZE) /* 209 */

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
#define SX_OFF_PAYLOAD_LEN              47u
#define SX_OFF_PAYLOAD                  49u

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

/* Result/error codes: values are uint16 big-endian on the wire. */
#define SX_RESULT_SUCCESS               0x0000u
#define SX_ERR_FRAME_INVALID            0x1001u
#define SX_ERR_LENGTH_INVALID           0x1002u
#define SX_ERR_CRC_INVALID              0x1003u
#define SX_ERR_VERSION_UNSUPPORTED      0x1004u
#define SX_ERR_DEVICE_ID_MISMATCH       0x1005u
#define SX_ERR_HMAC_INVALID             0x1006u
#define SX_ERR_REPLAY_OR_STALE_SEQUENCE 0x1007u
#define SX_ERR_RATE_LIMITED             0x1008u
#define SX_ERR_COMMAND_NOT_ALLOWED      0x1009u
#define SX_ERR_PARAMETER_OUT_OF_RANGE   0x100Au
#define SX_ERR_POLICY_VERSION_INVALID   0x100Bu
#define SX_ERR_PERSISTENCE_FAILED        0x100Cu
#define SX_ERR_SAFE_MODE_ACTIVE          0x100Du
#define SX_ERR_UNKNOWN_COMMAND           0x100Eu
#define SX_ERR_PAYLOAD_INVALID           0x100Fu
#define SX_ERR_KEY_ID_UNKNOWN            0x1010u
#define SX_ERR_LINK_OFFLINE              0x1011u
#define SX_ERR_DEVICE_BUSY               0x1012u
#define SX_ERR_EXECUTION_FAILED          0x1013u
#define SX_ERR_ACK_TIMEOUT               0x1014u
#define SX_ERR_ROLE_FORBIDDEN            0x1015u
#define SX_ERR_SESSION_EXPIRED           0x1016u
#define SX_ERR_DEVICE_NOT_FOUND          0x1017u
#define SX_ERR_IDEMPOTENCY_CONFLICT      0x1018u
#define SX_ERR_STORAGE_UNAVAILABLE       0x1019u
#define SX_ERR_POLICY_PENDING            0x101Au

#endif /* SENTINEL_X_PROTOCOL_V1_H */
