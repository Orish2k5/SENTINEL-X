#ifndef SX_TB6612_H
#define SX_TB6612_H

#include <stdbool.h>
#include <stdint.h>

typedef bool (*sx_tb6612_standby_fn)(void *context, bool enabled);
typedef bool (*sx_tb6612_inputs_fn)(void *context, bool ain1, bool ain2);
typedef bool (*sx_tb6612_pwm_fn)(void *context, uint16_t level_permille);

typedef struct {
    void *context;
    sx_tb6612_standby_fn set_standby;
    sx_tb6612_inputs_fn set_inputs;
    sx_tb6612_pwm_fn set_pwm_permille;
} sx_tb6612_io_t;

typedef struct {
    sx_tb6612_io_t io;
    uint16_t level_permille;
    bool initialized;
    bool faulted;
} sx_tb6612_t;

/* This MVP driver intentionally supports one fixed forward direction only. */
bool sx_tb6612_init(sx_tb6612_t *motor, const sx_tb6612_io_t *io);
bool sx_tb6612_force_off(sx_tb6612_t *motor);
bool sx_tb6612_set_forward_level(sx_tb6612_t *motor, uint16_t level_permille);
uint16_t sx_tb6612_get_level(const sx_tb6612_t *motor);
bool sx_tb6612_is_faulted(const sx_tb6612_t *motor);

#endif /* SX_TB6612_H */
