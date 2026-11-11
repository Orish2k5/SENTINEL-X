#include "sx_envelope.h"

#include <string.h>

static uint16_t sx_read_u16_be(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t sx_read_u32_be(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static uint64_t sx_read_u64_be(const uint8_t *p)
{
    uint64_t value = 0u;
    uint8_t i;
    for (i = 0u; i < 8u; ++i) {
        value = (value << 8) | (uint64_t)p[i];
    }
    return value;
}

static void sx_set_error(sx_envelope_error_t *error, sx_envelope_error_t value)
{
    if (error != NULL) {
        *error = value;
    }
}

void sx_device_id_make(uint8_t output[SX_SIZE_DEVICE_ID], const char *ascii_id)
{
    size_t i;

    if (output == NULL) {
        return;
    }
    (void)memset(output, 0, SX_SIZE_DEVICE_ID);
    if (ascii_id == NULL) {
        return;
    }
    for (i = 0u; (i < SX_SIZE_DEVICE_ID) && (ascii_id[i] != '\0'); ++i) {
        output[i] = (uint8_t)ascii_id[i];
    }
}

bool sx_envelope_parse(
    const uint8_t *bytes,
    size_t length,
    const uint8_t expected_device_id[SX_SIZE_DEVICE_ID],
    sx_envelope_view_t *view,
    sx_envelope_error_t *error)
{
    uint16_t payload_len;
    size_t expected_length;

    if ((bytes == NULL) || (expected_device_id == NULL) || (view == NULL)) {
        sx_set_error(error, SX_ENVELOPE_ERR_ARGUMENT);
        return false;
    }
    if ((length < (SX_ENVELOPE_HEADER_SIZE + SX_ENVELOPE_TAG_SIZE)) ||
        (length > SX_ENVELOPE_MAX_SIZE)) {
        sx_set_error(error, SX_ENVELOPE_ERR_LENGTH);
        return false;
    }
    if ((bytes[SX_OFF_MAGIC] != SX_ENVELOPE_MAGIC_0) ||
        (bytes[SX_OFF_MAGIC + 1u] != SX_ENVELOPE_MAGIC_1)) {
        sx_set_error(error, SX_ENVELOPE_ERR_MAGIC);
        return false;
    }
    if (bytes[SX_OFF_PROTOCOL_VERSION] != SX_PROTOCOL_VERSION) {
        sx_set_error(error, SX_ENVELOPE_ERR_VERSION);
        return false;
    }
    if ((bytes[SX_OFF_MESSAGE_TYPE] != SX_MSG_CMD) &&
        (bytes[SX_OFF_MESSAGE_TYPE] != SX_MSG_POLICY_UPDATE)) {
        sx_set_error(error, SX_ENVELOPE_ERR_MESSAGE_TYPE);
        return false;
    }
    if (memcmp(&bytes[SX_OFF_DEVICE_ID], expected_device_id, SX_SIZE_DEVICE_ID) != 0) {
        sx_set_error(error, SX_ENVELOPE_ERR_DEVICE_ID);
        return false;
    }
    if (bytes[SX_OFF_KEY_ID] != 1u) {
        sx_set_error(error, SX_ENVELOPE_ERR_KEY_ID);
        return false;
    }

    payload_len = sx_read_u16_be(&bytes[SX_OFF_PAYLOAD_LEN]);
    if (payload_len > SX_ENVELOPE_MAX_PAYLOAD) {
        sx_set_error(error, SX_ENVELOPE_ERR_PAYLOAD_LENGTH);
        return false;
    }
    expected_length = (size_t)SX_ENVELOPE_HEADER_SIZE + (size_t)payload_len + SX_ENVELOPE_TAG_SIZE;
    if (length != expected_length) {
        sx_set_error(error, SX_ENVELOPE_ERR_PAYLOAD_LENGTH);
        return false;
    }

    view->message_type = bytes[SX_OFF_MESSAGE_TYPE];
    view->key_id = bytes[SX_OFF_KEY_ID];
    view->sequence = sx_read_u64_be(&bytes[SX_OFF_SEQUENCE]);
    view->message_id = &bytes[SX_OFF_MESSAGE_ID];
    view->operation_code = sx_read_u16_be(&bytes[SX_OFF_OPERATION_CODE]);
    view->payload_len = payload_len;
    view->payload = &bytes[SX_OFF_PAYLOAD];
    view->tag = &bytes[SX_OFF_PAYLOAD + payload_len];
    view->authenticated_bytes = bytes;
    view->authenticated_length = (size_t)SX_ENVELOPE_HEADER_SIZE + payload_len;
    sx_set_error(error, SX_ENVELOPE_OK);
    return true;
}

bool sx_command_decode(const sx_envelope_view_t *view, sx_command_t *command,
                       sx_envelope_error_t *error)
{
    if ((view == NULL) || (command == NULL) || (view->message_type != SX_MSG_CMD)) {
        sx_set_error(error, SX_ENVELOPE_ERR_ARGUMENT);
        return false;
    }

    (void)memset(command, 0, sizeof(*command));
    command->operation_code = view->operation_code;

    switch (view->operation_code) {
    case SX_OP_SET_OUTPUT:
        if ((view->payload_len != 2u) || (view->payload[0] != 1u) || (view->payload[1] > 1u)) {
            sx_set_error(error, SX_ENVELOPE_ERR_COMMAND);
            return false;
        }
        command->selector = view->payload[0];
        command->value = view->payload[1];
        break;

    case SX_OP_SET_LEVEL:
        if ((view->payload_len != 3u) || (view->payload[0] != 1u)) {
            sx_set_error(error, SX_ENVELOPE_ERR_COMMAND);
            return false;
        }
        command->selector = view->payload[0];
        command->value = sx_read_u16_be(&view->payload[1]);
        if (command->value > 1000u) {
            sx_set_error(error, SX_ENVELOPE_ERR_COMMAND);
            return false;
        }
        break;

    case SX_OP_GET_STATUS:
        if (view->payload_len != 0u) {
            sx_set_error(error, SX_ENVELOPE_ERR_COMMAND);
            return false;
        }
        break;

    default:
        sx_set_error(error, SX_ENVELOPE_ERR_COMMAND);
        return false;
    }

    sx_set_error(error, SX_ENVELOPE_OK);
    return true;
}

static bool sx_policy_entry_bounds_valid(uint16_t code, uint16_t min_value, uint16_t max_value)
{
    if (min_value > max_value) {
        return false;
    }
    switch (code) {
    case SX_OP_SET_OUTPUT:
        return max_value <= 1u;
    case SX_OP_SET_LEVEL:
        return max_value <= 1000u;
    case SX_OP_GET_STATUS:
        return (min_value == 0u) && (max_value == 0u);
    default:
        return false;
    }
}

bool sx_policy_v1_decode(const uint8_t *payload, uint16_t payload_len,
                         uint32_t active_policy_version, sx_policy_v1_t *policy,
                         sx_envelope_error_t *error)
{
    uint8_t i;
    uint8_t j;
    uint8_t count;
    uint32_t version;
    uint16_t rate;
    uint8_t flags;
    uint16_t expected_len;

    if ((payload == NULL) || (policy == NULL)) {
        sx_set_error(error, SX_ENVELOPE_ERR_ARGUMENT);
        return false;
    }
    if (payload_len < 15u) {
        sx_set_error(error, SX_ENVELOPE_ERR_POLICY);
        return false;
    }

    version = sx_read_u32_be(&payload[0]);
    rate = sx_read_u16_be(&payload[4]);
    count = payload[6];
    flags = payload[7];

    if ((version <= active_policy_version) || (rate < 1u) || (rate > 20u) ||
        (count < 1u) || (count > 8u) || ((flags & 0xFEu) != 0u)) {
        sx_set_error(error, SX_ENVELOPE_ERR_POLICY);
        return false;
    }

    expected_len = (uint16_t)(8u + (uint16_t)(7u * count));
    if ((payload_len != expected_len) || (payload_len > 64u)) {
        sx_set_error(error, SX_ENVELOPE_ERR_POLICY);
        return false;
    }

    (void)memset(policy, 0, sizeof(*policy));
    policy->version = version;
    policy->max_commands_per_second = rate;
    policy->entry_count = count;
    policy->flags = flags;

    for (i = 0u; i < count; ++i) {
        const uint8_t *entry_bytes = &payload[8u + (uint16_t)(7u * i)];
        sx_policy_entry_t *entry = &policy->entries[i];

        entry->command_code = sx_read_u16_be(&entry_bytes[0]);
        entry->allowed = entry_bytes[2];
        entry->min_value = sx_read_u16_be(&entry_bytes[3]);
        entry->max_value = sx_read_u16_be(&entry_bytes[5]);

        if ((entry->allowed > 1u) ||
            !sx_policy_entry_bounds_valid(entry->command_code, entry->min_value, entry->max_value)) {
            sx_set_error(error, SX_ENVELOPE_ERR_POLICY);
            return false;
        }
        for (j = 0u; j < i; ++j) {
            if (policy->entries[j].command_code == entry->command_code) {
                sx_set_error(error, SX_ENVELOPE_ERR_POLICY);
                return false;
            }
        }
    }

    sx_set_error(error, SX_ENVELOPE_OK);
    return true;
}
