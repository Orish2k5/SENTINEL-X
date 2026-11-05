#ifndef SX_POLICY_H
#define SX_POLICY_H
#include <stddef.h>
#include <stdint.h>
#include "sx_uart_frame.h"
#define SX_POLICY_HEADER_SIZE 8u
#define SX_POLICY_ENTRY_SIZE 7u
#define SX_POLICY_MAX_SIZE 64u
#define SX_POLICY_MAX_ENTRIES 8u
#define SX_POLICY_FLAG_FORCE_OFF_ON_LINK_LOSS 0x01u
typedef struct { uint16_t command_code; uint8_t allowed; uint16_t min_value; uint16_t max_value; } sx_policy_entry_t;
typedef struct { uint32_t policy_version; uint16_t max_commands_per_second; uint8_t entry_count; uint8_t flags; sx_policy_entry_t entries[SX_POLICY_MAX_ENTRIES]; } sx_policy_t;
sx_proto_status_t sx_policy_parse_validate(const uint8_t *payload, size_t payload_len,
                                           uint32_t active_policy_version, sx_policy_t *out_policy);
#endif
