#ifndef SX_TEST_MOCK_MAIN_H
#define SX_TEST_MOCK_MAIN_H

#include <stdint.h>

typedef int HAL_StatusTypeDef;
#define HAL_OK 0

typedef struct { volatile uint32_t ARR; volatile uint32_t CCR3; } TIM_TypeDef;
typedef struct { volatile uint32_t ODR; } GPIO_TypeDef;
typedef struct { void *Instance; } UART_HandleTypeDef;
typedef struct { TIM_TypeDef *Instance; } TIM_HandleTypeDef;
typedef struct { uint32_t Pin; uint32_t Mode; uint32_t Pull; uint32_t Speed; } GPIO_InitTypeDef;

extern GPIO_TypeDef sx_mock_gpio_b;
#define GPIOB (&sx_mock_gpio_b)
#define USART1 ((void *)0x40011000u)

#define GPIO_PIN_0 0x0001u
#define GPIO_PIN_RESET 0u
#define GPIO_MODE_OUTPUT_PP 1u
#define GPIO_NOPULL 0u
#define GPIO_SPEED_FREQ_LOW 0u
#define TIM_CHANNEL_3 3u

#define __HAL_RCC_GPIOB_CLK_ENABLE() ((void)0)
#define __HAL_TIM_SET_COMPARE(htim, channel, value) \
    do { (void)(channel); (htim)->Instance->CCR3 = (uint32_t)(value); } while (0)
#define __HAL_TIM_GET_AUTORELOAD(htim) ((htim)->Instance->ARR)
#define __DMB() ((void)0)

void HAL_GPIO_WritePin(GPIO_TypeDef *gpio, uint32_t pin, uint32_t state);
void HAL_GPIO_Init(GPIO_TypeDef *gpio, GPIO_InitTypeDef *init);
HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *tim, uint32_t channel);
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *uart, uint8_t *data, uint16_t size);
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *uart);
uint32_t HAL_GetTick(void);

#endif /* SX_TEST_MOCK_MAIN_H */
