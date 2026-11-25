#include "sx_platform_port_at24c256.h"

#include "sx_board_config.h"
#include "sx_at24c256.h"
#include "sx_at24c256_hal.h"
#include "sx_crypto_mbedtls.h"
#include "sx_persistence_at24c256.h"

static sx_at24c256_hal_t sx_eeprom_hal;
static sx_at24c256_t sx_eeprom;
static sx_at24c256_io_t sx_eeprom_io;
static sx_persistence_at24c256_t sx_persistence;
static bool sx_storage_initialized;

static bool sx_key_provider_not_provisioned(void *context, uint8_t key_id,
                                            uint8_t key_out[SX_DEVICE_KEY_SIZE])
{
    (void)context;
    (void)key_id;
    (void)key_out;
    /* No key is compiled into firmware. Replace with the approved key store later. */
    return false;
}

static bool sx_rate_limit_not_configured(void *context, uint32_t now_ms,
                                         uint16_t max_commands_per_second)
{
    (void)context;
    (void)now_ms;
    (void)max_commands_per_second;
    /* Deny actuation until the rate-limit algorithm is resolved and implemented. */
    return false;
}

bool SX_Platform_AT24C256_Init(I2C_HandleTypeDef *i2c, uint8_t address_7bit)
{
    sx_storage_initialized = false;
    if (!sx_at24c256_hal_configure(&sx_eeprom_hal, i2c, address_7bit,
                                   SX_AT24C256_I2C_TIMEOUT_MS)) {
        return false;
    }
    if (!sx_at24c256_hal_probe(&sx_eeprom_hal)) {
        return false;
    }

    sx_at24c256_hal_make_io(&sx_eeprom_hal, &sx_eeprom_io);
    if (!sx_at24c256_init(&sx_eeprom, &sx_eeprom_io)) {
        return false;
    }
    if (!sx_persistence_at24c256_init(&sx_persistence, &sx_eeprom)) {
        return false;
    }

    sx_storage_initialized = true;
    return true;
}

bool SX_Platform_AT24C256_ProvisionBlankDevice(uint64_t initial_inbound_sequence,
                                               uint64_t initial_outbound_sequence)
{
    if (!sx_storage_initialized) return false;
    return sx_persistence_at24c256_provision_blank(&sx_persistence,
                                                    initial_inbound_sequence,
                                                    initial_outbound_sequence);
}

void SX_Platform_AT24C256_BindInterfaces(sx_sequence_store_t *sequence_store,
                                         sx_key_provider_t *key_provider,
                                         sx_crypto_provider_t *crypto_provider,
                                         sx_policy_store_t *policy_store)
{
    if (!sx_storage_initialized) {
        if (sequence_store != NULL) {
            sequence_store->context = NULL;
            sequence_store->load = NULL;
            sequence_store->commit = NULL;
        }
        if (key_provider != NULL) {
            key_provider->context = NULL;
            key_provider->get_key = sx_key_provider_not_provisioned;
        }
        if (crypto_provider != NULL) {
            crypto_provider->context = NULL;
            crypto_provider->verify = sx_crypto_mbedtls_verify_callback;
        }
        if (policy_store != NULL) {
            policy_store->context = NULL;
            policy_store->load = NULL;
            policy_store->commit = NULL;
        }
        return;
    }

    if ((sequence_store != NULL) && (policy_store != NULL)) {
        sx_persistence_at24c256_bind_app(&sx_persistence, sequence_store, policy_store);
    }
    if (key_provider != NULL) {
        key_provider->context = NULL;
        key_provider->get_key = sx_key_provider_not_provisioned;
    }
    if (crypto_provider != NULL) {
        crypto_provider->context = NULL;
        crypto_provider->verify = sx_crypto_mbedtls_verify_callback;
    }
}

/* This callback is intentionally exposed for use by sx_app_core_init(). */
bool SX_Platform_AT24C256_RateLimitAllow(void *context, uint32_t now_ms,
                                         uint16_t max_commands_per_second)
{
    return sx_rate_limit_not_configured(context, now_ms, max_commands_per_second);
}
