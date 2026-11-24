#include "sx_app_core.h"
#include "sx_envelope.h"
#include "sx_replay_guard.h"
#include "sx_uart_frame.h"

#include <stdio.h>
#include <string.h>

static unsigned int tests_run;
static unsigned int tests_failed;
static int mock_hmac_is_valid = 1;
static bool mock_rate_is_allowed = true;

#define CHECK(condition) do { \
    tests_run++; \
    if (!(condition)) { \
        tests_failed++; \
        (void)printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    } \
} while (0)

typedef struct {
    uint64_t high_water;
    bool load_ok;
    bool fail_commit;
    unsigned int commit_calls;
} mock_sequence_store_t;

static bool mock_sequence_load(void *context, uint64_t *high_water)
{
    mock_sequence_store_t *store = (mock_sequence_store_t *)context;
    if ((store == NULL) || (high_water == NULL) || !store->load_ok) {
        return false;
    }
    *high_water = store->high_water;
    return true;
}

static bool mock_sequence_commit(void *context, uint64_t new_high_water)
{
    mock_sequence_store_t *store = (mock_sequence_store_t *)context;
    if (store == NULL) {
        return false;
    }
    store->commit_calls++;
    if (store->fail_commit || (new_high_water <= store->high_water)) {
        return false;
    }
    store->high_water = new_high_water;
    return true;
}

typedef struct {
    sx_policy_v1_t policy;
    bool load_ok;
    bool fail_commit;
    unsigned int commit_calls;
} mock_policy_store_t;

static bool mock_policy_load(void *context, sx_policy_v1_t *policy)
{
    mock_policy_store_t *store = (mock_policy_store_t *)context;
    if ((store == NULL) || (policy == NULL) || !store->load_ok) {
        return false;
    }
    *policy = store->policy;
    return true;
}

static bool mock_policy_commit(void *context, const sx_policy_v1_t *policy)
{
    mock_policy_store_t *store = (mock_policy_store_t *)context;
    if ((store == NULL) || (policy == NULL)) {
        return false;
    }
    store->commit_calls++;
    if (store->fail_commit) {
        return false;
    }
    store->policy = *policy;
    store->load_ok = true;
    return true;
}

static bool mock_key_get(void *context, uint8_t key_id, uint8_t key_out[SX_DEVICE_KEY_SIZE])
{
    size_t i;
    (void)context;
    if ((key_id != 1u) || (key_out == NULL)) {
        return false;
    }
    for (i = 0u; i < SX_DEVICE_KEY_SIZE; ++i) {
        key_out[i] = (uint8_t)i;
    }
    return true;
}

static int mock_hmac_verify(void *context, const uint8_t key[SX_DEVICE_KEY_SIZE],
                            const uint8_t *data, size_t data_len, const uint8_t tag[32])
{
    (void)context;
    (void)key;
    (void)data;
    (void)data_len;
    (void)tag;
    return mock_hmac_is_valid; /* Test double only; Python validates the actual vector. */
}

static bool mock_rate_allow(void *context, uint32_t now_ms, uint16_t max_per_second)
{
    (void)context;
    (void)now_ms;
    return mock_rate_is_allowed && (max_per_second >= 1u);
}

typedef struct {
    mock_sequence_store_t *sequence_store;
    unsigned int off_calls;
    unsigned int set_calls;
    uint16_t last_level;
    bool sequence_was_committed_before_output;
} mock_output_t;

static void mock_output_off(void *context)
{
    mock_output_t *output = (mock_output_t *)context;
    if (output != NULL) {
        output->off_calls++;
    }
}

static void mock_output_set(void *context, uint16_t level_permille)
{
    mock_output_t *output = (mock_output_t *)context;
    if (output != NULL) {
        output->set_calls++;
        output->last_level = level_permille;
        output->sequence_was_committed_before_output =
            (output->sequence_store != NULL) && (output->sequence_store->high_water > 0u);
    }
}

static size_t build_envelope(uint8_t *out, uint8_t message_type, uint64_t sequence,
                             uint16_t operation_code, const uint8_t *payload,
                             uint16_t payload_len)
{
    static const uint8_t message_id[16] = {
        0x12u, 0x3Eu, 0x45u, 0x67u, 0xE8u, 0x9Bu, 0x12u, 0xD3u,
        0xA4u, 0x56u, 0x42u, 0x66u, 0x14u, 0x17u, 0x40u, 0x00u
    };
    const uint8_t device_id[6] = {'S', 'X', '-', '0', '0', '1'};
    size_t offset;
    uint8_t i;

    (void)memset(out, 0, SX_ENVELOPE_MAX_SIZE);
    out[0] = SX_ENVELOPE_MAGIC_0;
    out[1] = SX_ENVELOPE_MAGIC_1;
    out[2] = SX_PROTOCOL_VERSION;
    out[3] = message_type;
    (void)memcpy(&out[4], device_id, sizeof(device_id));
    out[20] = 1u;
    for (i = 0u; i < 8u; ++i) {
        out[21u + i] = (uint8_t)(sequence >> (56u - (8u * i)));
    }
    (void)memcpy(&out[29], message_id, sizeof(message_id));
    out[45] = (uint8_t)(operation_code >> 8);
    out[46] = (uint8_t)operation_code;
    out[47] = (uint8_t)(payload_len >> 8);
    out[48] = (uint8_t)payload_len;
    if ((payload != NULL) && (payload_len > 0u)) {
        (void)memcpy(&out[49], payload, payload_len);
    }
    offset = 49u + payload_len;
    /* Test double ignores this tag. It is non-secret placeholder test data. */
    (void)memset(&out[offset], 0xA5, 32u);
    return offset + 32u;
}

static void init_mock_policy(mock_policy_store_t *store)
{
    (void)memset(store, 0, sizeof(*store));
    store->load_ok = true;
    store->policy.version = 1u;
    store->policy.max_commands_per_second = 5u;
    store->policy.entry_count = 2u;
    store->policy.flags = 0u;
    store->policy.entries[0].command_code = SX_OP_SET_OUTPUT;
    store->policy.entries[0].allowed = 1u;
    store->policy.entries[0].min_value = 0u;
    store->policy.entries[0].max_value = 1u;
    store->policy.entries[1].command_code = SX_OP_SET_LEVEL;
    store->policy.entries[1].allowed = 1u;
    store->policy.entries[1].min_value = 0u;
    store->policy.entries[1].max_value = 1000u;
}

static bool init_app(sx_app_core_t *app, mock_sequence_store_t *sequence,
                     mock_policy_store_t *policy, mock_output_t *output,
                     bool with_policy, bool with_rate_limit)
{
    uint8_t device_id[SX_SIZE_DEVICE_ID];
    sx_sequence_store_t sequence_iface;
    sx_key_provider_t key_provider;
    sx_crypto_provider_t crypto_provider;
    sx_policy_store_t policy_iface;
    sx_output_interface_t output_iface;

    sx_device_id_make(device_id, "SX-001");
    sequence_iface.context = sequence;
    sequence_iface.load = mock_sequence_load;
    sequence_iface.commit = mock_sequence_commit;
    key_provider.context = NULL;
    key_provider.get_key = mock_key_get;
    crypto_provider.context = NULL;
    crypto_provider.verify = mock_hmac_verify;
    policy_iface.context = policy;
    policy_iface.load = with_policy ? mock_policy_load : NULL;
    policy_iface.commit = with_policy ? mock_policy_commit : NULL;
    output_iface.context = output;
    output_iface.force_off = mock_output_off;
    output_iface.set_level_permille = mock_output_set;
    output->sequence_store = sequence;

    return sx_app_core_init(app, device_id, &sequence_iface, &key_provider,
                            &crypto_provider, with_policy ? &policy_iface : NULL,
                            with_rate_limit ? mock_rate_allow : NULL, NULL, &output_iface);
}

static void test_crc_and_uart_parser(void)
{
    static const uint8_t check[] = "123456789";
    uint8_t raw[2u + 1u + 2u + 81u + 2u];
    uint16_t crc;
    size_t i;
    sx_uart_parser_t parser;
    sx_uart_frame_t frame;
    sx_uart_parse_result_t result = SX_UART_PARSE_NONE;

    CHECK(sx_crc16_ccitt_false(check, 9u) == 0x29B1u);

    raw[0] = SX_UART_SOF_0;
    raw[1] = SX_UART_SOF_1;
    raw[2] = SX_UART_FRAME_AUTH_ENVELOPE;
    raw[3] = 0u;
    raw[4] = 81u;
    (void)memset(&raw[5], 0x5Au, 81u);
    crc = sx_crc16_ccitt_false(&raw[2], 84u);
    raw[86] = (uint8_t)(crc >> 8);
    raw[87] = (uint8_t)crc;

    sx_uart_parser_init(&parser);
    for (i = 0u; i < sizeof(raw); ++i) {
        result = sx_uart_parser_feed(&parser, raw[i], &frame);
    }
    CHECK(result == SX_UART_PARSE_COMPLETE);
    CHECK(frame.frame_type == SX_UART_FRAME_AUTH_ENVELOPE);
    CHECK(frame.payload_len == 81u);
    CHECK(frame.payload[0] == 0x5Au);

    /* An AUTH_ENVELOPE shorter than the protocol minimum must be rejected. */
    sx_uart_parser_init(&parser);
    (void)sx_uart_parser_feed(&parser, SX_UART_SOF_0, &frame);
    (void)sx_uart_parser_feed(&parser, SX_UART_SOF_1, &frame);
    (void)sx_uart_parser_feed(&parser, SX_UART_FRAME_AUTH_ENVELOPE, &frame);
    (void)sx_uart_parser_feed(&parser, 0u, &frame);
    result = sx_uart_parser_feed(&parser, 80u, &frame);
    CHECK(result == SX_UART_PARSE_ERROR);
    CHECK(parser.error_count == 1u);
}

static void test_envelope_and_command(void)
{
    static const uint8_t vector[] = {
        0x53,0x58,0x01,0x01,0x53,0x58,0x2d,0x30,0x30,0x31,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x12,0x3e,0x45,
        0x67,0xe8,0x9b,0x12,0xd3,0xa4,0x56,0x42,0x66,0x14,0x17,0x40,0x00,0x00,0x01,0x00,
        0x02,0x01,0x01,0x96,0xa0,0x90,0x46,0xcc,0x1f,0x69,0xb8,0x77,0xc9,0x7d,0xf4,0xea,
        0x4b,0x58,0xba,0xb8,0x9e,0xae,0x59,0x56,0x0a,0x4b,0x92,0xff,0xfd,0x37,0x4f,0x91,
        0xe5,0x70,0xea
    };
    uint8_t device_id[SX_SIZE_DEVICE_ID];
    sx_envelope_view_t view;
    sx_envelope_error_t error;
    sx_command_t command;

    sx_device_id_make(device_id, "SX-001");
    CHECK(sizeof(vector) == 83u);
    CHECK(sx_envelope_parse(vector, sizeof(vector), device_id, &view, &error));
    CHECK(view.sequence == 1u);
    CHECK(view.operation_code == SX_OP_SET_OUTPUT);
    CHECK(view.payload_len == 2u);
    CHECK(sx_command_decode(&view, &command, &error));
    CHECK(command.selector == 1u);
    CHECK(command.value == 1u);

    device_id[0] = 'X';
    CHECK(!sx_envelope_parse(vector, sizeof(vector), device_id, &view, &error));
    CHECK(error == SX_ENVELOPE_ERR_DEVICE_ID);
}

static void test_policy_validation(void)
{
    const uint8_t valid_policy[] = {
        0x00,0x00,0x00,0x02, /* policy_version = 2 */
        0x00,0x05,           /* max commands/sec = 5 */
        0x02,0x00,           /* two entries, flags 0 */
        0x00,0x01,0x01,0x00,0x00,0x00,0x01, /* SET_OUTPUT allow 0..1 */
        0x00,0x02,0x01,0x00,0x00,0x03,0xE8  /* SET_LEVEL allow 0..1000 */
    };
    uint8_t duplicate[sizeof(valid_policy)];
    sx_policy_v1_t policy;
    sx_envelope_error_t error;

    CHECK(sx_policy_v1_decode(valid_policy, (uint16_t)sizeof(valid_policy), 1u, &policy, &error));
    CHECK(policy.version == 2u);
    CHECK(policy.entry_count == 2u);
    CHECK(!sx_policy_v1_decode(valid_policy, (uint16_t)sizeof(valid_policy), 2u, &policy, &error));

    (void)memcpy(duplicate, valid_policy, sizeof(duplicate));
    duplicate[15] = duplicate[8];
    duplicate[16] = duplicate[9];
    CHECK(!sx_policy_v1_decode(duplicate, (uint16_t)sizeof(duplicate), 1u, &policy, &error));
}

static void test_replay_guard_commit_order_and_fail_closed(void)
{
    mock_sequence_store_t memory = {0u, true, false, 0u};
    sx_sequence_store_t iface = {&memory, mock_sequence_load, mock_sequence_commit};
    sx_replay_guard_t guard;

    CHECK(sx_replay_guard_init(&guard, &iface));
    CHECK(sx_replay_guard_consume(&guard, 3u) == SX_REPLAY_ACCEPTED);
    CHECK(memory.high_water == 3u);
    CHECK(sx_replay_guard_consume(&guard, 3u) == SX_REPLAY_REJECTED);
    memory.fail_commit = true;
    CHECK(sx_replay_guard_consume(&guard, 4u) == SX_REPLAY_STORAGE_ERROR);
    CHECK(!guard.ready);
    CHECK(sx_replay_guard_consume(&guard, 5u) == SX_REPLAY_STORAGE_ERROR);
}

static void test_app_execution_after_sequence_commit(void)
{
    sx_app_core_t app;
    mock_sequence_store_t sequence = {0u, true, false, 0u};
    mock_policy_store_t policy;
    mock_output_t output = {0};
    uint8_t link_status[7] = {1u, 1u, 0u, 0u, 0u, 0u, 1u};
    uint8_t payload[2] = {1u, 1u};
    uint8_t envelope[SX_ENVELOPE_MAX_SIZE];
    size_t envelope_len;
    sx_app_result_t result;

    init_mock_policy(&policy);
    CHECK(init_app(&app, &sequence, &policy, &output, true, true));
    sx_app_process_link_status(&app, link_status, 10u);
    envelope_len = build_envelope(envelope, SX_MSG_CMD, 1u, SX_OP_SET_OUTPUT, payload, 2u);
    result = sx_app_process_envelope(&app, envelope, envelope_len, 20u);
    CHECK(result.result_code == SX_APP_RESULT_SUCCESS);
    CHECK(sequence.high_water == 1u);
    CHECK(output.set_calls == 1u);
    CHECK(output.last_level == 1000u);
    CHECK(output.sequence_was_committed_before_output);

    result = sx_app_process_envelope(&app, envelope, envelope_len, 30u);
    CHECK(result.result_code == SX_APP_RESULT_REPLAY);
    CHECK(output.set_calls == 1u);

    sx_app_process_link_status(&app, (const uint8_t[7]){0u,1u,0u,0u,0u,0u,2u}, 40u);
    CHECK(output.off_calls > 0u);
    CHECK(app.output_state == 0u);
}

static void test_missing_policy_denies_actuation_but_allows_status(void)
{
    sx_app_core_t app;
    mock_sequence_store_t sequence = {0u, true, false, 0u};
    mock_policy_store_t policy;
    mock_output_t output = {0};
    uint8_t link_status[7] = {1u,1u,0u,0u,0u,0u,1u};
    uint8_t set_payload[2] = {1u,1u};
    uint8_t envelope[SX_ENVELOPE_MAX_SIZE];
    size_t length;
    sx_app_result_t result;

    (void)memset(&policy, 0, sizeof(policy));
    CHECK(init_app(&app, &sequence, &policy, &output, false, true));
    sx_app_process_link_status(&app, link_status, 10u);

    length = build_envelope(envelope, SX_MSG_CMD, 1u, SX_OP_SET_OUTPUT, set_payload, 2u);
    result = sx_app_process_envelope(&app, envelope, length, 20u);
    CHECK(result.result_code == SX_APP_RESULT_SAFE_MODE);
    CHECK(sequence.high_water == 1u); /* authenticated/schema-valid denied command still consumes sequence */
    CHECK(output.set_calls == 0u);

    length = build_envelope(envelope, SX_MSG_CMD, 2u, SX_OP_GET_STATUS, NULL, 0u);
    result = sx_app_process_envelope(&app, envelope, length, 30u);
    CHECK(result.result_code == SX_APP_RESULT_SUCCESS);
    CHECK(output.set_calls == 0u);
}

static void test_hmac_failure_and_rate_limit_do_not_actuate(void)
{
    sx_app_core_t app;
    mock_sequence_store_t sequence = {0u, true, false, 0u};
    mock_policy_store_t policy;
    mock_output_t output = {0};
    uint8_t payload[2] = {1u, 1u};
    uint8_t envelope[SX_ENVELOPE_MAX_SIZE];
    size_t length;
    sx_app_result_t result;

    init_mock_policy(&policy);
    mock_hmac_is_valid = 0;
    mock_rate_is_allowed = true;
    CHECK(init_app(&app, &sequence, &policy, &output, true, true));
    sx_app_process_link_status(&app, (const uint8_t[7]){1u,1u,0u,0u,0u,0u,1u}, 10u);
    length = build_envelope(envelope, SX_MSG_CMD, 1u, SX_OP_SET_OUTPUT, payload, 2u);
    result = sx_app_process_envelope(&app, envelope, length, 20u);
    CHECK(result.result_code == SX_APP_RESULT_HMAC_INVALID);
    CHECK(sequence.high_water == 0u);
    CHECK(sequence.commit_calls == 0u);
    CHECK(output.set_calls == 0u);

    mock_hmac_is_valid = 1;
    sequence.high_water = 0u;
    sequence.commit_calls = 0u;
    CHECK(init_app(&app, &sequence, &policy, &output, true, true));
    sx_app_process_link_status(&app, (const uint8_t[7]){1u,1u,0u,0u,0u,0u,1u}, 10u);
    mock_rate_is_allowed = false;
    result = sx_app_process_envelope(&app, envelope, length, 20u);
    CHECK(result.result_code == SX_APP_RESULT_RATE_LIMITED);
    CHECK(sequence.high_water == 1u); /* rate-limited but authenticated commands consume sequence */
    CHECK(output.set_calls == 0u);
    mock_rate_is_allowed = true;
}

static void test_policy_update_persists_before_activation(void)
{
    sx_app_core_t app;
    mock_sequence_store_t sequence = {0u, true, false, 0u};
    mock_policy_store_t policy;
    mock_output_t output = {0};
    const uint8_t policy_payload[] = {
        0x00,0x00,0x00,0x02, 0x00,0x05, 0x01,0x00,
        0x00,0x01,0x01,0x00,0x00,0x00,0x01
    };
    uint8_t envelope[SX_ENVELOPE_MAX_SIZE];
    size_t length;
    sx_app_result_t result;

    init_mock_policy(&policy);
    CHECK(init_app(&app, &sequence, &policy, &output, true, true));
    length = build_envelope(envelope, SX_MSG_POLICY_UPDATE, 1u,
                            SX_OP_POLICY_UPDATE, policy_payload,
                            (uint16_t)sizeof(policy_payload));
    result = sx_app_process_envelope(&app, envelope, length, 100u);
    CHECK(result.result_code == SX_APP_RESULT_SUCCESS);
    CHECK(sequence.high_water == 1u);
    CHECK(policy.commit_calls == 1u);
    CHECK(app.policy_valid);
    CHECK(app.active_policy.version == 2u);
    CHECK(output.set_calls == 0u);
}

static void test_storage_failure_and_link_timeout(void)
{
    sx_app_core_t app;
    mock_sequence_store_t sequence = {0u, true, true, 0u};
    mock_policy_store_t policy;
    mock_output_t output = {0};
    sx_app_result_t result;
    uint8_t payload[2] = {1u,1u};
    uint8_t envelope[SX_ENVELOPE_MAX_SIZE];
    size_t length;

    init_mock_policy(&policy);
    CHECK(init_app(&app, &sequence, &policy, &output, true, true));
    sx_app_process_link_status(&app, (const uint8_t[7]){1u,1u,0u,0u,0u,0u,1u}, 100u);
    length = build_envelope(envelope, SX_MSG_CMD, 1u, SX_OP_SET_OUTPUT, payload, 2u);
    result = sx_app_process_envelope(&app, envelope, length, 200u);
    CHECK(result.result_code == SX_APP_RESULT_PERSISTENCE_FAILED);
    CHECK(!app.storage_ready);
    CHECK(output.set_calls == 0u);

    /* Separate healthy instance: missing heartbeat beyond 3000 ms forces OFF. */
    sequence.high_water = 0u;
    sequence.fail_commit = false;
    CHECK(init_app(&app, &sequence, &policy, &output, true, true));
    sx_app_process_link_status(&app, (const uint8_t[7]){1u,1u,0u,0u,0u,0u,1u}, 100u);
    sx_app_tick(&app, 3101u);
    CHECK(app.output_state == 0u);
    CHECK(output.off_calls > 0u);
}

int main(void)
{
    test_crc_and_uart_parser();
    test_envelope_and_command();
    test_policy_validation();
    test_replay_guard_commit_order_and_fail_closed();
    test_app_execution_after_sequence_commit();
    test_missing_policy_denies_actuation_but_allows_status();
    test_hmac_failure_and_rate_limit_do_not_actuate();
    test_policy_update_persists_before_activation();
    test_storage_failure_and_link_timeout();

    (void)printf("STM32 core host tests: %u checks, %u failures\n", tests_run, tests_failed);
    return tests_failed == 0u ? 0 : 1;
}
