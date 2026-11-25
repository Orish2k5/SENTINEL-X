#include "sx_persistence_at24c256.h"

#include <stddef.h>
#include <string.h>

#define SX_SEQ_MAGIC_0 0x53u /* S */
#define SX_SEQ_MAGIC_1 0x58u /* X */
#define SX_SEQ_MAGIC_2 0x4Au /* J */
#define SX_SEQ_MAGIC_3 0x52u /* R */
#define SX_SEQ_FORMAT_VERSION 1u
#define SX_SEQ_STREAM_INBOUND 1u
#define SX_SEQ_STREAM_OUTBOUND 2u
#define SX_RECORD_COMMIT_MARKER 0xA5u
#define SX_RECORD_INVALID_MARKER 0x00u
#define SX_POLICY_MAGIC_0 0x53u /* S */
#define SX_POLICY_MAGIC_1 0x58u /* X */
#define SX_POLICY_MAGIC_2 0x50u /* P */
#define SX_POLICY_MAGIC_3 0x4Cu /* L */
#define SX_POLICY_FORMAT_VERSION 1u
#define SX_POLICY_PAYLOAD_MAX 64u
#define SX_POLICY_PAYLOAD_OFFSET 12u
#define SX_POLICY_CRC_OFFSET 76u
#define SX_POLICY_MARKER_OFFSET 127u
#define SX_SCAN_CHUNK_SIZE 256u

typedef enum {
    SX_SCAN_EMPTY = 0,
    SX_SCAN_VALID,
    SX_SCAN_ERROR
} sx_scan_state_t;

typedef struct {
    sx_scan_state_t state;
    uint64_t value;
    uint32_t generation;
    uint16_t slot;
} sx_scan_result_t;

/*
 * Any EEPROM I/O error is ambiguous: the device may have completed the write
 * even if the MCU failed to observe the ACK/readback. Latch the store faulted
 * until the persistence object is reinitialized and the journals are rescanned.
 */
static bool sx_persistence_read(sx_persistence_at24c256_t *persistence,
                                uint16_t address, uint8_t *data, size_t length)
{
    if ((persistence == NULL) || !persistence->initialized || persistence->faulted) {
        return false;
    }
    if (!sx_at24c256_read(persistence->eeprom, address, data, length)) {
        persistence->faulted = true;
        return false;
    }
    return true;
}

static bool sx_persistence_write(sx_persistence_at24c256_t *persistence,
                                 uint16_t address, const uint8_t *data, size_t length)
{
    if ((persistence == NULL) || !persistence->initialized || persistence->faulted) {
        return false;
    }
    if (!sx_at24c256_write(persistence->eeprom, address, data, length)) {
        persistence->faulted = true;
        return false;
    }
    return true;
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

static void sx_write_u16_be(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value >> 8);
    p[1] = (uint8_t)value;
}

static void sx_write_u32_be(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value >> 24);
    p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8);
    p[3] = (uint8_t)value;
}

static void sx_write_u64_be(uint8_t *p, uint64_t value)
{
    uint8_t i;
    for (i = 0u; i < 8u; ++i) {
        p[i] = (uint8_t)(value >> (56u - (8u * i)));
    }
}

static uint32_t sx_crc32_ieee(const uint8_t *data, size_t length)
{
    uint32_t crc = 0xFFFFFFFFu;
    size_t i;
    uint8_t bit;

    for (i = 0u; i < length; ++i) {
        crc ^= data[i];
        for (bit = 0u; bit < 8u; ++bit) {
            crc = ((crc & 1u) != 0u) ? ((crc >> 1) ^ 0xEDB88320u) : (crc >> 1);
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

static bool sx_generation_is_newer(uint32_t candidate, uint32_t reference)
{
    return (int32_t)(candidate - reference) > 0;
}

static bool sx_generation_is_ambiguous(uint32_t a, uint32_t b)
{
    return (uint32_t)(a - b) == 0x80000000u;
}

static bool sx_all_bytes_are(const uint8_t *data, size_t length, uint8_t value)
{
    size_t i;
    for (i = 0u; i < length; ++i) {
        if (data[i] != value) {
            return false;
        }
    }
    return true;
}

static bool sx_sequence_record_valid(const uint8_t record[SX_PERSISTENCE_SEQUENCE_RECORD_SIZE],
                                     uint8_t stream_id,
                                     uint32_t *generation,
                                     uint64_t *sequence)
{
    uint32_t gen;
    uint64_t seq;

    if ((record[0] != SX_SEQ_MAGIC_0) || (record[1] != SX_SEQ_MAGIC_1) ||
        (record[2] != SX_SEQ_MAGIC_2) || (record[3] != SX_SEQ_MAGIC_3) ||
        (record[4] != SX_SEQ_FORMAT_VERSION) || (record[5] != stream_id) ||
        (record[6] != 0u) || (record[7] != 0u) ||
        (record[28] != 0u) || (record[29] != 0u) || (record[30] != 0u) ||
        (record[31] != SX_RECORD_COMMIT_MARKER)) {
        return false;
    }

    gen = sx_read_u32_be(&record[8]);
    seq = sx_read_u64_be(&record[12]);
    if (sx_read_u32_be(&record[24]) != ~gen) {
        return false;
    }
    if (sx_read_u32_be(&record[20]) != sx_crc32_ieee(record, 20u)) {
        return false;
    }

    *generation = gen;
    *sequence = seq;
    return true;
}

static void sx_sequence_record_build(uint8_t record[SX_PERSISTENCE_SEQUENCE_RECORD_SIZE],
                                     uint8_t stream_id, uint32_t generation,
                                     uint64_t sequence)
{
    (void)memset(record, 0, SX_PERSISTENCE_SEQUENCE_RECORD_SIZE);
    record[0] = SX_SEQ_MAGIC_0;
    record[1] = SX_SEQ_MAGIC_1;
    record[2] = SX_SEQ_MAGIC_2;
    record[3] = SX_SEQ_MAGIC_3;
    record[4] = SX_SEQ_FORMAT_VERSION;
    record[5] = stream_id;
    sx_write_u32_be(&record[8], generation);
    sx_write_u64_be(&record[12], sequence);
    sx_write_u32_be(&record[20], sx_crc32_ieee(record, 20u));
    sx_write_u32_be(&record[24], ~generation);
    record[31] = 0xFFu; /* Commit marker is written separately and last. */
}

static bool sx_scan_sequence_log(sx_persistence_at24c256_t *persistence, uint16_t base,
                                 uint8_t stream_id, sx_scan_result_t *result)
{
    uint8_t chunk[SX_SCAN_CHUNK_SIZE];
    uint8_t record[SX_PERSISTENCE_SEQUENCE_RECORD_SIZE];
    uint16_t slot = 0u;
    bool any_valid = false;
    bool dirty_without_commit = false;
    uint64_t newest_value = 0u;
    uint32_t newest_generation = 0u;
    uint16_t newest_slot = 0u;

    if ((persistence == NULL) || !persistence->initialized ||
        persistence->faulted || (result == NULL)) {
        return false;
    }

    while (slot < SX_PERSISTENCE_SEQUENCE_SLOT_COUNT) {
        uint16_t first_slot = slot;
        uint16_t records_in_chunk = (uint16_t)(SX_SCAN_CHUNK_SIZE /
                                               SX_PERSISTENCE_SEQUENCE_RECORD_SIZE);
        uint16_t i;
        uint16_t read_address = (uint16_t)(base +
            (uint32_t)first_slot * SX_PERSISTENCE_SEQUENCE_RECORD_SIZE);

        if ((uint16_t)(SX_PERSISTENCE_SEQUENCE_SLOT_COUNT - slot) < records_in_chunk) {
            records_in_chunk = (uint16_t)(SX_PERSISTENCE_SEQUENCE_SLOT_COUNT - slot);
        }
        if (!sx_persistence_read(persistence, read_address, chunk,
                                 (size_t)records_in_chunk * SX_PERSISTENCE_SEQUENCE_RECORD_SIZE)) {
            return false;
        }

        for (i = 0u; i < records_in_chunk; ++i) {
            uint32_t candidate_generation;
            uint64_t candidate_value;
            uint16_t current_slot = (uint16_t)(first_slot + i);
            (void)memcpy(record, &chunk[(size_t)i * SX_PERSISTENCE_SEQUENCE_RECORD_SIZE],
                         SX_PERSISTENCE_SEQUENCE_RECORD_SIZE);

            if (record[31] == 0xFFu) {
                if (!sx_all_bytes_are(record, SX_PERSISTENCE_SEQUENCE_RECORD_SIZE, 0xFFu)) {
                    result->state = SX_SCAN_ERROR;
                    return false;
                }
                continue;
            }
            if (record[31] == SX_RECORD_INVALID_MARKER) {
                dirty_without_commit = true;
                continue;
            }
            if (record[31] != SX_RECORD_COMMIT_MARKER) {
                result->state = SX_SCAN_ERROR;
                return false;
            }
            if (!sx_sequence_record_valid(record, stream_id,
                                          &candidate_generation, &candidate_value)) {
                result->state = SX_SCAN_ERROR;
                return false;
            }

            if (!any_valid) {
                any_valid = true;
                newest_generation = candidate_generation;
                newest_value = candidate_value;
                newest_slot = current_slot;
            } else if (candidate_generation == newest_generation) {
                if (candidate_value != newest_value) {
                    result->state = SX_SCAN_ERROR;
                    return false;
                }
            } else if (sx_generation_is_ambiguous(candidate_generation, newest_generation)) {
                result->state = SX_SCAN_ERROR;
                return false;
            } else if (sx_generation_is_newer(candidate_generation, newest_generation)) {
                if (candidate_value <= newest_value) {
                    result->state = SX_SCAN_ERROR;
                    return false;
                }
                newest_generation = candidate_generation;
                newest_value = candidate_value;
                newest_slot = current_slot;
            } else if (candidate_value >= newest_value) {
                result->state = SX_SCAN_ERROR;
                return false;
            }
        }
        slot = (uint16_t)(slot + records_in_chunk);
    }

    if (any_valid) {
        result->state = SX_SCAN_VALID;
        result->value = newest_value;
        result->generation = newest_generation;
        result->slot = newest_slot;
        return true;
    }

    if (dirty_without_commit) {
        result->state = SX_SCAN_ERROR;
        return false;
    }

    result->state = SX_SCAN_EMPTY;
    result->value = 0u;
    result->generation = 0u;
    result->slot = 0u;
    return true;
}

static uint16_t sx_sequence_base_for_stream(uint8_t stream_id)
{
    return (stream_id == SX_SEQ_STREAM_INBOUND)
        ? SX_PERSISTENCE_INBOUND_BASE : SX_PERSISTENCE_OUTBOUND_BASE;
}

static sx_persistence_sequence_state_t *sx_sequence_state_for_stream(
    sx_persistence_at24c256_t *persistence, uint8_t stream_id)
{
    return (stream_id == SX_SEQ_STREAM_INBOUND)
        ? &persistence->inbound : &persistence->outbound;
}

static bool sx_sequence_load_stream(sx_persistence_at24c256_t *persistence,
                                    uint8_t stream_id, uint64_t *high_water_mark)
{
    sx_persistence_sequence_state_t *state;
    sx_scan_result_t scan;

    if ((persistence == NULL) || !persistence->initialized || persistence->faulted ||
        (high_water_mark == NULL)) {
        return false;
    }
    state = sx_sequence_state_for_stream(persistence, stream_id);
    if (!state->loaded) {
        if (!sx_scan_sequence_log(persistence,
                                  sx_sequence_base_for_stream(stream_id),
                                  stream_id, &scan)) {
            persistence->faulted = true;
            return false;
        }
        if (scan.state != SX_SCAN_VALID) {
            return false; /* Blank/unprovisioned is not silently initialized. */
        }
        state->high_water_mark = scan.value;
        state->generation = scan.generation;
        state->current_slot = scan.slot;
        state->present = true;
        state->loaded = true;
    }
    if (!state->present) {
        return false;
    }
    *high_water_mark = state->high_water_mark;
    return true;
}

static bool sx_sequence_append(sx_persistence_at24c256_t *persistence,
                               uint8_t stream_id, uint64_t sequence,
                               bool allow_initial_record)
{
    sx_persistence_sequence_state_t *state;
    uint8_t record[SX_PERSISTENCE_SEQUENCE_RECORD_SIZE];
    uint8_t verify[SX_PERSISTENCE_SEQUENCE_RECORD_SIZE - 1u];
    uint8_t marker;
    uint16_t base;
    uint16_t slot;
    uint16_t address;
    uint32_t generation;

    if ((persistence == NULL) || !persistence->initialized || persistence->faulted) {
        return false;
    }
    state = sx_sequence_state_for_stream(persistence, stream_id);
    base = sx_sequence_base_for_stream(stream_id);

    if (!state->loaded) {
        sx_scan_result_t scan;
        if (!sx_scan_sequence_log(persistence, base, stream_id, &scan)) {
            persistence->faulted = true;
            return false;
        }
        if (scan.state == SX_SCAN_VALID) {
            state->high_water_mark = scan.value;
            state->generation = scan.generation;
            state->current_slot = scan.slot;
            state->present = true;
            state->loaded = true;
        } else if ((scan.state == SX_SCAN_EMPTY) && allow_initial_record) {
            state->high_water_mark = 0u;
            state->generation = 0u;
            state->current_slot = (uint16_t)(SX_PERSISTENCE_SEQUENCE_SLOT_COUNT - 1u);
            state->present = false;
            state->loaded = true;
        } else {
            return false;
        }
    }

    if (state->present) {
        if (sequence <= state->high_water_mark) {
            return false;
        }
        slot = (uint16_t)((state->current_slot + 1u) % SX_PERSISTENCE_SEQUENCE_SLOT_COUNT);
        generation = state->generation + 1u;
    } else {
        if (!allow_initial_record) {
            return false;
        }
        slot = 0u;
        generation = 1u;
    }

    address = (uint16_t)(base + (uint32_t)slot * SX_PERSISTENCE_SEQUENCE_RECORD_SIZE);
    sx_sequence_record_build(record, stream_id, generation, sequence);

    /* Invalidate the old slot first. The previous newest record is never overwritten. */
    marker = SX_RECORD_INVALID_MARKER;
    if (!sx_persistence_write(persistence,
                           (uint16_t)(address + SX_PERSISTENCE_SEQUENCE_RECORD_SIZE - 1u),
                           &marker, 1u)) {
        return false;
    }
    if (!sx_persistence_write(persistence, address, record,
                           SX_PERSISTENCE_SEQUENCE_RECORD_SIZE - 1u)) {
        return false;
    }
    if (!sx_persistence_read(persistence, address, verify, sizeof(verify)) ||
        (memcmp(verify, record, sizeof(verify)) != 0)) {
        return false;
    }

    marker = SX_RECORD_COMMIT_MARKER;
    if (!sx_persistence_write(persistence,
                           (uint16_t)(address + SX_PERSISTENCE_SEQUENCE_RECORD_SIZE - 1u),
                           &marker, 1u)) {
        return false;
    }
    {
        uint8_t marker_readback = 0u;
        if (!sx_persistence_read(persistence,
                              (uint16_t)(address + SX_PERSISTENCE_SEQUENCE_RECORD_SIZE - 1u),
                              &marker_readback, 1u) ||
            (marker_readback != SX_RECORD_COMMIT_MARKER)) {
            return false;
        }
    }

    state->high_water_mark = sequence;
    state->generation = generation;
    state->current_slot = slot;
    state->present = true;
    state->loaded = true;
    return true;
}

static bool sx_policy_payload_encode(const sx_policy_v1_t *policy,
                                     uint8_t payload[SX_POLICY_PAYLOAD_MAX],
                                     uint16_t *payload_length)
{
    uint8_t i;
    uint8_t j;
    uint16_t length;

    if ((policy == NULL) || (payload == NULL) || (payload_length == NULL) ||
        (policy->version == 0u) ||
        (policy->max_commands_per_second < 1u) ||
        (policy->max_commands_per_second > 20u) ||
        (policy->entry_count < 1u) || (policy->entry_count > 8u) ||
        ((policy->flags & 0xFEu) != 0u)) {
        return false;
    }

    length = (uint16_t)(8u + (uint16_t)(7u * policy->entry_count));
    if (length > SX_POLICY_PAYLOAD_MAX) {
        return false;
    }

    (void)memset(payload, 0, SX_POLICY_PAYLOAD_MAX);
    sx_write_u32_be(&payload[0], policy->version);
    sx_write_u16_be(&payload[4], policy->max_commands_per_second);
    payload[6] = policy->entry_count;
    payload[7] = policy->flags;

    for (i = 0u; i < policy->entry_count; ++i) {
        const sx_policy_entry_t *entry = &policy->entries[i];
        uint8_t *out = &payload[8u + (uint16_t)(7u * i)];
        if ((entry->allowed > 1u) || (entry->min_value > entry->max_value)) {
            return false;
        }
        switch (entry->command_code) {
        case SX_OP_SET_OUTPUT:
            if (entry->max_value > 1u) return false;
            break;
        case SX_OP_SET_LEVEL:
            if (entry->max_value > 1000u) return false;
            break;
        case SX_OP_GET_STATUS:
            if ((entry->min_value != 0u) || (entry->max_value != 0u)) return false;
            break;
        default:
            return false;
        }
        for (j = 0u; j < i; ++j) {
            if (policy->entries[j].command_code == entry->command_code) return false;
        }
        sx_write_u16_be(&out[0], entry->command_code);
        out[2] = entry->allowed;
        sx_write_u16_be(&out[3], entry->min_value);
        sx_write_u16_be(&out[5], entry->max_value);
    }

    *payload_length = length;
    return true;
}

static bool sx_policy_record_valid(const uint8_t record[SX_PERSISTENCE_POLICY_SLOT_SIZE],
                                   uint32_t *generation, sx_policy_v1_t *policy)
{
    uint16_t payload_length;
    sx_envelope_error_t error;
    uint8_t i;

    if ((record[0] != SX_POLICY_MAGIC_0) || (record[1] != SX_POLICY_MAGIC_1) ||
        (record[2] != SX_POLICY_MAGIC_2) || (record[3] != SX_POLICY_MAGIC_3) ||
        (record[4] != SX_POLICY_FORMAT_VERSION) || (record[5] != 0u) ||
        (record[SX_POLICY_MARKER_OFFSET] != SX_RECORD_COMMIT_MARKER)) {
        return false;
    }
    payload_length = (uint16_t)(((uint16_t)record[6] << 8) | record[7]);
    if ((payload_length < 15u) || (payload_length > SX_POLICY_PAYLOAD_MAX) ||
        (sx_read_u32_be(&record[SX_POLICY_CRC_OFFSET]) !=
         sx_crc32_ieee(record, SX_POLICY_CRC_OFFSET))) {
        return false;
    }
    for (i = (uint8_t)(SX_POLICY_PAYLOAD_OFFSET + payload_length);
         i < (uint8_t)(SX_POLICY_CRC_OFFSET); ++i) {
        if (record[i] != 0u) return false;
    }
    if (!sx_policy_v1_decode(&record[SX_POLICY_PAYLOAD_OFFSET], payload_length,
                             0u, policy, &error)) {
        return false;
    }
    *generation = sx_read_u32_be(&record[8]);
    if (*generation == 0u) return false;
    for (i = (uint8_t)(SX_POLICY_CRC_OFFSET + 4u);
         i < (uint8_t)SX_POLICY_MARKER_OFFSET; ++i) {
        if (record[i] != 0u) return false;
    }
    return true;
}

static bool sx_scan_policy(sx_persistence_at24c256_t *persistence,
                           bool *present, uint8_t *active_slot,
                           uint32_t *generation, sx_policy_v1_t *policy,
                           bool *blank)
{
    uint8_t record_a[SX_PERSISTENCE_POLICY_SLOT_SIZE];
    uint8_t record_b[SX_PERSISTENCE_POLICY_SLOT_SIZE];
    uint32_t generation_a = 0u;
    uint32_t generation_b = 0u;
    sx_policy_v1_t policy_a;
    sx_policy_v1_t policy_b;
    bool valid_a;
    bool valid_b;
    bool dirty_a;
    bool dirty_b;

    if ((persistence == NULL) || !persistence->initialized || (present == NULL) ||
        (active_slot == NULL) || (generation == NULL) || (policy == NULL) || (blank == NULL)) {
        return false;
    }
    if (!sx_persistence_read(persistence, SX_PERSISTENCE_POLICY_SLOT_A,
                          record_a, sizeof(record_a)) ||
        !sx_persistence_read(persistence, SX_PERSISTENCE_POLICY_SLOT_B,
                          record_b, sizeof(record_b))) {
        return false;
    }

    valid_a = record_a[SX_POLICY_MARKER_OFFSET] == SX_RECORD_COMMIT_MARKER;
    valid_b = record_b[SX_POLICY_MARKER_OFFSET] == SX_RECORD_COMMIT_MARKER;
    dirty_a = record_a[SX_POLICY_MARKER_OFFSET] == SX_RECORD_INVALID_MARKER;
    dirty_b = record_b[SX_POLICY_MARKER_OFFSET] == SX_RECORD_INVALID_MARKER;

    if ((record_a[SX_POLICY_MARKER_OFFSET] != SX_RECORD_COMMIT_MARKER) &&
        (record_a[SX_POLICY_MARKER_OFFSET] != SX_RECORD_INVALID_MARKER) &&
        !sx_all_bytes_are(record_a, sizeof(record_a), 0xFFu)) return false;
    if ((record_b[SX_POLICY_MARKER_OFFSET] != SX_RECORD_COMMIT_MARKER) &&
        (record_b[SX_POLICY_MARKER_OFFSET] != SX_RECORD_INVALID_MARKER) &&
        !sx_all_bytes_are(record_b, sizeof(record_b), 0xFFu)) return false;

    if (valid_a && !sx_policy_record_valid(record_a, &generation_a, &policy_a)) return false;
    if (valid_b && !sx_policy_record_valid(record_b, &generation_b, &policy_b)) return false;

    if (valid_a && valid_b) {
        if (generation_a == generation_b || sx_generation_is_ambiguous(generation_a, generation_b)) {
            return false;
        }
        if (sx_generation_is_newer(generation_a, generation_b)) {
            if (policy_a.version <= policy_b.version) return false;
            *present = true; *active_slot = 0u; *generation = generation_a; *policy = policy_a;
        } else {
            if (policy_b.version <= policy_a.version) return false;
            *present = true; *active_slot = 1u; *generation = generation_b; *policy = policy_b;
        }
        *blank = false;
        return true;
    }
    if (valid_a) {
        *present = true; *active_slot = 0u; *generation = generation_a; *policy = policy_a;
        *blank = false;
        return true;
    }
    if (valid_b) {
        *present = true; *active_slot = 1u; *generation = generation_b; *policy = policy_b;
        *blank = false;
        return true;
    }

    if (dirty_a || dirty_b) return false;
    *present = false;
    *active_slot = 0u;
    *generation = 0u;
    (void)memset(policy, 0, sizeof(*policy));
    *blank = true;
    return true;
}

static bool sx_policy_load_internal(sx_persistence_at24c256_t *persistence,
                                    sx_policy_v1_t *policy, bool *blank)
{
    bool present;
    uint8_t slot;
    uint32_t generation;
    sx_policy_v1_t loaded;

    if (!sx_scan_policy(persistence, &present, &slot, &generation, &loaded, blank)) {
        persistence->faulted = true;
        return false;
    }
    persistence->policy_loaded = true;
    persistence->policy_present = present;
    persistence->policy_active_slot = slot;
    persistence->policy_generation = generation;
    if (present) persistence->active_policy = loaded;
    if (policy != NULL && present) *policy = loaded;
    return present;
}

bool sx_persistence_at24c256_init(sx_persistence_at24c256_t *persistence,
                                  sx_at24c256_t *eeprom)
{
    if ((persistence == NULL) || (eeprom == NULL) || !eeprom->initialized) {
        return false;
    }
    (void)memset(persistence, 0, sizeof(*persistence));
    persistence->eeprom = eeprom;
    persistence->initialized = true;
    return true;
}

bool sx_persistence_at24c256_provision_blank(sx_persistence_at24c256_t *persistence,
                                              uint64_t initial_inbound_sequence,
                                              uint64_t initial_outbound_sequence)
{
    sx_scan_result_t inbound_scan;
    sx_scan_result_t outbound_scan;
    bool policy_present;
    bool policy_blank;
    uint8_t policy_slot;
    uint32_t policy_generation;
    sx_policy_v1_t policy;

    if ((persistence == NULL) || !persistence->initialized || persistence->faulted) return false;
    if (!sx_scan_sequence_log(persistence, SX_PERSISTENCE_INBOUND_BASE,
                              SX_SEQ_STREAM_INBOUND, &inbound_scan) ||
        !sx_scan_sequence_log(persistence, SX_PERSISTENCE_OUTBOUND_BASE,
                              SX_SEQ_STREAM_OUTBOUND, &outbound_scan) ||
        !sx_scan_policy(persistence, &policy_present, &policy_slot,
                       &policy_generation, &policy, &policy_blank)) {
        persistence->faulted = true;
        return false;
    }
    if ((inbound_scan.state != SX_SCAN_EMPTY) || (outbound_scan.state != SX_SCAN_EMPTY) ||
        !policy_blank || policy_present) {
        return false;
    }

    persistence->inbound.loaded = true;
    persistence->inbound.present = false;
    persistence->outbound.loaded = true;
    persistence->outbound.present = false;

    if (!sx_sequence_append(persistence, SX_SEQ_STREAM_INBOUND,
                            initial_inbound_sequence, true)) {
        return false;
    }
    if (!sx_sequence_append(persistence, SX_SEQ_STREAM_OUTBOUND,
                            initial_outbound_sequence, true)) {
        return false;
    }
    return true;
}

bool sx_persistence_inbound_sequence_load(void *context, uint64_t *high_water_mark)
{
    return sx_sequence_load_stream((sx_persistence_at24c256_t *)context,
                                   SX_SEQ_STREAM_INBOUND, high_water_mark);
}

bool sx_persistence_inbound_sequence_commit(void *context, uint64_t high_water_mark)
{
    return sx_sequence_append((sx_persistence_at24c256_t *)context,
                              SX_SEQ_STREAM_INBOUND, high_water_mark, false);
}

bool sx_persistence_outbound_sequence_load(void *context, uint64_t *high_water_mark)
{
    return sx_sequence_load_stream((sx_persistence_at24c256_t *)context,
                                   SX_SEQ_STREAM_OUTBOUND, high_water_mark);
}

bool sx_persistence_outbound_sequence_commit(void *context, uint64_t high_water_mark)
{
    return sx_sequence_append((sx_persistence_at24c256_t *)context,
                              SX_SEQ_STREAM_OUTBOUND, high_water_mark, false);
}

bool sx_persistence_policy_load(void *context, sx_policy_v1_t *policy)
{
    sx_persistence_at24c256_t *persistence = (sx_persistence_at24c256_t *)context;
    bool blank;
    if ((persistence == NULL) || !persistence->initialized || persistence->faulted ||
        (policy == NULL)) return false;
    if (persistence->policy_loaded) {
        if (!persistence->policy_present) return false;
        *policy = persistence->active_policy;
        return true;
    }
    return sx_policy_load_internal(persistence, policy, &blank);
}

bool sx_persistence_policy_commit(void *context, const sx_policy_v1_t *policy)
{
    sx_persistence_at24c256_t *persistence = (sx_persistence_at24c256_t *)context;
    uint8_t payload[SX_POLICY_PAYLOAD_MAX];
    uint16_t payload_length;
    uint8_t record[SX_PERSISTENCE_POLICY_SLOT_SIZE];
    uint8_t verify[SX_PERSISTENCE_POLICY_SLOT_SIZE - 1u];
    uint8_t marker;
    uint8_t target_slot;
    uint16_t target_address;
    uint32_t generation;
    bool blank = false;
    sx_policy_v1_t current;

    if ((persistence == NULL) || !persistence->initialized || persistence->faulted ||
        (policy == NULL)) return false;

    if (!persistence->policy_loaded) {
        bool current_present = sx_policy_load_internal(persistence, &current, &blank);
        if (!current_present && (persistence->faulted || !blank)) return false;
    }
    if (persistence->policy_present && policy->version <= persistence->active_policy.version) {
        return false;
    }
    if (!sx_policy_payload_encode(policy, payload, &payload_length)) return false;

    target_slot = persistence->policy_present
        ? (uint8_t)(1u - persistence->policy_active_slot) : 0u;
    target_address = (target_slot == 0u)
        ? SX_PERSISTENCE_POLICY_SLOT_A : SX_PERSISTENCE_POLICY_SLOT_B;
    generation = persistence->policy_present ? (persistence->policy_generation + 1u) : 1u;

    (void)memset(record, 0, sizeof(record));
    record[0] = SX_POLICY_MAGIC_0;
    record[1] = SX_POLICY_MAGIC_1;
    record[2] = SX_POLICY_MAGIC_2;
    record[3] = SX_POLICY_MAGIC_3;
    record[4] = SX_POLICY_FORMAT_VERSION;
    record[5] = 0u;
    sx_write_u16_be(&record[6], payload_length);
    sx_write_u32_be(&record[8], generation);
    (void)memcpy(&record[SX_POLICY_PAYLOAD_OFFSET], payload, payload_length);
    sx_write_u32_be(&record[SX_POLICY_CRC_OFFSET], sx_crc32_ieee(record, SX_POLICY_CRC_OFFSET));
    record[SX_POLICY_MARKER_OFFSET] = 0xFFu;

    marker = SX_RECORD_INVALID_MARKER;
    if (!sx_persistence_write(persistence,
                           (uint16_t)(target_address + SX_POLICY_MARKER_OFFSET),
                           &marker, 1u) ||
        !sx_persistence_write(persistence, target_address, record,
                           SX_POLICY_MARKER_OFFSET) ||
        !sx_persistence_read(persistence, target_address, verify, sizeof(verify)) ||
        (memcmp(verify, record, sizeof(verify)) != 0)) {
        return false;
    }

    marker = SX_RECORD_COMMIT_MARKER;
    if (!sx_persistence_write(persistence,
                           (uint16_t)(target_address + SX_POLICY_MARKER_OFFSET),
                           &marker, 1u)) {
        return false;
    }
    {
        uint8_t marker_readback = 0u;
        if (!sx_persistence_read(persistence,
                              (uint16_t)(target_address + SX_POLICY_MARKER_OFFSET),
                              &marker_readback, 1u) ||
            (marker_readback != SX_RECORD_COMMIT_MARKER)) {
            return false;
        }
    }

    persistence->active_policy = *policy;
    persistence->policy_generation = generation;
    persistence->policy_active_slot = target_slot;
    persistence->policy_present = true;
    persistence->policy_loaded = true;
    return true;
}

bool sx_persistence_at24c256_is_faulted(const sx_persistence_at24c256_t *persistence)
{
    return (persistence == NULL) || persistence->faulted;
}

void sx_persistence_at24c256_bind_app(sx_persistence_at24c256_t *persistence,
                                      sx_sequence_store_t *sequence_store,
                                      sx_policy_store_t *policy_store)
{
    if ((persistence == NULL) || (sequence_store == NULL) || (policy_store == NULL)) return;

    sequence_store->context = persistence;
    sequence_store->load = sx_persistence_inbound_sequence_load;
    sequence_store->commit = sx_persistence_inbound_sequence_commit;

    policy_store->context = persistence;
    policy_store->load = sx_persistence_policy_load;
    policy_store->commit = sx_persistence_policy_commit;
}
