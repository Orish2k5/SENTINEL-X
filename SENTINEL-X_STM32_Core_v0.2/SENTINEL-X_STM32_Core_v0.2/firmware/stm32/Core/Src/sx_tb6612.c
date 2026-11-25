#include "sx_tb6612.h"

#include <stddef.h>
#include <string.h>

static bool sx_tb6612_io_valid(const sx_tb6612_io_t *io)
{
    return (io != NULL) && (io->set_standby != NULL) &&
           (io->set_inputs != NULL) && (io->set_pwm_permille != NULL);
}

static bool sx_tb6612_emergency_stop(sx_tb6612_t *motor)
{
    bool ok = true;

    if ((motor == NULL) || !sx_tb6612_io_valid(&motor->io)) return false;

    /* Disable the H-bridge first; then remove PWM and direction inputs. */
    if (!motor->io.set_standby(motor->io.context, false)) ok = false;
    if (!motor->io.set_pwm_permille(motor->io.context, 0u)) ok = false;
    if (!motor->io.set_inputs(motor->io.context, false, false)) ok = false;

    motor->level_permille = 0u;
    motor->faulted = !ok;
    return ok;
}

bool sx_tb6612_init(sx_tb6612_t *motor, const sx_tb6612_io_t *io)
{
    if ((motor == NULL) || !sx_tb6612_io_valid(io)) return false;

    (void)memset(motor, 0, sizeof(*motor));
    motor->io = *io;
    if (!sx_tb6612_emergency_stop(motor)) {
        motor->initialized = false;
        motor->faulted = true;
        return false;
    }
    motor->initialized = true;
    motor->faulted = false;
    return true;
}

bool sx_tb6612_force_off(sx_tb6612_t *motor)
{
    if ((motor == NULL) || !motor->initialized) return false;
    return sx_tb6612_emergency_stop(motor);
}

bool sx_tb6612_set_forward_level(sx_tb6612_t *motor, uint16_t level_permille)
{
    if ((motor == NULL) || !motor->initialized || (level_permille > 1000u)) return false;
    if (level_permille == 0u) return sx_tb6612_force_off(motor);

    /* Keep STBY low while updating direction and duty; enable only last. */
    if (!motor->io.set_standby(motor->io.context, false) ||
        !motor->io.set_pwm_permille(motor->io.context, 0u) ||
        !motor->io.set_inputs(motor->io.context, true, false) ||
        !motor->io.set_pwm_permille(motor->io.context, level_permille) ||
        !motor->io.set_standby(motor->io.context, true)) {
        (void)sx_tb6612_emergency_stop(motor);
        motor->faulted = true;
        return false;
    }

    motor->level_permille = level_permille;
    motor->faulted = false;
    return true;
}

uint16_t sx_tb6612_get_level(const sx_tb6612_t *motor)
{
    return (motor != NULL) ? motor->level_permille : 0u;
}

bool sx_tb6612_is_faulted(const sx_tb6612_t *motor)
{
    return (motor == NULL) || motor->faulted;
}
