# STM32CubeIDE setup — NUCLEO-F411RE

This document describes the local CubeIDE project generation. A generated `.ioc` is deliberately not committed in this initial slice because the installed STM32CubeF4 package/CubeIDE version and board clock configuration have not been confirmed on the developer machine.

## 1. Create the project

1. Open STM32CubeIDE → **File → New → STM32 Project**.
2. In **Board Selector**, search for `NUCLEO-F411RE` and select the matching board. If selecting by MCU instead, use `STM32F411RETx` and verify the package is for STM32F4.
3. Set project name to `SENTINEL_X_STM32` and use the STM32CubeIDE toolchain.
4. Keep the project in the SENTINEL-X workspace, preferably under `firmware/stm32/` (or create it there after ensuring the IDE does not overwrite the source folders).
5. Choose C, not C++, and do not enable an RTOS in this initial bring-up.

## 2. GPIO and fail-safe output

**Baseline output:** `PB0 = TIM3_CH3`, low-voltage PWM only. The output is active-high in this proposed baseline.

- Configure PB0 as `TIM3 Channel 3 PWM Generation CH3` (alternate function AF2).
- Configure PWM mode 1, output polarity HIGH, fast mode disabled, and initial pulse/CCR = `0`.
- Keep the timer output disabled until the application has forced CCR to zero.
- `Core/Src/sx_stm32_adapter.c` calls `__HAL_TIM_SET_COMPARE(..., TIM_CHANNEL_3, 0)` before starting PWM.
- Add an external pull-down (for example 10 kΩ) at the low-voltage driver input so the signal remains OFF while the MCU is reset or unpowered. Confirm the exact electrical design with the driver circuit.
- Do not connect mains loads or drive an actuator directly from the MCU pin.

Suggested PWM frequency: **1 kHz**, subject to the timer clock. Use:

`f_PWM = f_TIM3 / ((PSC + 1) × (ARR + 1))`

For example, if the actual TIM3 input clock is 84 MHz, `PSC=83` and `ARR=999` produce a 1 kHz PWM. Do not assume 84 MHz; calculate `PSC` from the clock tree configured in this `.ioc`.

## 3. USART1

Configure `USART1` in Asynchronous mode:

| Parameter | Value |
|---|---|
| TX pin | PB6 / AF7 |
| RX pin | PB7 / AF7 |
| Baud | 115200 |
| Word length | 8 bits |
| Parity | None |
| Stop bits | 1 |
| Hardware flow control | None |
| Oversampling | 16 |
| RX interrupt | Enabled in NVIC |

External wiring to ESP32-S3 is crossed:

- STM32 PB6 / USART1_TX → ESP32-S3 UART RX (baseline GPIO18)
- STM32 PB7 / USART1_RX ← ESP32-S3 UART TX (baseline GPIO17)
- Common GND; both sides use 3.3 V logic.

The ESP32-S3 GPIO mapping is proposed and must be checked against the exact DevKit board before wiring.

## 4. Timer clock / PWM setup

Configure TIM3 channel 3 as PWM1. The adapter calculates CCR from the configured ARR, so it does not assume a particular system-clock frequency. Set ARR/PSC so the resulting PWM frequency matches the output stage's requirements. Set initial CCR to zero in CubeMX and keep it zero in the generated initialization code.

## 5. Other pins reserved by the Blueprint

These are the current proposed assignments; they are not all used by this core slice:

| Function | STM32 pin |
|---|---|
| TFT SPI2 SCK / MOSI / MISO | PB13 / PB15 / PB14 |
| TFT CS / DC / RESET / BACKLIGHT | PB12 / PA9 / PC6 / PC7 |
| FRAM I2C1 SCL / SDA | PB8 / PB9 |
| RGB LED R / G / B | PA8 / PB1 / PA10 |
| Buzzer | PB5 |
| User button | PC13 |

Do not configure these until checking for conflicts with the selected board, TFT/FRAM breakout and any debug interface.

## 6. Compiler include paths and source files

Add the shared protocol header directory to the compiler include paths:

`../../protocol/c`

Add the files from `firmware/stm32/Core/Inc` and `firmware/stm32/Core/Src` to the CubeIDE project. The portable host-tested core files are:

- `sx_uart_frame.c`
- `sx_envelope.c`
- `sx_replay_guard.c`
- `sx_app_core.c`
- `sx_ring_buffer.c`

The HAL-specific adapter `sx_stm32_adapter.c` requires the generated `main.h`, `huart1` and `htim3` handles.

## 7. Mbed TLS

Add a maintained Mbed TLS build compatible with the version selected for the project. The initial crypto adapter requires SHA-256/HMAC support (`MBEDTLS_MD_C` and `MBEDTLS_SHA256_C`) and the header `mbedtls/md.h`. Add the matching Mbed TLS include and source/library paths in CubeIDE.

` sx_crypto_mbedtls.c` is intentionally excluded from the host GCC test target because Mbed TLS is not vendored in this repository. Target compilation must be recorded after the team selects and adds the exact Mbed TLS version/configuration.

## 8. Generate code and verify

Generate code, then integrate the adapter using `INTEGRATION_NOTES.md`. Do not start by sending an actuator command. First verify PB0 remains low at reset/boot and on UART timeout with a multimeter or oscilloscope and a safe low-voltage test circuit.
