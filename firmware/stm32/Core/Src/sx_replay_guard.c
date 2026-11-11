#include "sx_replay_guard.h"

#include <string.h>

bool sx_replay_guard_init(sx_replay_guard_t *guard, const sx_sequence_store_t *store)
{
    uint64_t high_water_mark = 0u;

    if ((guard == NULL) || (store == NULL) || (store->load == NULL) || (store->commit == NULL)) {
        return false;
    }

    (void)memset(guard, 0, sizeof(*guard));
    guard->store = *store;
    if (!guard->store.load(guard->store.context, &high_water_mark)) {
        guard->ready = false;
        return false;
    }

    guard->high_water_mark = high_water_mark;
    guard->ready = true;
    return true;
}

sx_replay_result_t sx_replay_guard_consume(sx_replay_guard_t *guard, uint64_t sequence)
{
    if ((guard == NULL) || !guard->ready || (guard->store.commit == NULL)) {
        return SX_REPLAY_STORAGE_ERROR;
    }
    if (sequence <= guard->high_water_mark) {
        return SX_REPLAY_REJECTED;
    }

    /* Commit before changing RAM state and, critically, before actuator execution. */
    if (!guard->store.commit(guard->store.context, sequence)) {
        guard->ready = false;
        return SX_REPLAY_STORAGE_ERROR;
    }

    guard->high_water_mark = sequence;
    return SX_REPLAY_ACCEPTED;
}

uint64_t sx_replay_guard_high_water(const sx_replay_guard_t *guard)
{
    if (guard == NULL) {
        return 0u;
    }
    return guard->high_water_mark;
}
