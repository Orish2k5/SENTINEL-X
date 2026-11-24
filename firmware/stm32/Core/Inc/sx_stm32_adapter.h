#ifndef SX_STM32_ADAPTER_H
#define SX_STM32_ADAPTER_H

#include "main.h"
#include "sx_app_core.h"

/* Call immediately after HAL_Init(), before MX_GPIO_Init(). */
void SX_STM32_ForceOutputOffEarly(void);
/* Call after MX_GPIO_Init() and MX_TIM3_Init(); starts PWM at 0 permille. */
HAL_StatusTypeDef SX_STM32_InitOutputSafe(void);
/* Bind this interface into sx_app_core_init(). */
void SX_STM32_GetOutputInterface(sx_output_interface_t *output);
/* Start interrupt-driven USART1 RX; call once after MX_USART1_UART_Init(). */
HAL_StatusTypeDef SX_STM32_StartUartRx(void);
/* Call these from HAL_UART_RxCpltCallback / HAL_UART_ErrorCallback. */
void SX_STM32_UartRxCompleteCallback(UART_HandleTypeDef *huart);
void SX_STM32_UartErrorCallback(UART_HandleTypeDef *huart);
/* Call repeatedly from the foreground main loop, never from an ISR. */
void SX_STM32_Process(sx_app_core_t *app, uint32_t now_ms);

#endif /* SX_STM32_ADAPTER_H */
