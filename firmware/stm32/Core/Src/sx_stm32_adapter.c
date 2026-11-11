#include "sx_stm32_adapter.h"

#include "sx_ring_buffer.h"
#include "sx_uart_frame.h"

extern UART_HandleTypeDef huart1;
extern TIM_HandleTypeDef htim3;

static uint8_t sx_uart_rx_byte;
static sx_byte_ring_t sx_uart_rx_ring;
static sx_uart_parser_t sx_uart_parser;
static bool sx_uart_adapter_initialized;
static volatile uint32_t sx_last_rx_byte_ms;

static void sx_output_hal_off(void *context)
{
    (void)context;
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, 0u);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
}

static void sx_output_hal_set_level(void *context, uint16_t level_permille)
{
    uint32_t arr;
    uint32_t compare;
    (void)context;

    if (level_permille == 0u) {
        sx_output_hal_off(NULL);
        return;
    }

    arr = __HAL_TIM_GET_AUTORELOAD(&htim3);
    compare = ((arr + 1u) * (uint32_t)level_permille) / 1000u;
    if (compare > 65535u) {
        compare = 65535u;
    }
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, compare);
}

void SX_STM32_ForceOutputOffEarly(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    /* Preload ODR low before switching PB0 to output mode. */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
    gpio.Pin = GPIO_PIN_0;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &gpio);
}

HAL_StatusTypeDef SX_STM32_InitOutputSafe(void)
{
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, 0u);
    sx_output_hal_off(NULL);
    return HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
}

void SX_STM32_GetOutputInterface(sx_output_interface_t *output)
{
    if (output == NULL) {
        return;
    }
    output->context = NULL;
    output->force_off = sx_output_hal_off;
    output->set_level_permille = sx_output_hal_set_level;
}

HAL_StatusTypeDef SX_STM32_StartUartRx(void)
{
    sx_byte_ring_init(&sx_uart_rx_ring);
    sx_uart_parser_init(&sx_uart_parser);
    sx_uart_adapter_initialized = true;
    return HAL_UART_Receive_IT(&huart1, &sx_uart_rx_byte, 1u);
}

void SX_STM32_UartRxCompleteCallback(UART_HandleTypeDef *huart)
{
    if ((huart == NULL) || (huart->Instance != USART1) || !sx_uart_adapter_initialized) {
        return;
    }
    if (!sx_byte_ring_push_isr(&sx_uart_rx_ring, sx_uart_rx_byte)) {
        /* Overflow is handled in foreground; no parsing, crypto or actuation in this callback. */
    }
    sx_last_rx_byte_ms = HAL_GetTick();
    (void)HAL_UART_Receive_IT(&huart1, &sx_uart_rx_byte, 1u);
}

void SX_STM32_UartErrorCallback(UART_HandleTypeDef *huart)
{
    if ((huart == NULL) || (huart->Instance != USART1) || !sx_uart_adapter_initialized) {
        return;
    }
    sx_uart_rx_ring.overflow = true;
    (void)HAL_UART_AbortReceive(&huart1);
    (void)HAL_UART_Receive_IT(&huart1, &sx_uart_rx_byte, 1u);
}

void SX_STM32_Process(sx_app_core_t *app, uint32_t now_ms)
{
    uint8_t byte;
    sx_uart_frame_t frame;

    if ((app == NULL) || !sx_uart_adapter_initialized) {
        return;
    }

    if (sx_byte_ring_take_overflow(&sx_uart_rx_ring)) {
        sx_uart_parser_reset(&sx_uart_parser);
        sx_app_force_off(app);
    }
    if (sx_uart_parser_is_active(&sx_uart_parser) &&
        ((uint32_t)(now_ms - sx_last_rx_byte_ms) > SX_UART_FRAME_TIMEOUT_MS)) {
        sx_uart_parser_reset(&sx_uart_parser);
    }

    while (sx_byte_ring_pop(&sx_uart_rx_ring, &byte)) {
        sx_uart_parse_result_t parsed = sx_uart_parser_feed(&sx_uart_parser, byte, &frame);
        if (parsed == SX_UART_PARSE_COMPLETE) {
            if (frame.frame_type == SX_UART_FRAME_LINK_STATUS) {
                sx_app_process_link_status(app, frame.payload, now_ms);
            } else if (frame.frame_type == SX_UART_FRAME_AUTH_ENVELOPE) {
                /* HMAC, sequence persistence and command handling run in foreground context. */
                (void)sx_app_process_envelope(app, frame.payload, frame.payload_len, now_ms);
            }
        }
    }

    sx_app_tick(app, now_ms);
}
