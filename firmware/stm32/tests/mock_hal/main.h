#ifndef SX_TEST_MOCK_MAIN_H
#define SX_TEST_MOCK_MAIN_H

#include <stdint.h>

typedef enum { HAL_OK = 0, HAL_ERROR = 1, HAL_BUSY = 2, HAL_TIMEOUT = 3 } HAL_StatusTypeDef;
#define I2C_MEMADD_SIZE_16BIT 2u

typedef struct { volatile uint32_t ARR; volatile uint32_t CCR3; } TIM_TypeDef;
typedef struct { volatile uint32_t ODR; } GPIO_TypeDef;
typedef struct { void *Instance; } UART_HandleTypeDef;
typedef struct { TIM_TypeDef *Instance; } TIM_HandleTypeDef;
typedef struct { void *Instance; } I2C_HandleTypeDef;
typedef struct { uint32_t Pin; uint32_t Mode; uint32_t Pull; uint32_t Speed; } GPIO_InitTypeDef;

extern GPIO_TypeDef sx_mock_gpio_a;
extern GPIO_TypeDef sx_mock_gpio_b;
extern GPIO_TypeDef sx_mock_gpio_c;
#define GPIOA (&sx_mock_gpio_a)
#define GPIOB (&sx_mock_gpio_b)
#define GPIOC (&sx_mock_gpio_c)
#define USART1 ((void *)0x40011000u)

#define GPIO_PIN_0 0x0001u
#define GPIO_PIN_1 0x0002u
#define GPIO_PIN_RESET 0u
#define GPIO_PIN_SET 1u
#define GPIO_MODE_OUTPUT_PP 1u
#define GPIO_NOPULL 0u
#define GPIO_SPEED_FREQ_LOW 0u
#define TIM_CHANNEL_3 3u

#define __HAL_RCC_GPIOA_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOB_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOC_CLK_ENABLE() ((void)0)
#define __HAL_TIM_SET_COMPARE(htim, channel, value) \
    do { (void)(channel); (htim)->Instance->CCR3 = (uint32_t)(value); } while (0)
#define __HAL_TIM_GET_AUTORELOAD(htim) ((htim)->Instance->ARR)
#define __DMB() ((void)0)

void HAL_GPIO_WritePin(GPIO_TypeDef *gpio, uint32_t pin, uint32_t state);
void HAL_GPIO_Init(GPIO_TypeDef *gpio, GPIO_InitTypeDef *init);
HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *tim, uint32_t channel);
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *uart, uint8_t *data, uint16_t size);
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *uart);
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *i2c, uint16_t address,
                                   uint16_t memory_address, uint16_t memory_address_size,
                                   uint8_t *data, uint16_t size, uint32_t timeout);
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *i2c, uint16_t address,
                                    uint16_t memory_address, uint16_t memory_address_size,
                                    uint8_t *data, uint16_t size, uint32_t timeout);
HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *i2c, uint16_t address,
                                        uint32_t trials, uint32_t timeout);
uint32_t HAL_GetTick(void);
void HAL_Delay(uint32_t delay_ms);

#endif /* SX_TEST_MOCK_MAIN_H */
