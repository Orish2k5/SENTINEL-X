#ifndef SX_ENVELOPE_H
#define SX_ENVELOPE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sentinel_x_protocol_v1.h"

typedef struct {
    uint8_t message_type;
    uint8_t key_id;
    uint64_t sequence;
    const uint8_t *message_id;
    uint16_t operation_code;
    uint16_t payload_len;
    const uint8_t *payload;
    const uint8_t *tag;
    const uint8_t *authenticated_bytes;
    size_t authenticated_length;
} sx_envelope_view_t;

typedef struct {
    uint16_t command_code;
    uint8_t allowed;
    uint16_t min_value;
    uint16_t max_value;
} sx_policy_entry_t;

typedef struct {
    uint32_t version;
    uint16_t max_commands_per_second;
    uint8_t entry_count;
    uint8_t flags;
    sx_policy_entry_t entries[8];
} sx_policy_v1_t;

typedef struct {
    uint16_t operation_code;
    uint16_t value;
    uint8_t selector;
} sx_command_t;

typedef enum {
    SX_ENVELOPE_OK = 0,
    SX_ENVELOPE_ERR_ARGUMENT,
    SX_ENVELOPE_ERR_LENGTH,
    SX_ENVELOPE_ERR_MAGIC,
    SX_ENVELOPE_ERR_VERSION,
    SX_ENVELOPE_ERR_MESSAGE_TYPE,
    SX_ENVELOPE_ERR_DEVICE_ID,
    SX_ENVELOPE_ERR_KEY_ID,
    SX_ENVELOPE_ERR_PAYLOAD_LENGTH,
    SX_ENVELOPE_ERR_COMMAND,
    SX_ENVELOPE_ERR_POLICY
} sx_envelope_error_t;

void sx_device_id_make(uint8_t output[SX_SIZE_DEVICE_ID], const char *ascii_id);
bool sx_envelope_parse(
    const uint8_t *bytes,
    size_t length,
    const uint8_t expected_device_id[SX_SIZE_DEVICE_ID],
    sx_envelope_view_t *view,
    sx_envelope_error_t *error);
bool sx_command_decode(const sx_envelope_view_t *view, sx_command_t *command,
                       sx_envelope_error_t *error);
bool sx_policy_v1_decode(const uint8_t *payload, uint16_t payload_len,
                         uint32_t active_policy_version, sx_policy_v1_t *policy,
                         sx_envelope_error_t *error);

#endif /* SX_ENVELOPE_H */
