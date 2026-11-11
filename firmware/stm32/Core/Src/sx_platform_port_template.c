#include "sx_platform_port.h"
#include "sx_crypto_mbedtls.h"

/*
 * Deliberately fail-closed template. Replace each function with the selected
 * FRAM/key-provisioning/rate-limit implementation. Do not return a fabricated
 * sequence value of zero and do not embed a test key here.
 */

bool SX_Platform_SequenceLoad(void *context, uint64_t *high_water_mark)
{
    (void)context;
    (void)high_water_mark;
    return false;
}

bool SX_Platform_SequenceCommit(void *context, uint64_t high_water_mark)
{
    (void)context;
    (void)high_water_mark;
    return false;
}

bool SX_Platform_KeyGet(void *context, uint8_t key_id,
                        uint8_t key_out[SX_DEVICE_KEY_SIZE])
{
    (void)context;
    (void)key_id;
    (void)key_out;
    return false;
}

bool SX_Platform_PolicyLoad(void *context, sx_policy_v1_t *policy)
{
    (void)context;
    (void)policy;
    return false;
}

bool SX_Platform_PolicyCommit(void *context, const sx_policy_v1_t *policy)
{
    (void)context;
    (void)policy;
    return false;
}

bool SX_Platform_RateLimitAllow(void *context, uint32_t now_ms,
                                uint16_t max_commands_per_second)
{
    (void)context;
    (void)now_ms;
    (void)max_commands_per_second;
    return false;
}

void SX_Platform_BindInterfaces(sx_sequence_store_t *sequence_store,
                                sx_key_provider_t *key_provider,
                                sx_crypto_provider_t *crypto_provider,
                                sx_policy_store_t *policy_store)
{
    if (sequence_store != NULL) {
        sequence_store->context = NULL;
        sequence_store->load = SX_Platform_SequenceLoad;
        sequence_store->commit = SX_Platform_SequenceCommit;
    }
    if (key_provider != NULL) {
        key_provider->context = NULL;
        key_provider->get_key = SX_Platform_KeyGet;
    }
    if (crypto_provider != NULL) {
        crypto_provider->context = NULL;
        crypto_provider->verify = sx_crypto_mbedtls_verify_callback;
    }
    if (policy_store != NULL) {
        policy_store->context = NULL;
        policy_store->load = SX_Platform_PolicyLoad;
        policy_store->commit = SX_Platform_PolicyCommit;
    }
}
