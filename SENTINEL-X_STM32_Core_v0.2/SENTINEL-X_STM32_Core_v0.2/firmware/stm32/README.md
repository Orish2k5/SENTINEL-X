# SENTINEL-X STM32 Core v0.2

MCU-side firmware foundation for the STM32F411RE / NUCLEO-F411RE prototype. This package adds an AT24C256 I2C storage backend with wear-levelled sequence journals and a TB6612FNG motor output driver to the v0.1 application/protocol core.

## Hardware proposal (not yet bench-verified)

| Function | Proposed pin / setting |
|---|---|
| TB6612 PWMA | PB0 / TIM3_CH3 |
| TB6612 AIN1 | PA0 GPIO output |
| TB6612 AIN2 | PA1 GPIO output |
| TB6612 STBY | PC0 GPIO output, low = standby |
| ESP32 UART | USART1 PB6 TX / PB7 RX, 115200 8N1 |
| AT24C256 | I2C1 PB8 SCL / PB9 SDA, start at 100 kHz |
| EEPROM 7-bit address | `0x50` default when A2/A1/A0 are low |

Confirm the final pinout in CubeMX and against the exact NUCLEO revision, breakout board and module pull-ups before wiring.

## Source modules

- `sx_uart_frame.c`: streaming UART frame parser and CRC16/CCITT-FALSE.
- `sx_envelope.c`: EnvelopeV1, command and PolicyV1 validation.
- `sx_crypto_mbedtls.c`: HMAC-SHA256 verification adapter (real Mbed TLS is an external dependency).
- `sx_app_core.c`: command validation/orchestration, safe state, output callback failures.
- `sx_replay_guard.c`: persist-before-actuation anti-replay guard.
- `sx_ring_buffer.c`: bounded UART byte queue.
- `sx_at24c256.c` / `sx_at24c256_hal.c`: page-bounded EEPROM access and STM32 HAL I2C adapter.
- `sx_persistence_at24c256.c`: circular inbound/outbound sequence journals and two-slot policy journal.
- `sx_platform_port_at24c256.c`: binds EEPROM persistence to the app; key and rate-limit providers remain deny-by-default.
- `sx_tb6612.c`: portable motor driver; safe standby ordering and one fixed direction.
- `sx_stm32_adapter.c`: board HAL adapter for PB0/PA0/PA1/PC0, UART RX and foreground processing.

## Critical safety status

The package is intentionally not ready to actuate a physical motor. The following are still deliberately disabled or incomplete:

- key provider returns false; no device key is stored or provisioned;
- rate-limit callback returns false until the algorithm conflict is resolved;
- EEPROM sequence journal must be factory-provisioned through an authorized process; blank/corrupt state never becomes sequence zero automatically;
- policy must be persisted before actuator commands can be accepted;
- MCU→Edge outbound envelopes/ACK/EVENT/STATUS generation and UART TX remain future work;
- no relay Modbus master is included in this version.

The TB6612 driver can be unit-tested with callbacks, but actual GPIO/PWM, motor current, power and reset behavior have not been validated on a board.

## Host tests

From the repository root on Windows PowerShell:

```powershell
.\firmware\stm32\tests\run_host_tests.ps1
```

Or on Linux / WSL / Git Bash:

```bash
./firmware/stm32/tests/run_host_tests.sh
```

Tests compile portable C modules with strict warnings and mock the I2C EEPROM and TB6612 GPIO/PWM callbacks. HAL-specific source is syntax-checked against declaration-only mock headers. This does not replace building with the real STM32F4 HAL, linking real Mbed TLS or testing the physical board.

## Integration docs

- `docs/CUBEIDE_SETUP.md`
- `docs/INTEGRATION_NOTES.md`
- `docs/AT24C256_JOURNAL.md`
- `docs/TB6612_MOTOR.md`
- `VALIDATION.md` at package root
