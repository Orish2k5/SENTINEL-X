#ifndef SX_PERSISTENCE_AT24C256_H
#define SX_PERSISTENCE_AT24C256_H

#include <stdbool.h>
#include <stdint.h>

#include "sx_app_core.h"
#include "sx_at24c256.h"

#define SX_PERSISTENCE_SEQUENCE_RECORD_SIZE 32u
#define SX_PERSISTENCE_SEQUENCE_LOG_SIZE 0x3F00u
#define SX_PERSISTENCE_SEQUENCE_SLOT_COUNT \
    (SX_PERSISTENCE_SEQUENCE_LOG_SIZE / SX_PERSISTENCE_SEQUENCE_RECORD_SIZE)
#define SX_PERSISTENCE_INBOUND_BASE 0x0000u
#define SX_PERSISTENCE_OUTBOUND_BASE 0x3F00u
#define SX_PERSISTENCE_POLICY_SLOT_SIZE 128u
#define SX_PERSISTENCE_POLICY_SLOT_A 0x7E00u
#define SX_PERSISTENCE_POLICY_SLOT_B 0x7E80u

typedef struct {
    uint64_t high_water_mark;
    uint32_t generation;
    uint16_t current_slot;
    bool loaded;
    bool present;
} sx_persistence_sequence_state_t;

typedef struct {
    sx_at24c256_t *eeprom;
    sx_persistence_sequence_state_t inbound;
    sx_persistence_sequence_state_t outbound;
    sx_policy_v1_t active_policy;
    uint32_t policy_generation;
    uint8_t policy_active_slot;
    bool initialized;
    bool policy_loaded;
    bool policy_present;
    bool faulted; /* Latches any ambiguous EEPROM I/O until re-init after reboot/recovery. */
} sx_persistence_at24c256_t;

bool sx_persistence_at24c256_init(sx_persistence_at24c256_t *persistence,
                                  sx_at24c256_t *eeprom);

/*
 * Explicit factory commissioning only. This never runs automatically at boot.
 * It succeeds only when both sequence logs and both policy slots are erased.
 */
bool sx_persistence_at24c256_provision_blank(sx_persistence_at24c256_t *persistence,
                                              uint64_t initial_inbound_sequence,
                                              uint64_t initial_outbound_sequence);

bool sx_persistence_inbound_sequence_load(void *context, uint64_t *high_water_mark);
bool sx_persistence_inbound_sequence_commit(void *context, uint64_t high_water_mark);
bool sx_persistence_outbound_sequence_load(void *context, uint64_t *high_water_mark);
bool sx_persistence_outbound_sequence_commit(void *context, uint64_t high_water_mark);
bool sx_persistence_policy_load(void *context, sx_policy_v1_t *policy);
bool sx_persistence_policy_commit(void *context, const sx_policy_v1_t *policy);

bool sx_persistence_at24c256_is_faulted(const sx_persistence_at24c256_t *persistence);

void sx_persistence_at24c256_bind_app(sx_persistence_at24c256_t *persistence,
                                      sx_sequence_store_t *sequence_store,
                                      sx_policy_store_t *policy_store);

#endif /* SX_PERSISTENCE_AT24C256_H */
