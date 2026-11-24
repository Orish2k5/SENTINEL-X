#ifndef SX_REPLAY_GUARD_H
#define SX_REPLAY_GUARD_H

#include <stdbool.h>
#include <stdint.h>

typedef bool (*sx_sequence_load_fn)(void *context, uint64_t *high_water_mark);
typedef bool (*sx_sequence_commit_fn)(void *context, uint64_t new_high_water_mark);

typedef struct {
    void *context;
    sx_sequence_load_fn load;
    sx_sequence_commit_fn commit;
} sx_sequence_store_t;

typedef enum {
    SX_REPLAY_ACCEPTED = 0,
    SX_REPLAY_REJECTED,
    SX_REPLAY_STORAGE_ERROR
} sx_replay_result_t;

typedef struct {
    sx_sequence_store_t store;
    uint64_t high_water_mark;
    bool ready;
} sx_replay_guard_t;

bool sx_replay_guard_init(sx_replay_guard_t *guard, const sx_sequence_store_t *store);
sx_replay_result_t sx_replay_guard_consume(sx_replay_guard_t *guard, uint64_t sequence);
uint64_t sx_replay_guard_high_water(const sx_replay_guard_t *guard);

#endif /* SX_REPLAY_GUARD_H */
