#include "sx_stm32_adapter.h"

#include "sx_board_config.h"
#include "sx_ring_buffer.h"
#include "sx_tb6612.h"
#include "sx_uart_frame.h"

extern UART_HandleTypeDef huart1;
extern TIM_HandleTypeDef htim3;

static uint8_t sx_uart_rx_byte;
static sx_byte_ring_t sx_uart_rx_ring;
static sx_uart_parser_t sx_uart_parser;
static sx_tb6612_t sx_motor;
static bool sx_uart_adapter_initialized;
static bool sx_motor_adapter_initialized;
static volatile uint32_t sx_last_rx_byte_ms;

static bool sx_motor_set_standby(void *context, bool enabled)
{
    (void)context;
    HAL_GPIO_WritePin(SX_TB6612_STBY_GPIO_PORT, SX_TB6612_STBY_GPIO_PIN,
                      enabled ? GPIO_PIN_SET : GPIO_PIN_RESET);
    return true;
}

static bool sx_motor_set_inputs(void *context, bool ain1, bool ain2)
{
    (void)context;
    HAL_GPIO_WritePin(SX_TB6612_AIN1_GPIO_PORT, SX_TB6612_AIN1_GPIO_PIN,
                      ain1 ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SX_TB6612_AIN2_GPIO_PORT, SX_TB6612_AIN2_GPIO_PIN,
                      ain2 ? GPIO_PIN_SET : GPIO_PIN_RESET);
    return true;
}

static bool sx_motor_set_pwm(void *context, uint16_t level_permille)
{
    uint32_t arr;
    uint32_t compare;
    (void)context;

    if ((htim3.Instance == NULL) || (level_permille > 1000u)) return false;
    if (level_permille == 0u) {
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, 0u);
        return true;
    }

    arr = __HAL_TIM_GET_AUTORELOAD(&htim3);
    compare = (((arr + 1u) * (uint32_t)level_permille) + 500u) / 1000u;
    if (compare > 65535u) compare = 65535u;
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, compare);
    return true;
}

static bool sx_output_hal_off(void *context)
{
    (void)context;
    return sx_motor_adapter_initialized && sx_tb6612_force_off(&sx_motor);
}

static bool sx_output_hal_set_level(void *context, uint16_t level_permille)
{
    (void)context;
    return sx_motor_adapter_initialized &&
           sx_tb6612_set_forward_level(&sx_motor, level_permille);
}

void SX_STM32_ForceOutputOffEarly(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* Preload all TB6612 control pins low before changing pin modes. */
    HAL_GPIO_WritePin(SX_TB6612_AIN1_GPIO_PORT, SX_TB6612_AIN1_GPIO_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SX_TB6612_AIN2_GPIO_PORT, SX_TB6612_AIN2_GPIO_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SX_TB6612_STBY_GPIO_PORT, SX_TB6612_STBY_GPIO_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SX_TB6612_PWM_GPIO_PORT, SX_TB6612_PWM_GPIO_PIN, GPIO_PIN_RESET);

    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    gpio.Pin = SX_TB6612_AIN1_GPIO_PIN | SX_TB6612_AIN2_GPIO_PIN;
    HAL_GPIO_Init(SX_TB6612_AIN1_GPIO_PORT, &gpio);
    gpio.Pin = SX_TB6612_STBY_GPIO_PIN;
    HAL_GPIO_Init(SX_TB6612_STBY_GPIO_PORT, &gpio);
    gpio.Pin = SX_TB6612_PWM_GPIO_PIN;
    HAL_GPIO_Init(SX_TB6612_PWM_GPIO_PORT, &gpio);
}

HAL_StatusTypeDef SX_STM32_InitOutputSafe(void)
{
    sx_tb6612_io_t io;

    if (htim3.Instance == NULL) return HAL_ERROR;

    io.context = NULL;
    io.set_standby = sx_motor_set_standby;
    io.set_inputs = sx_motor_set_inputs;
    io.set_pwm_permille = sx_motor_set_pwm;

    sx_motor_adapter_initialized = false;
    if (!sx_tb6612_init(&sx_motor, &io)) return HAL_ERROR;
    sx_motor_adapter_initialized = true;

    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, 0u);
    if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3) != HAL_OK) {
        (void)sx_tb6612_force_off(&sx_motor);
        sx_motor_adapter_initialized = false;
        return HAL_ERROR;
    }
    return HAL_OK;
}

void SX_STM32_GetOutputInterface(sx_output_interface_t *output)
{
    if (output == NULL) return;
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
    if ((huart == NULL) || (huart->Instance != USART1) || !sx_uart_adapter_initialized) return;
    if (!sx_byte_ring_push_isr(&sx_uart_rx_ring, sx_uart_rx_byte)) {
        /* Overflow is handled by foreground code; no crypto or actuator work in ISR. */
    }
    sx_last_rx_byte_ms = HAL_GetTick();
    (void)HAL_UART_Receive_IT(&huart1, &sx_uart_rx_byte, 1u);
}

void SX_STM32_UartErrorCallback(UART_HandleTypeDef *huart)
{
    if ((huart == NULL) || (huart->Instance != USART1) || !sx_uart_adapter_initialized) return;
    sx_uart_rx_ring.overflow = true;
    (void)HAL_UART_AbortReceive(&huart1);
    (void)HAL_UART_Receive_IT(&huart1, &sx_uart_rx_byte, 1u);
}

void SX_STM32_Process(sx_app_core_t *app, uint32_t now_ms)
{
    uint8_t byte;
    sx_uart_frame_t frame;

    if ((app == NULL) || !sx_uart_adapter_initialized) return;

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
                (void)sx_app_process_envelope(app, frame.payload, frame.payload_len, now_ms);
            }
        }
    }

    sx_app_tick(app, now_ms);
}
