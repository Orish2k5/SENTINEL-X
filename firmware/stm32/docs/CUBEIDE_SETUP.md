# STM32CubeIDE setup — SENTINEL-X STM32 Core v0.2

## 1. Create the project

1. Create a project for `NUCLEO-F411RE` / `STM32F411RETx` in STM32CubeIDE or STM32CubeMX.
2. Use C and the STM32CubeIDE ARM GCC toolchain.
3. Place the project in `firmware/stm32/` so the repository paths remain `firmware/stm32/Core/Inc` and `firmware/stm32/Core/Src`.
4. Do not create a second `Core/Core` folder. If CubeIDE cannot create directly in the existing directory, generate in a temporary directory and move the project root carefully.

## 2. Pin configuration proposal

| Pin | CubeMX function | Purpose |
|---|---|---|
| PB0 | TIM3_CH3 | TB6612 PWMA |
| PA0 | GPIO_Output | TB6612 AIN1 |
| PA1 | GPIO_Output | TB6612 AIN2 |
| PC0 | GPIO_Output | TB6612 STBY |
| PB6 | USART1_TX | ESP32 UART |
| PB7 | USART1_RX | ESP32 UART |
| PB8 | I2C1_SCL | AT24C256 |
| PB9 | I2C1_SDA | AT24C256 |

These assignments are PROPOSED. Check the exact NUCLEO-F411RE board revision, all display/LED/button assignments and the live CubeMX pinout for conflicts. Set PA0, PA1 and PC0 initial output level to Low.

## 3. GPIO/PWM

- Configure PB0 as TIM3 Channel 3 PWM Generation CH3.
- PWM Mode 1, active high, initial Pulse/CCR = 0.
- Configure PA0, PA1 and PC0 as low-level GPIO outputs.
- Choose PSC/ARR only after confirming the actual APB1/TIM3 clock and the motor's acceptable PWM frequency. A 1 kHz value is an initial test proposal only.

## 4. USART1

- Asynchronous, 115200 baud, 8 data bits, no parity, 1 stop bit, no flow control.
- Enable USART1 global interrupt.
- Preserve the UART callback forwarding in `HAL_UART_RxCpltCallback` and `HAL_UART_ErrorCallback`; merge with existing callbacks rather than defining duplicates.

## 5. I2C1 for AT24C256

- PB8=SCL, PB9=SDA.
- Start at 100 kHz Standard Mode.
- 7-bit address default `0x50` if A2/A1/A0 are low; inspect the actual module jumpers/straps.
- The HAL adapter uses 16-bit EEPROM memory addressing.
- Verify EEPROM module VCC and pull-up voltage. Keep SDA/SCL at 3.3 V logic for direct STM32 connection unless level shifting is intentionally provided.
- Generate `hi2c1` and `MX_I2C1_Init()`.

## 6. Add the SENTINEL-X source

Copy the v0.2 `Core/Inc/sx_*.h` and `Core/Src/sx_*.c` files into the generated `Core/Inc` and `Core/Src`. Do not copy `tests/mock_hal` into the firmware target.

CubeMX generates `main.h` for the board application, and the STM32CubeF4 HAL package supplies `stm32f4xx_hal.h`. The v0.2 adapter header now includes `stm32f4xx_hal.h` directly rather than depending on the generated `main.h`; Cube-generated `main.h` remains part of the application project and is not copied from the package.

Add include path `../../protocol/c` relative to `firmware/stm32` (or use the correct workspace-relative path shown by the IDE). Keep the repository's existing `protocol/c/sentinel_x_protocol_v1.h`; do not overwrite it with the package's reference copy.

## 7. Mbed TLS

` sx_crypto_mbedtls.c` includes `mbedtls/md.h`. The package does not contain Mbed TLS. Add a real Mbed TLS version compatible with the project, its include directory, source/library objects built for Cortex-M4, and the required SHA-256/MD configuration. At minimum, check the build config for `MBEDTLS_MD_C` and `MBEDTLS_SHA256_C` (names depend on the selected Mbed TLS version).

A header alone is insufficient; the linker must receive the implementation of the Mbed TLS APIs. Never use the public protocol test key as the device key.

## 8. Platform storage initialization

Generate `hi2c1` and call `SX_Platform_AT24C256_Init(&hi2c1, SX_AT24C256_I2C_ADDRESS_7BIT)` after `MX_I2C1_Init()`.

The EEPROM sequence logs must be provisioned through an authorized factory commissioning process. `SX_Platform_AT24C256_ProvisionBlankDevice(0, 0)` is a one-time explicit operation and MUST NOT be called unconditionally on every boot. The package has no authenticated commissioning UI yet; do not expose this function to network/UART input.

The key provider and rate limiter deliberately return false. Therefore, the current integration cannot authorize actuator commands until a device-specific key provider and the chosen rate-limit implementation are added.

## 9. Build order

1. Generate CubeMX project and verify no pin conflicts.
2. Add source files and include paths.
3. Build portable modules with ARM GCC.
4. Integrate real Mbed TLS and rebuild.
5. Build the board HAL adapter against the actual STM32F4 HAL (not the mock header).
6. Flash with motor driver STBY held low and PWM compare 0.
7. Validate boot/reset/motor standby before implementing key provisioning or allowing commands.

Host tests do not replace target build or hardware bring-up.
