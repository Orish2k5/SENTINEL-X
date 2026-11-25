#include "sx_at24c256.h"
#include "sx_persistence_at24c256.h"
#include "sx_tb6612.h"

#include <stdio.h>
#include <string.h>

static unsigned int checks;
static unsigned int failures;

#define CHECK(condition) do { \
    checks++; \
    if (!(condition)) { \
        failures++; \
        (void)printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    } \
} while (0)

typedef struct {
    uint8_t bytes[SX_AT24C256_CAPACITY_BYTES];
    uint32_t write_calls;
    uint32_t fail_on_write_call;
    uint32_t write_then_fail_on_write_call;
    bool page_boundary_violation;
} mock_eeprom_t;

static bool mock_eeprom_read(void *context, uint16_t address,
                             uint8_t *data, uint16_t length)
{
    mock_eeprom_t *memory = (mock_eeprom_t *)context;
    if ((memory == NULL) || (data == NULL) || (length == 0u) ||
        ((uint32_t)address + length > SX_AT24C256_CAPACITY_BYTES)) return false;
    (void)memcpy(data, &memory->bytes[address], length);
    return true;
}

static bool mock_eeprom_write(void *context, uint16_t address,
                              const uint8_t *data, uint16_t length)
{
    mock_eeprom_t *memory = (mock_eeprom_t *)context;
    if ((memory == NULL) || (data == NULL) || (length == 0u) ||
        ((uint32_t)address + length > SX_AT24C256_CAPACITY_BYTES)) return false;
    if (((uint32_t)address % SX_AT24C256_PAGE_SIZE) + length > SX_AT24C256_PAGE_SIZE) {
        memory->page_boundary_violation = true;
        return false;
    }
    memory->write_calls++;
    if ((memory->fail_on_write_call != 0u) &&
        (memory->write_calls == memory->fail_on_write_call)) return false;
    (void)memcpy(&memory->bytes[address], data, length);
    if ((memory->write_then_fail_on_write_call != 0u) &&
        (memory->write_calls == memory->write_then_fail_on_write_call)) return false;
    return true;
}

static bool init_memory(mock_eeprom_t *memory, sx_at24c256_t *device)
{
    sx_at24c256_io_t io;
    (void)memset(memory, 0, sizeof(*memory));
    (void)memset(memory->bytes, 0xFF, sizeof(memory->bytes));
    io.context = memory;
    io.read = mock_eeprom_read;
    io.write = mock_eeprom_write;
    return sx_at24c256_init(device, &io);
}

static void make_test_policy(sx_policy_v1_t *policy, uint32_t version)
{
    (void)memset(policy, 0, sizeof(*policy));
    policy->version = version;
    policy->max_commands_per_second = 5u;
    policy->entry_count = 2u;
    policy->flags = 1u;
    policy->entries[0].command_code = SX_OP_SET_OUTPUT;
    policy->entries[0].allowed = 1u;
    policy->entries[0].min_value = 0u;
    policy->entries[0].max_value = 1u;
    policy->entries[1].command_code = SX_OP_SET_LEVEL;
    policy->entries[1].allowed = 1u;
    policy->entries[1].min_value = 0u;
    policy->entries[1].max_value = 800u;
}

static void test_eeprom_page_split_and_bounds(void)
{
    mock_eeprom_t memory;
    sx_at24c256_t device;
    uint8_t input[4] = {0x11u, 0x22u, 0x33u, 0x44u};
    uint8_t output[4] = {0u};

    CHECK(init_memory(&memory, &device));
    CHECK(sx_at24c256_write(&device, 0x003Eu, input, sizeof(input)));
    CHECK(!memory.page_boundary_violation);
    CHECK(sx_at24c256_read(&device, 0x003Eu, output, sizeof(output)));
    CHECK(memcmp(input, output, sizeof(input)) == 0);
    CHECK(!sx_at24c256_read(&device, 0x7FFFu, output, 2u));
    CHECK(!sx_at24c256_write(&device, 0x7FFFu, input, 2u));
}

static void test_sequence_factory_provision_and_reboot(void)
{
    mock_eeprom_t memory;
    sx_at24c256_t device;
    sx_persistence_at24c256_t persistence;
    sx_persistence_at24c256_t rebooted;
    uint64_t value = 99u;

    CHECK(init_memory(&memory, &device));
    CHECK(sx_persistence_at24c256_init(&persistence, &device));
    CHECK(sx_persistence_at24c256_provision_blank(&persistence, 0u, 0u));
    CHECK(sx_persistence_inbound_sequence_load(&persistence, &value));
    CHECK(value == 0u);
    CHECK(sx_persistence_outbound_sequence_load(&persistence, &value));
    CHECK(value == 0u);
    CHECK(sx_persistence_inbound_sequence_commit(&persistence, 1u));
    CHECK(sx_persistence_inbound_sequence_commit(&persistence, 2u));
    CHECK(!sx_persistence_inbound_sequence_commit(&persistence, 2u));

    CHECK(sx_persistence_at24c256_init(&rebooted, &device));
    CHECK(sx_persistence_inbound_sequence_load(&rebooted, &value));
    CHECK(value == 2u);
    CHECK(sx_persistence_outbound_sequence_load(&rebooted, &value));
    CHECK(value == 0u);
    CHECK(!sx_persistence_at24c256_provision_blank(&rebooted, 0u, 0u));
}

static void test_sequence_wear_level_ring_rollover(void)
{
    mock_eeprom_t memory;
    sx_at24c256_t device;
    sx_persistence_at24c256_t persistence;
    sx_persistence_at24c256_t rebooted;
    uint64_t value = 0u;
    uint64_t sequence;

    CHECK(init_memory(&memory, &device));
    CHECK(sx_persistence_at24c256_init(&persistence, &device));
    CHECK(sx_persistence_at24c256_provision_blank(&persistence, 0u, 0u));

    for (sequence = 1u; sequence <= (SX_PERSISTENCE_SEQUENCE_SLOT_COUNT + 25u); ++sequence) {
        if (!sx_persistence_inbound_sequence_commit(&persistence, sequence)) {
            CHECK(false);
            break;
        }
    }
    CHECK(!memory.page_boundary_violation);
    CHECK(sx_persistence_at24c256_init(&rebooted, &device));
    CHECK(sx_persistence_inbound_sequence_load(&rebooted, &value));
    CHECK(value == (SX_PERSISTENCE_SEQUENCE_SLOT_COUNT + 25u));
}

static void test_interrupted_sequence_commit_preserves_previous_high_water(void)
{
    mock_eeprom_t memory;
    sx_at24c256_t device;
    sx_persistence_at24c256_t persistence;
    sx_persistence_at24c256_t rebooted;
    uint64_t value = 0u;

    CHECK(init_memory(&memory, &device));
    CHECK(sx_persistence_at24c256_init(&persistence, &device));
    CHECK(sx_persistence_at24c256_provision_blank(&persistence, 0u, 0u));
    CHECK(sx_persistence_inbound_sequence_commit(&persistence, 10u));

    /* Fail the final marker write: body remains uncommitted; prior high-water survives. */
    memory.fail_on_write_call = memory.write_calls + 3u;
    CHECK(!sx_persistence_inbound_sequence_commit(&persistence, 11u));
    memory.fail_on_write_call = 0u;
    CHECK(sx_persistence_at24c256_is_faulted(&persistence));
    CHECK(!sx_persistence_inbound_sequence_commit(&persistence, 12u));
    CHECK(!sx_persistence_inbound_sequence_load(&persistence, &value));

    CHECK(sx_persistence_at24c256_init(&rebooted, &device));
    CHECK(sx_persistence_inbound_sequence_load(&rebooted, &value));
    CHECK(value == 10u);
}

static void test_ambiguous_sequence_commit_latches_until_reboot(void)
{
    mock_eeprom_t memory;
    sx_at24c256_t device;
    sx_persistence_at24c256_t persistence;
    sx_persistence_at24c256_t rebooted;
    uint64_t value = 0u;

    CHECK(init_memory(&memory, &device));
    CHECK(sx_persistence_at24c256_init(&persistence, &device));
    CHECK(sx_persistence_at24c256_provision_blank(&persistence, 0u, 0u));

    /* Simulate EEPROM committing A5 but the HAL caller observing an I/O error. */
    memory.write_then_fail_on_write_call = memory.write_calls + 3u;
    CHECK(!sx_persistence_inbound_sequence_commit(&persistence, 1u));
    CHECK(sx_persistence_at24c256_is_faulted(&persistence));
    memory.write_then_fail_on_write_call = 0u;
    CHECK(!sx_persistence_inbound_sequence_commit(&persistence, 2u));

    /* Re-init rescans flash/EEPROM truth; committed sequence 1 must survive. */
    CHECK(sx_persistence_at24c256_init(&rebooted, &device));
    CHECK(sx_persistence_inbound_sequence_load(&rebooted, &value));
    CHECK(value == 1u);
    CHECK(sx_persistence_inbound_sequence_commit(&rebooted, 2u));
}

static void test_corrupt_committed_sequence_fails_closed(void)
{
    mock_eeprom_t memory;
    sx_at24c256_t device;
    sx_persistence_at24c256_t persistence;
    sx_persistence_at24c256_t rebooted;
    uint64_t value = 0u;
    uint16_t newest_address;

    CHECK(init_memory(&memory, &device));
    CHECK(sx_persistence_at24c256_init(&persistence, &device));
    CHECK(sx_persistence_at24c256_provision_blank(&persistence, 0u, 0u));
    CHECK(sx_persistence_inbound_sequence_commit(&persistence, 1u));

    newest_address = (uint16_t)(SX_PERSISTENCE_INBOUND_BASE +
        (uint32_t)persistence.inbound.current_slot * SX_PERSISTENCE_SEQUENCE_RECORD_SIZE);
    memory.bytes[newest_address + 12u] ^= 0x01u;

    CHECK(sx_persistence_at24c256_init(&rebooted, &device));
    CHECK(!sx_persistence_inbound_sequence_load(&rebooted, &value));
}

static void test_policy_two_slot_journal(void)
{
    mock_eeprom_t memory;
    sx_at24c256_t device;
    sx_persistence_at24c256_t persistence;
    sx_persistence_at24c256_t rebooted;
    sx_policy_v1_t policy1;
    sx_policy_v1_t policy2;
    sx_policy_v1_t loaded;

    CHECK(init_memory(&memory, &device));
    CHECK(sx_persistence_at24c256_init(&persistence, &device));
    CHECK(sx_persistence_at24c256_provision_blank(&persistence, 0u, 0u));
    make_test_policy(&policy1, 1u);
    make_test_policy(&policy2, 2u);

    CHECK(sx_persistence_policy_commit(&persistence, &policy1));
    CHECK(sx_persistence_policy_load(&persistence, &loaded));
    CHECK(loaded.version == 1u);
    CHECK(sx_persistence_policy_commit(&persistence, &policy2));
    CHECK(!sx_persistence_policy_commit(&persistence, &policy2));

    CHECK(sx_persistence_at24c256_init(&rebooted, &device));
    CHECK(sx_persistence_policy_load(&rebooted, &loaded));
    CHECK(loaded.version == 2u);
    CHECK(loaded.entries[1].max_value == 800u);
}

static void test_policy_interrupted_update_keeps_previous_policy(void)
{
    mock_eeprom_t memory;
    sx_at24c256_t device;
    sx_persistence_at24c256_t persistence;
    sx_persistence_at24c256_t rebooted;
    sx_policy_v1_t policy1;
    sx_policy_v1_t policy2;
    sx_policy_v1_t loaded;

    CHECK(init_memory(&memory, &device));
    CHECK(sx_persistence_at24c256_init(&persistence, &device));
    CHECK(sx_persistence_at24c256_provision_blank(&persistence, 0u, 0u));
    make_test_policy(&policy1, 1u);
    make_test_policy(&policy2, 2u);
    CHECK(sx_persistence_policy_commit(&persistence, &policy1));

    /* Policy payload spans two EEPROM page writes; fail the final marker write. */
    memory.fail_on_write_call = memory.write_calls + 4u;
    CHECK(!sx_persistence_policy_commit(&persistence, &policy2));
    memory.fail_on_write_call = 0u;
    CHECK(sx_persistence_at24c256_is_faulted(&persistence));
    CHECK(!sx_persistence_policy_commit(&persistence, &policy2));

    CHECK(sx_persistence_at24c256_init(&rebooted, &device));
    CHECK(sx_persistence_policy_load(&rebooted, &loaded));
    CHECK(loaded.version == 1u);
}

static void test_ambiguous_policy_commit_latches_until_reboot(void)
{
    mock_eeprom_t memory;
    sx_at24c256_t device;
    sx_persistence_at24c256_t persistence;
    sx_persistence_at24c256_t rebooted;
    sx_policy_v1_t policy1;
    sx_policy_v1_t policy2;
    sx_policy_v1_t loaded;

    CHECK(init_memory(&memory, &device));
    CHECK(sx_persistence_at24c256_init(&persistence, &device));
    CHECK(sx_persistence_at24c256_provision_blank(&persistence, 0u, 0u));
    make_test_policy(&policy1, 1u);
    make_test_policy(&policy2, 2u);
    CHECK(sx_persistence_policy_commit(&persistence, &policy1));

    /* The commit marker reaches EEPROM, but the write callback reports failure. */
    memory.write_then_fail_on_write_call = memory.write_calls + 4u;
    CHECK(!sx_persistence_policy_commit(&persistence, &policy2));
    CHECK(sx_persistence_at24c256_is_faulted(&persistence));
    memory.write_then_fail_on_write_call = 0u;
    CHECK(!sx_persistence_policy_commit(&persistence, &policy2));

    CHECK(sx_persistence_at24c256_init(&rebooted, &device));
    CHECK(sx_persistence_policy_load(&rebooted, &loaded));
    CHECK(loaded.version == 2u);
}

typedef struct {
    char events[64];
    uint16_t pwm_level;
    bool standby_enabled;
    bool ain1;
    bool ain2;
    unsigned int event_count;
    unsigned int fail_on_event;
} mock_motor_t;

static bool motor_event(mock_motor_t *mock, char event)
{
    if (mock == NULL) return false;
    mock->event_count++;
    if (mock->event_count <= sizeof(mock->events)) {
        mock->events[mock->event_count - 1u] = event;
    }
    return (mock->fail_on_event == 0u) || (mock->event_count != mock->fail_on_event);
}

static bool motor_standby(void *context, bool enabled)
{
    mock_motor_t *mock = (mock_motor_t *)context;
    if (!motor_event(mock, enabled ? 'E' : 'D')) return false;
    mock->standby_enabled = enabled;
    return true;
}

static bool motor_inputs(void *context, bool ain1, bool ain2)
{
    mock_motor_t *mock = (mock_motor_t *)context;
    if (!motor_event(mock, (ain1 && !ain2) ? 'F' : 'N')) return false;
    mock->ain1 = ain1;
    mock->ain2 = ain2;
    return true;
}

static bool motor_pwm(void *context, uint16_t level)
{
    mock_motor_t *mock = (mock_motor_t *)context;
    if (!motor_event(mock, level == 0u ? '0' : 'P')) return false;
    mock->pwm_level = level;
    return true;
}

static void test_tb6612_safe_order_and_failure(void)
{
    mock_motor_t mock;
    sx_tb6612_t motor;
    sx_tb6612_io_t io;

    (void)memset(&mock, 0, sizeof(mock));
    io.context = &mock;
    io.set_standby = motor_standby;
    io.set_inputs = motor_inputs;
    io.set_pwm_permille = motor_pwm;

    CHECK(sx_tb6612_init(&motor, &io));
    CHECK(!mock.standby_enabled);
    CHECK(mock.pwm_level == 0u);
    CHECK(!mock.ain1 && !mock.ain2);

    mock.event_count = 0u;
    CHECK(sx_tb6612_set_forward_level(&motor, 650u));
    CHECK(mock.event_count == 5u);
    CHECK(mock.events[0] == 'D');
    CHECK(mock.events[1] == '0');
    CHECK(mock.events[2] == 'F');
    CHECK(mock.events[3] == 'P');
    CHECK(mock.events[4] == 'E');
    CHECK(mock.standby_enabled);
    CHECK(mock.pwm_level == 650u);
    CHECK(mock.ain1 && !mock.ain2);

    CHECK(sx_tb6612_force_off(&motor));
    CHECK(!mock.standby_enabled);
    CHECK(mock.pwm_level == 0u);
    CHECK(!mock.ain1 && !mock.ain2);
    CHECK(!sx_tb6612_set_forward_level(&motor, 1001u));

    mock.fail_on_event = mock.event_count + 3u;
    CHECK(!sx_tb6612_set_forward_level(&motor, 500u));
    CHECK(sx_tb6612_is_faulted(&motor));
    CHECK(!mock.standby_enabled);
}

int main(void)
{
    test_eeprom_page_split_and_bounds();
    test_sequence_factory_provision_and_reboot();
    test_sequence_wear_level_ring_rollover();
    test_interrupted_sequence_commit_preserves_previous_high_water();
    test_ambiguous_sequence_commit_latches_until_reboot();
    test_corrupt_committed_sequence_fails_closed();
    test_policy_two_slot_journal();
    test_policy_interrupted_update_keeps_previous_policy();
    test_ambiguous_policy_commit_latches_until_reboot();
    test_tb6612_safe_order_and_failure();

    (void)printf("STM32 hardware-driver host tests: %u checks, %u failures\n", checks, failures);
    return failures == 0u ? 0 : 1;
}
