# TB6612FNG motor output — STM32 Core v0.2

## 1. MVP behavior

The first hardware actuator is one two-wire brushed DC motor connected to TB6612FNG channel A.

- `SET_OUTPUT(output_id=1, state=0)`: stop; force STBY low and PWM=0.
- `SET_OUTPUT(output_id=1, state=1)`: run forward using the last nonzero level (default 1000 permille).
- `SET_LEVEL(channel_id=1, level_permille=0)`: stop.
- `SET_LEVEL(channel_id=1, level_permille=1..1000)`: run forward at that PWM duty setting.
- `GET_STATUS`: returns application output state/level once the response path is implemented.

This is a one-direction MVP. Reverse, active braking, current sensing, stall detection and motor fault telemetry are not implemented. Duty percentage is not a calibrated RPM value.

## 2. Proposed NUCLEO-F411RE pin assignment

These pins are proposals only. Confirm them in CubeMX and on the exact NUCLEO board before wiring.

| TB6612 signal | STM32 pin | Function |
|---|---|---|
| PWMA | PB0 | TIM3_CH3 PWM |
| AIN1 | PA0 | GPIO output |
| AIN2 | PA1 | GPIO output |
| STBY | PC0 | GPIO output; low = standby |
| GND | GND | Common logic/motor ground |

Other current project proposals include USART1 PB6/PB7 and I2C1 PB8/PB9. Check the complete `.ioc` pinout for conflicts before generating code.

## 3. Safe-state sequence

`SX_STM32_ForceOutputOffEarly()` is called immediately after `HAL_Init()`:

1. Enable GPIOA/GPIOB/GPIOC clocks.
2. Preload AIN1, AIN2, STBY and PB0 low.
3. Configure the control pins as push-pull outputs, initially low.
4. CubeMX later configures PB0 as TIM3_CH3 alternate function; TIM3 CCR must stay zero before PWM start.
5. `SX_STM32_InitOutputSafe()` initializes the portable driver with STBY low and starts TIM3_CH3 only after compare is zero.

When a nonzero level is authorized, the driver keeps STBY low while it sets PWM=0, AIN1=1/AIN2=0, sets the requested PWM compare, and raises STBY last. On any callback failure it attempts emergency stop and reports command execution failure to the application.

Add an external pull-down (for example 10 kΩ, subject to module/board validation) on STBY so the driver remains in standby while the MCU pins are reset/high-impedance. The TB6612FNG has an internal STBY pull-down, but the module schematic must be checked.

## 4. Power and wiring cautions

- VCC is the logic supply; VM is the motor supply. Verify the exact breakout board labels and supported voltages.
- Do not power the motor from an STM32 GPIO or the NUCLEO 3.3 V pin.
- Motor supply ground and MCU logic ground must be common unless the system intentionally uses isolation.
- Check the motor stall current against the TB6612FNG/module continuous and peak current limits and thermal conditions.
- Place the motor supply decoupling close to the driver module; follow the exact module/IC datasheet.
- Test with a low-voltage supply and the motor unloaded first. Keep the relay/mains path out of this prototype test.

## 5. CubeMX settings

- PB0: TIM3_CH3, PWM Generation CH3.
- PA0, PA1, PC0: GPIO_Output, initial output level Low.
- TIM3: PWM Mode 1, active-high output; set PSC/ARR from the actual timer clock and required motor PWM frequency. 1 kHz is only a starting proposal, not a final motor specification.
- Use CCR=0 at initialization. Confirm PB0 alternate-function mapping and timer clock in the generated `.ioc`.

## 6. Code modules

- `sx_tb6612.c/.h`: portable driver and safe command ordering; unit tested with callback mocks.
- `sx_stm32_adapter.c`: HAL implementation for PA0/PA1/PC0 and PB0/TIM3_CH3; syntax checked against a declaration-only HAL shim, not built with the real Cube HAL yet.
- `sx_app_core.c`: output callbacks now return `bool`, so a failed motor operation is not reported as successful command execution.

## 7. Datasheet reference

The control pin names and STBY standby behavior follow the Toshiba TB6612FNG datasheet. The IC datasheet lists STBY low as standby and identifies PWMA/AIN1/AIN2 as channel-A control inputs. The IC's supply ranges do not automatically guarantee that a third-party breakout module has the same wiring or onboard regulator arrangement; inspect the exact module schematic/labels before connecting it.

- Toshiba, TB6612FNG datasheet: https://toshiba.semicon-storage.com/info/TB6612FNG_datasheet_en_20141001.pdf?did=10660
