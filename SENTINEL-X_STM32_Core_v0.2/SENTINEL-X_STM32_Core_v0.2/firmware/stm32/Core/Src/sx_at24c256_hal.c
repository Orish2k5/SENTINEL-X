#include "sx_at24c256_hal.h"

#include <stddef.h>

static uint16_t sx_at24c256_hal_device_address(const sx_at24c256_hal_t *context)
{
    return (uint16_t)((uint16_t)context->address_7bit << 1);
}

bool sx_at24c256_hal_configure(sx_at24c256_hal_t *context,
                               I2C_HandleTypeDef *i2c,
                               uint8_t address_7bit,
                               uint32_t timeout_ms)
{
    if ((context == NULL) || (i2c == NULL) || (address_7bit < 0x50u) ||
        (address_7bit > 0x57u) || (timeout_ms == 0u)) {
        return false;
    }
    context->i2c = i2c;
    context->address_7bit = address_7bit;
    context->timeout_ms = timeout_ms;
    return true;
}

bool sx_at24c256_hal_probe(sx_at24c256_hal_t *context)
{
    if ((context == NULL) || (context->i2c == NULL)) {
        return false;
    }
    return HAL_I2C_IsDeviceReady(context->i2c, sx_at24c256_hal_device_address(context),
                                 2u, context->timeout_ms) == HAL_OK;
}

bool sx_at24c256_hal_read(void *opaque, uint16_t address,
                          uint8_t *data, uint16_t length)
{
    sx_at24c256_hal_t *context = (sx_at24c256_hal_t *)opaque;
    if ((context == NULL) || (context->i2c == NULL) || (data == NULL) || (length == 0u)) {
        return false;
    }
    return HAL_I2C_Mem_Read(context->i2c, sx_at24c256_hal_device_address(context),
                            address, I2C_MEMADD_SIZE_16BIT, data, length,
                            context->timeout_ms) == HAL_OK;
}

bool sx_at24c256_hal_write(void *opaque, uint16_t address,
                           const uint8_t *data, uint16_t length)
{
    sx_at24c256_hal_t *context = (sx_at24c256_hal_t *)opaque;
    uint32_t start;

    if ((context == NULL) || (context->i2c == NULL) || (data == NULL) ||
        (length == 0u) || (length > SX_AT24C256_PAGE_SIZE) ||
        (((uint32_t)address % SX_AT24C256_PAGE_SIZE) + length > SX_AT24C256_PAGE_SIZE)) {
        return false;
    }

    if (HAL_I2C_Mem_Write(context->i2c, sx_at24c256_hal_device_address(context),
                          address, I2C_MEMADD_SIZE_16BIT, (uint8_t *)data, length,
                          context->timeout_ms) != HAL_OK) {
        return false;
    }

    /* EEPROM self-timed write cycle: poll ACK only from foreground context. */
    start = HAL_GetTick();
    do {
        if (HAL_I2C_IsDeviceReady(context->i2c, sx_at24c256_hal_device_address(context),
                                  1u, 1u) == HAL_OK) {
            return true;
        }
        HAL_Delay(1u);
    } while ((uint32_t)(HAL_GetTick() - start) < context->timeout_ms);

    return false;
}

void sx_at24c256_hal_make_io(sx_at24c256_hal_t *context, sx_at24c256_io_t *io)
{
    if (io == NULL) {
        return;
    }
    io->context = context;
    io->read = sx_at24c256_hal_read;
    io->write = sx_at24c256_hal_write;
}
