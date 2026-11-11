# SENTINEL-X STM32 Core v0.1

Initial STM32F411RE security-enforcement core and bring-up material for the `develop` branch.

## Contents

- `firmware/stm32/Core`: portable C parser, EnvelopeV1 validator, PolicyV1 validator, HMAC/Mbed TLS adapter, persistent replay guard, application core, byte ring and Cube HAL adapter.
- `firmware/stm32/docs`: CubeIDE setup and generated-code integration instructions.
- `firmware/stm32/tests`: host unit tests, HMAC vector test, HAL/Mbed TLS declaration shims and Windows PowerShell/Bash runners.
- `protocol`: mirror copies of the shared protocol C header and public HMAC test vector to make the archive self-testable. These files already exist in the live repo and should remain governed by the repo's source of truth.
- `VALIDATION.md`: exact checks run and limitations.
- `APPLY_TO_REPO.md`: feature-branch patch workflow.

## Validation result

`./firmware/stm32/tests/run_host_tests.sh` passes: **68 C assertions, 0 failures**, and the HMAC vector test passes. The HAL and Mbed TLS code received syntax checks against declaration-only shims. This is not a target CubeIDE build or a physical-board validation.

## Safety status

The default sequence-store/key/policy/rate-limit port template returns `false`. This is deliberate: no network command can actuate the PWM output until real persistent storage, a device-specific key, a persisted valid policy and the rate limiter are implemented. Never substitute the public test vector key for a device key.
