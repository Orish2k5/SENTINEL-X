#include "sx_actuator_hal_example.h"
#define ACT_GPIO_PORT GPIOB
#define ACT_GPIO_PIN GPIO_PIN_0
#define ACT_TIM_CHANNEL TIM_CHANNEL_3
static void configure_pb0_low(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    HAL_GPIO_WritePin(ACT_GPIO_PORT, ACT_GPIO_PIN, GPIO_PIN_RESET);
    gpio.Pin = ACT_GPIO_PIN; gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL; gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(ACT_GPIO_PORT, &gpio);
}
static void configure_pb0_tim3(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    gpio.Pin = ACT_GPIO_PIN; gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL; gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(ACT_GPIO_PORT, &gpio);
}
HAL_StatusTypeDef sx_actuator_force_off_before_network(void)
{
    configure_pb0_low();
    return HAL_OK;
}
HAL_StatusTypeDef sx_actuator_pwm_set_permille(TIM_HandleTypeDef *htim3, uint16_t level_permille)
{
    uint32_t arr, pulse; HAL_StatusTypeDef rc;
    if ((htim3 == NULL) || (level_permille > 1000u)) return HAL_ERROR;
    if (level_permille == 0u) {
        (void)HAL_TIM_PWM_Stop(htim3, ACT_TIM_CHANNEL);
        configure_pb0_low();
        __HAL_TIM_SET_COMPARE(htim3, ACT_TIM_CHANNEL, 0u);
        return HAL_OK;
    }
    arr = __HAL_TIM_GET_AUTORELOAD(htim3);
    pulse = (uint32_t)(((uint64_t)(arr + 1u) * level_permille) / 1000u);
    if (pulse > arr) pulse = arr;
    (void)HAL_TIM_PWM_Stop(htim3, ACT_TIM_CHANNEL);
    __HAL_TIM_SET_COMPARE(htim3, ACT_TIM_CHANNEL, pulse);
    configure_pb0_tim3();
    rc = HAL_TIM_PWM_Start(htim3, ACT_TIM_CHANNEL);
    if (rc != HAL_OK) {
        configure_pb0_low();
        __HAL_TIM_SET_COMPARE(htim3, ACT_TIM_CHANNEL, 0u);
    }
    return rc;
}
