#ifndef SX_PLATFORM_PORT_H
#define SX_PLATFORM_PORT_H

#include "sx_app_core.h"

/*
 * Legacy fail-closed fallback binding. For the v0.2 AT24C256 implementation,
 * include sx_platform_port_at24c256.h and call its explicitly named bind API.
 * Never replace persistent storage with a RAM-only high-water counter.
 */

bool SX_Platform_SequenceLoad(void *context, uint64_t *high_water_mark);
bool SX_Platform_SequenceCommit(void *context, uint64_t high_water_mark);
bool SX_Platform_KeyGet(void *context, uint8_t key_id,
                        uint8_t key_out[SX_DEVICE_KEY_SIZE]);
bool SX_Platform_PolicyLoad(void *context, sx_policy_v1_t *policy);
bool SX_Platform_PolicyCommit(void *context, const sx_policy_v1_t *policy);
bool SX_Platform_RateLimitAllow(void *context, uint32_t now_ms,
                                uint16_t max_commands_per_second);
void SX_Platform_BindInterfaces(sx_sequence_store_t *sequence_store,
                                sx_key_provider_t *key_provider,
                                sx_crypto_provider_t *crypto_provider,
                                sx_policy_store_t *policy_store);

#endif /* SX_PLATFORM_PORT_H */
