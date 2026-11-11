#ifndef SX_ACTUATOR_HAL_EXAMPLE_H
#define SX_ACTUATOR_HAL_EXAMPLE_H
#include "stm32f4xx_hal.h"
/* PROPOSED per Blueprint: PB0 = TIM3_CH3 (AF2). Verify on the exact board. */
HAL_StatusTypeDef sx_actuator_force_off_before_network(void);
HAL_StatusTypeDef sx_actuator_pwm_set_permille(TIM_HandleTypeDef *htim3, uint16_t level_permille);
#endif
