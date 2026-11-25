# SENTINEL-X STM32 Core v0.2

This is an incremental firmware package based on STM32 Core v0.1. It keeps the existing EnvelopeV1, UART, HMAC adapter and application design, and adds:

1. AT24C256 page-bounded I2C access;
2. wear-levelled, fail-closed sequence journals for both sequence directions;
3. dual-slot persistent PolicyV1 storage;
4. a TB6612FNG motor driver for one fixed forward direction;
5. boolean output callbacks so a hardware adapter error is not reported as a successful command;
6. a fault latch that prevents further EEPROM reads/commits after ambiguous I/O until reinitialization and journal rescan;
7. host tests for EEPROM page splitting, journal reboot/rollover, interrupted and ambiguous commits, corruption fail-closed, policy persistence and motor safe ordering.

## Integration

Use `apply_to_repo.ps1` (Windows PowerShell) or manually copy only `sx_*.h` from package `Core/Inc` and `sx_*.c` from package `Core/Src` into the matching repository directories. Preserve CubeMX-generated `main.c`, `main.h`, `.ioc`, HAL files and the repository's existing shared protocol header at `protocol/c/sentinel_x_protocol_v1.h`. Do not copy `tests/mock_hal` into the CubeIDE target build. Use the package's `protocol/` copies only for standalone package tests; do not overwrite the repository's canonical protocol header/vector with them.

The NUCLEO pin proposal is PB0 TIM3_CH3, PA0 AIN1, PA1 AIN2, PC0 STBY, PB8/PB9 I2C1 and PB6/PB7 USART1. Confirm the `.ioc` and board wiring before connecting the motor or EEPROM.

## Deliberately incomplete security integrations

- AT24C256 blank provisioning is a manual factory-only API; never call it on every boot.
- The key provider and rate limiter return false by design. No actuator command can be accepted until real device-key provisioning and the rate-limit decision are implemented. Any uncertain EEPROM I/O error latches the persistence object faulted; reinitialize and rescan after reboot/recovery before considering further commands.
- Policy must be stored before actuation is permitted.
- Outbound ACK/EVENT/STATUS serialization/transmission, Modbus relay control and physical test evidence are not part of v0.2.

See `APPLY_TO_REPO.md`, `firmware/stm32/README.md` and `firmware/stm32/docs/` for integration details.
