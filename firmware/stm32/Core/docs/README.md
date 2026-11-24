# STM32F411RE firmware — initial core

This folder contains the first MCU-side implementation slice for SENTINEL-X. It is intended to be copied into a **STM32CubeIDE project generated for NUCLEO-F411RE / STM32F411RETx**. It is not a complete, self-contained CubeIDE project: CubeIDE must generate the startup code, linker script, CMSIS files, STM32F4 HAL and the `.ioc` for the local installed Cube firmware package.

## Implemented in this slice

- UART frame parser: SOF `A5 5A`, type/length validation, CRC-16/CCITT-FALSE, bounded payload storage and a proposed 100 ms partial-frame timeout (confirm with ESP32 timing).
- EnvelopeV1 structural parser: version, direction, device ID, key ID, payload length, big-endian sequence and operation code.
- Command payload validation for `SET_OUTPUT`, `SET_LEVEL`, `GET_STATUS`.
- PolicyV1 payload validation, including version monotonicity, entry count, reserved flags, duplicate command rejection and absolute bounds.
- HMAC-SHA-256 verification adapter for Mbed TLS, using a constant-time 32-byte tag comparison.
- Replay guard with a persistent-storage callback. It commits the high-water mark before any output callback and fails closed on read/write failure.
- MCU foreground processing core, link-status timeout handling and an interrupt-fed UART byte ring.
- STM32 HAL adapter for PB0 / TIM3_CH3 PWM, USART1 interrupt RX, and early output-low initialization.
- Host tests for CRC, UART parser, EnvelopeV1, command schemas, PolicyV1, replay persistence ordering, safe-mode behavior and the public HMAC vector.

## Not implemented / hardware-dependent

- CubeIDE-generated `.ioc`, startup code, linker script and HAL/CMSIS libraries.
- Concrete FRAM part selection and its two-slot journal driver. The sequence and policy stores are interfaces only; do not replace them with RAM counters.
- Real 32-byte device-key provisioning and the key-provider implementation. No real or test HMAC key is included in firmware.
- Mbed TLS source/library integration and target compilation of `sx_crypto_mbedtls.c`.
- ACK/EVENT/STATUS Envelope generation and UART TX path.
- Final rate-limit algorithm selection. The contracts have a fixed-window/token-bucket discrepancy; the current core requires a rate-limit callback and denies actuator commands when it is absent.
- Physical validation of PB0 PWM, UART wiring, external pull-down and the exact NUCLEO/module revision.

Until valid persistent sequence storage, a provisioned device key, a valid persisted policy and a functioning link heartbeat are all provided, the MCU stays in a safe state and does not energize the PWM output.

## Host tests

From the repository root:

```bash
./firmware/stm32/tests/run_host_tests.sh
```

The host tests compile the portable C modules with GCC (`-Wall -Wextra -Werror -pedantic`) and verify the public interoperability vector with Python's standard `hmac` implementation. They do **not** compile the STM32 HAL adapter or the Mbed TLS source file.

## Bring-up order

1. Generate the NUCLEO-F411RE STM32CubeIDE project following `docs/CUBEIDE_SETUP.md`.
2. Add `Core/Inc` and `Core/Src` files to the generated project; add `protocol/c` to compiler include paths.
3. Wire early PB0 OFF, TIM3_CH3 PWM, USART1 RX/TX and the foreground adapter exactly as described in `docs/INTEGRATION_NOTES.md`.
4. Build and flash with storage/key/policy providers still fail-closed. Confirm PWM remains OFF at reset, boot and UART disconnect.
5. Select the FRAM part and implement the storage port before enabling any command that actuates output.
6. Add Mbed TLS and a device-specific key provider; never use the public test vector key as a device key.
7. Validate on the actual board with a low-voltage load/driver only.
