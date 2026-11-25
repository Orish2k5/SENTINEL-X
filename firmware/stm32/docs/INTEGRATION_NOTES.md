# Integration notes — STM32 Core v0.2

## 1. Main startup order

The order below shows the intended wiring; function names must be checked against the generated CubeMX project before copying. Do not add a call to factory provisioning in the normal boot path.

```c
#include "main.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"

#include "sx_app_core.h"
#include "sx_platform_port_at24c256.h"
#include "sx_stm32_adapter.h"

static sx_app_core_t g_sx_app;
static uint8_t g_sx_device_id[SX_SIZE_DEVICE_ID];
static sx_sequence_store_t g_sx_sequence_store;
static sx_key_provider_t g_sx_key_provider;
static sx_crypto_provider_t g_sx_crypto_provider;
static sx_policy_store_t g_sx_policy_store;
static sx_output_interface_t g_sx_output;

int main(void)
{
    HAL_Init();
    SX_STM32_ForceOutputOffEarly();

    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_USART1_UART_Init();
    MX_TIM3_Init();

    if (SX_STM32_InitOutputSafe() != HAL_OK) {
        Error_Handler(); /* hardware motor interface did not initialize safely */
    }

    sx_device_id_make(g_sx_device_id, "SX-001");
    SX_STM32_GetOutputInterface(&g_sx_output);

    if (!SX_Platform_AT24C256_Init(&hi2c1, SX_AT24C256_I2C_ADDRESS_7BIT)) {
        SX_STM32_ForceOutputOffEarly();
        Error_Handler();
    }

    SX_Platform_AT24C256_BindInterfaces(&g_sx_sequence_store,
                                        &g_sx_key_provider,
                                        &g_sx_crypto_provider,
                                        &g_sx_policy_store);

    if (!sx_app_core_init(&g_sx_app, g_sx_device_id,
                          &g_sx_sequence_store, &g_sx_key_provider,
                          &g_sx_crypto_provider, &g_sx_policy_store,
                          SX_Platform_AT24C256_RateLimitAllow, NULL,
                          &g_sx_output)) {
        /* Keep outputs disabled; expose local diagnostics only. */
        SX_STM32_ForceOutputOffEarly();
    }

    if (SX_STM32_StartUartRx() != HAL_OK) {
        sx_app_force_off(&g_sx_app);
    }

    while (1) {
        SX_STM32_Process(&g_sx_app, HAL_GetTick());
    }
}
```

This is an integration skeleton, not a drop-in `main.c`. In particular, the error path should use a project-specific safe diagnostic loop rather than repeatedly calling `Error_Handler()` without reviewing its behavior.

## 2. UART callbacks

Merge calls into existing HAL callbacks if they already exist:

```c
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    SX_STM32_UartRxCompleteCallback(huart);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    SX_STM32_UartErrorCallback(huart);
}
```

Callbacks perform bounded byte queueing/rearm only. CRC parsing, HMAC, EEPROM writes and output changes run in foreground context.

## 3. Factory provisioning

The EEPROM starts erased and must not be silently assigned sequence zero. A factory/service action may call `SX_Platform_AT24C256_ProvisionBlankDevice(0u, 0u)` only after confirming that the device is genuinely new and the operation is authorized. The function rejects nonblank/corrupt memory. The project still needs an authenticated/physical commissioning workflow before field use.

The first policy is not provisioned by that call. Until a valid policy is persisted, actuation is denied. The public protocol test key is never a deployment key.

## 4. Explicitly disabled paths

- Key provider returns false until a device-specific secret storage/provisioning method is designed.
- Rate-limit callback returns false until the fixed-window vs token-bucket decision is recorded in an ADR/contract.
- Outbound sequence journal APIs exist but MCU ACK/EVENT/STATUS generation/transmit is not implemented here.
- Relay Modbus RTU is not implemented; current actuator is one-direction TB6612 channel A only.

## 5. Hardware verification checklist

- Confirm PA0/PA1/PC0/PB0, PB6/PB7 and PB8/PB9 pinout in the final `.ioc`.
- Confirm STBY has an external pull-down and is low during reset.
- Verify PWM output is zero before enabling TB6612 STBY.
- Verify AT24C256 address straps and 3.3 V pull-ups.
- Verify I2C read/write and ACK polling on the selected module.
- Perform controlled power-cut tests during sequence invalidation, body write and commit-marker write.
- Verify reboot loads the prior high-water or enters SAFE_MODE; never reset sequence to zero automatically.
- Verify motor supply and stall current against the actual motor and TB6612 module datasheet.

No board-level test result is claimed by the host test suite.
