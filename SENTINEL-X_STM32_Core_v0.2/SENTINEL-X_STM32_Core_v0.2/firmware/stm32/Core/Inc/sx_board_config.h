#ifndef SX_BOARD_CONFIG_H
#define SX_BOARD_CONFIG_H

#include "stm32f4xx_hal.h"

/*
 * NUCLEO-F411RE baseline pin proposal. Verify in CubeMX and against the exact
 * board/module wiring before connecting hardware. These pins are not measured.
 *
 * TB6612 channel A:
 *   PWMA -> PB0 / TIM3_CH3
 *   AIN1 -> PA0
 *   AIN2 -> PA1
 *   STBY -> PC0
 *
 * AT24C256:
 *   I2C1 SCL -> PB8
 *   I2C1 SDA -> PB9
 *   Address 0x50 assumes A2/A1/A0 are all low. Change if the module straps differ.
 */
#define SX_TB6612_PWM_GPIO_PORT       GPIOB
#define SX_TB6612_PWM_GPIO_PIN        GPIO_PIN_0
#define SX_TB6612_AIN1_GPIO_PORT      GPIOA
#define SX_TB6612_AIN1_GPIO_PIN       GPIO_PIN_0
#define SX_TB6612_AIN2_GPIO_PORT      GPIOA
#define SX_TB6612_AIN2_GPIO_PIN       GPIO_PIN_1
#define SX_TB6612_STBY_GPIO_PORT      GPIOC
#define SX_TB6612_STBY_GPIO_PIN       GPIO_PIN_0

#define SX_AT24C256_I2C_ADDRESS_7BIT  0x50u
#define SX_AT24C256_I2C_TIMEOUT_MS    25u

#endif /* SX_BOARD_CONFIG_H */
