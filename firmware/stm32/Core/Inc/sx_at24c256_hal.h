#ifndef SX_AT24C256_HAL_H
#define SX_AT24C256_HAL_H

#include "stm32f4xx_hal.h"
#include "sx_at24c256.h"

typedef struct {
    I2C_HandleTypeDef *i2c;
    uint8_t address_7bit;
    uint32_t timeout_ms;
} sx_at24c256_hal_t;

bool sx_at24c256_hal_configure(sx_at24c256_hal_t *context,
                               I2C_HandleTypeDef *i2c,
                               uint8_t address_7bit,
                               uint32_t timeout_ms);
bool sx_at24c256_hal_probe(sx_at24c256_hal_t *context);
bool sx_at24c256_hal_read(void *context, uint16_t address,
                          uint8_t *data, uint16_t length);
bool sx_at24c256_hal_write(void *context, uint16_t address,
                           const uint8_t *data, uint16_t length);
void sx_at24c256_hal_make_io(sx_at24c256_hal_t *context, sx_at24c256_io_t *io);

#endif /* SX_AT24C256_HAL_H */
