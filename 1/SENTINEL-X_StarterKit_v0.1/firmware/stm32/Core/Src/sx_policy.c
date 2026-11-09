#include "sx_policy.h"
#include "sentinel_x_protocol_v1.h"
#include <string.h>
static uint16_t read_be16(const uint8_t *p) { return (uint16_t)(((uint16_t)p[0] << 8) | p[1]); }
static uint32_t read_be32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
static int range_valid(uint16_t code, uint16_t min, uint16_t max)
{
    if (min > max) return 0;
    switch (code) {
    case SX_OP_SET_OUTPUT: return max <= 1u;
    case SX_OP_SET_LEVEL: return max <= 1000u;
    case SX_OP_GET_STATUS: return (min == 0u && max == 0u);
    default: return 0;
    }
}
sx_proto_status_t sx_policy_parse_validate(const uint8_t *payload, size_t payload_len,
                                           uint32_t active_policy_version, sx_policy_t *out_policy)
{
    sx_policy_t p = {0}; uint8_t count, flags; size_t i, j, offset, expected;
    if ((payload == NULL) || (out_policy == NULL)) return SX_PROTO_ERR_ARGUMENT;
    if ((payload_len < 15u) || (payload_len > SX_POLICY_MAX_SIZE)) return SX_PROTO_ERR_LENGTH;
    p.policy_version = read_be32(&payload[0]);
    p.max_commands_per_second = read_be16(&payload[4]);
    count = payload[6]; flags = payload[7];
    if (p.policy_version <= active_policy_version) return SX_PROTO_ERR_FIELD;
    if ((p.max_commands_per_second < 1u) || (p.max_commands_per_second > 20u)) return SX_PROTO_ERR_FIELD;
    if ((count < 1u) || (count > SX_POLICY_MAX_ENTRIES)) return SX_PROTO_ERR_FIELD;
    if ((flags & (uint8_t)~SX_POLICY_FLAG_FORCE_OFF_ON_LINK_LOSS) != 0u) return SX_PROTO_ERR_FIELD;
    expected = SX_POLICY_HEADER_SIZE + (size_t)count * SX_POLICY_ENTRY_SIZE;
    if (payload_len != expected) return SX_PROTO_ERR_LENGTH;
    p.entry_count = count; p.flags = flags; offset = SX_POLICY_HEADER_SIZE;
    for (i = 0u; i < count; ++i) {
        sx_policy_entry_t *e = &p.entries[i];
        e->command_code = read_be16(&payload[offset]);
        e->allowed = payload[offset + 2u];
        e->min_value = read_be16(&payload[offset + 3u]);
        e->max_value = read_be16(&payload[offset + 5u]);
        if (e->allowed > 1u || !range_valid(e->command_code, e->min_value, e->max_value)) return SX_PROTO_ERR_FIELD;
        for (j = 0u; j < i; ++j) if (p.entries[j].command_code == e->command_code) return SX_PROTO_ERR_FIELD;
        offset += SX_POLICY_ENTRY_SIZE;
    }
    memcpy(out_policy, &p, sizeof(p));
    return SX_PROTO_OK;
}
