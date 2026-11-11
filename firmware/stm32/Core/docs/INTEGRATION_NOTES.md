# Integrating the STM32 core into generated CubeIDE code

## 1. Startup order

In the generated `main.c`, integrate the startup in this order. Preserve CubeMX-generated initialization and add the indicated calls:

```c
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

    /* First hardware action: preload PB0 low and make it a GPIO output. */
    SX_STM32_ForceOutputOffEarly();

    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_TIM3_Init();

    /* Set CCR=0 and only then start the PWM channel. */
    if (SX_STM32_InitOutputSafe() != HAL_OK) {
        Error_Handler();
    }

    /* These providers must be implemented by the hardware/provisioning layer.
       No RAM-only replay store and no public test key are acceptable. */
    sx_device_id_make(g_sx_device_id, "SX-001");
    SX_STM32_GetOutputInterface(&g_sx_output);
    SX_Platform_BindInterfaces(&g_sx_sequence_store,
                               &g_sx_key_provider,
                               &g_sx_crypto_provider,
                               &g_sx_policy_store);
    if (!sx_app_core_init(&g_sx_app,
                          g_sx_device_id,
                          &g_sx_sequence_store,
                          &g_sx_key_provider,
                          &g_sx_crypto_provider,
                          &g_sx_policy_store,
                          SX_Platform_RateLimitAllow,
                          NULL,
                          &g_sx_output)) {
        /* Keep the device in OFF/SAFE_MODE. Do not skip the foreground loop. */
        SX_STM32_ForceOutputOffEarly();
    }

    if (SX_STM32_StartUartRx() != HAL_OK) {
        sx_app_force_off(&g_sx_app);
    }

    while (1) {
        SX_STM32_Process(&g_sx_app, HAL_GetTick());
        /* Other non-blocking diagnostics may run here. */
    }
}
```

`SX_Platform_BindInterfaces()` wires the initial fail-closed template callbacks. The storage/key/rate-limit callbacks in `sx_platform_port_template.c` deliberately return `false`; replace them with verified hardware/provisioning implementations before attempting actuation. The app must receive:

- a sequence-store load/commit callback backed by the two-slot FRAM journal;
- a key provider that returns the provisioned 32-byte device key only for a known key ID;
- `sx_crypto_verify_hmac_sha256()` wrapped as an `sx_hmac_verify_fn`;
- a policy-store load/commit callback backed by persistent storage;
- a rate-limit callback using the algorithm selected by a recorded ADR;
- the output interface returned by `SX_STM32_GetOutputInterface()`.

If sequence storage cannot be read, `sx_app_core_init()` returns false and the app remains fail-closed. A policy-load failure does not authorize output commands; only an authenticated `GET_STATUS` is accepted when sequence storage is healthy.

## 2. UART HAL callbacks

Add the adapter calls to the callbacks generated/owned by the application. If callbacks already exist, merge the calls instead of defining duplicates:

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

The callback only puts received bytes into a bounded single-producer/single-consumer ring and rearms one-byte interrupt RX. CRC, HMAC, persistent sequence commit and output changes execute in the foreground via `SX_STM32_Process()`.

## 3. Output behavior

- `SX_STM32_ForceOutputOffEarly()` must run immediately after `HAL_Init()` and before generated GPIO setup.
- TIM3_CH3 must be configured on PB0 and initialized with CCR=0.
- `SX_STM32_InitOutputSafe()` starts PWM with compare zero.
- The first nonzero output is possible only after valid HMAC, schema validation, successful persistent sequence commit, online link status, valid persisted policy and successful rate-limit check.
- `SET_OUTPUT(0)` and `SET_LEVEL(0)` set PWM to zero.
- `SET_OUTPUT(1)` restores the last nonzero level (default 1000 permille); `SET_LEVEL(1..1000)` sets the level and enables the output.
- Missing heartbeat for more than 3000 ms, link offline, UART ring overflow, invalid persistent storage or missing policy forces output OFF. Recovery of the link does not automatically restore the previous output.

## 4. Sequence-store implementation contract

The current code contains only `sx_sequence_store_t` callbacks. A future FRAM backend must implement the Blueprint's two-slot journal:

1. Read both slots and validate generation, CRC and commit marker.
2. Select the newest valid slot without treating two invalid slots as sequence zero.
3. To commit a new high-water mark, write the inactive slot and verify readback.
4. Write/verify the commit marker only after the data and CRC are valid.
5. Keep the previous committed slot until the new slot is committed.
6. On read/write/verification failure, return `false`; the replay guard enters unavailable state and the app forces OFF.

The exact FRAM part/address/capacity is not specified in the current Blueprint. Do not invent a part-specific I2C driver until the BOM is selected.

## 5. Current limitations

- Partial-frame timeout is currently proposed as 100 ms and must be checked against ESP32-S3 sender timing.
- The adapter currently receives and validates inbound frames but does not serialize/transmit ACK, EVENT or STATUS envelopes.
- The app core does not yet implement the full Event Queue, Status serializer, outbound MCU sequence journal or all policy-application ACK behavior.
- The HMAC key provider and FRAM stores are intentionally not faked. The device will not execute output commands until they are implemented.
- Rate limiting is injected through a required callback. Resolve the fixed-window vs token-bucket discrepancy in the project documents before writing its implementation.
- No physical timing, PWM, UART noise, brown-out or reset-retention result is claimed by these host tests.
