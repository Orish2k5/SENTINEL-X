#ifndef SX_PLATFORM_PORT_H
#define SX_PLATFORM_PORT_H

#include "sx_app_core.h"

/*
 * Board-specific implementation contract.
 * Implement these callbacks only after the FRAM part and provisioning procedure
 * have been selected. The template intentionally contains no RAM fallback.
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
