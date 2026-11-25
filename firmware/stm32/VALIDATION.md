# STM32 Core v0.2 validation record

## Host test results

Run command:

```bash
bash firmware/stm32/tests/run_host_tests.sh
```

Observed results in the current build environment:

- STM32 application/protocol C host tests: 73 checks, 0 failures.
- EEPROM/persistence/TB6612 C host tests: 111 checks, 0 failures.
- Public Python HMAC vector: PASS (valid tag and one-bit mutation).
- HAL / I2C / platform-port / Mbed TLS source syntax checks against declaration-only mock headers: PASS.

AddressSanitizer and UndefinedBehaviorSanitizer: 73 core checks and 111 hardware-driver checks passed in the current Linux build environment.

## What these results demonstrate

- AT24C256 generic driver splits writes at 64-byte page boundaries and rejects out-of-range access.
- The in-memory EEPROM model tests factory provisioning, inbound/outbound sequence persistence, ring rollover, reboot reload, interrupted sequence commit, ambiguous commit-marker completion and committed-record corruption fail-closed.
- Policy storage tests commit/load, two-slot alternation, monotonic version behavior and ambiguous commit handling.
- An ambiguous EEPROM write (including a commit marker that may have reached the chip even when the callback reports failure) latches storage faulted until reinitialization/re-scan; same-session commits are refused.
- The TB6612 portable driver tests STBY-low sequencing, fixed-direction setup, duty application, force-off and callback-failure handling.
- The application reports `EXECUTION_FAILED` when a motor output callback fails rather than claiming success.

## What these results do NOT demonstrate

- The HAL I2C adapter has not been built against the actual STM32CubeF4 HAL package or run against the actual EEPROM module.
- The motor HAL adapter has not been built with the real CubeMX-generated project or tested on a NUCLEO-F411RE/TB6612/motor assembly.
- No real power-cut, brown-out, EEPROM retention/endurance or motor stall-current test has been performed.
- Mbed TLS syntax is checked against a mock header; real Mbed TLS has not been linked into the target firmware.
- Device-key provisioning and rate limiter are deliberately deny-by-default; actuator commands remain disabled.

Do not treat host tests as hardware certification or as evidence that the final system is ready for actuation.
