# Validation report — STM32 core slice v0.1

Host validation performed in the artifact workspace on 2026-11-09.

| Check | Result | Scope / limitation |
|---|---|---|
| GCC portable C build | PASS | C11, `-Wall -Wextra -Werror -pedantic` |
| C host tests | PASS | 68 assertions covering CRC, UART parser, EnvelopeV1, command schemas, PolicyV1, replay guard, commit-before-actuation, fail-closed behavior, rate limiting and policy persistence |
| HMAC interoperability vector | PASS | Python standard library verifies the published HMAC-SHA-256 tag and one-bit mutation case |
| STM32 HAL adapter syntax check | PASS | Declaration-only HAL shim; not a real STM32CubeF4 HAL compile/link |
| Mbed TLS adapter syntax check | PASS | Declaration-only Mbed TLS shim; actual Mbed TLS library integration not linked or run |
| Target STM32CubeIDE build | NOT RUN | Requires user's installed CubeIDE/CubeF4 package and generated project |
| Physical NUCLEO-F411RE test | NOT RUN | No board, UART partner or low-voltage output stage attached in this environment |

## What PASS does not mean

- It does not certify target compiler compatibility or RAM/Flash/stack use.
- It does not verify the exact STM32CubeF4 HAL version, Mbed TLS version, UART electrical integrity or PWM polarity on the actual board.
- The C application tests use mock key, storage and policy callbacks. They verify call ordering and fail-closed behavior, not FRAM wear/power-loss characteristics.
- The C test double for HMAC returns an injected result. Cryptographic interoperability is checked with Python against the shared public test vector; target Mbed TLS HMAC still needs a target-side test.
- The public test key is only a test vector and is never included in the firmware key provider.

## Required before enabling actuator commands

1. Select the exact I2C FRAM part and implement/read-test the two-slot journal.
2. Implement device-specific key provisioning and return only the provisioned 32-byte key for `key_id=1`.
3. Persist/load a valid PolicyV1 record and validate record CRC/generation/commit marker.
4. Resolve the rate-limit algorithm discrepancy through an ADR, then implement its callback.
5. Build in CubeIDE with the actual HAL and Mbed TLS versions; record compiler/linker output and memory usage.
6. Verify boot OFF, UART loss OFF, bad HMAC no actuation, replay no actuation and persistent replay rejection after reset using low-voltage test hardware.
