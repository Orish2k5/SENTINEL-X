#ifndef SX_PLATFORM_PORT_AT24C256_H
#define SX_PLATFORM_PORT_AT24C256_H

#include "stm32f4xx_hal.h"
#include "sx_app_core.h"

#include <stdbool.h>
#include <stdint.h>

/* Configure the real I2C HAL adapter and probe the EEPROM. */
bool SX_Platform_AT24C256_Init(I2C_HandleTypeDef *i2c, uint8_t address_7bit);

/*
 * Explicit factory-only commissioning operation. Must not be called from the
 * normal boot path. It refuses non-erased/corrupt journals.
 */
bool SX_Platform_AT24C256_ProvisionBlankDevice(uint64_t initial_inbound_sequence,
                                               uint64_t initial_outbound_sequence);

/* Bind EEPROM-backed inbound sequence/policy stores; key and rate-limit remain deny-by-default. */
bool SX_Platform_AT24C256_RateLimitAllow(void *context, uint32_t now_ms,
                                         uint16_t max_commands_per_second);

void SX_Platform_AT24C256_BindInterfaces(sx_sequence_store_t *sequence_store,
                                         sx_key_provider_t *key_provider,
                                         sx_crypto_provider_t *crypto_provider,
                                         sx_policy_store_t *policy_store);

#endif /* SX_PLATFORM_PORT_AT24C256_H */
