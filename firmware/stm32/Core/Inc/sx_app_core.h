#ifndef SX_APP_CORE_H
#define SX_APP_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sx_envelope.h"
#include "sx_replay_guard.h"

#define SX_DEVICE_KEY_SIZE 32u
#define SX_LINK_TIMEOUT_MS 3000u

typedef bool (*sx_key_get_fn)(void *context, uint8_t key_id,
                              uint8_t key_out[SX_DEVICE_KEY_SIZE]);
typedef int (*sx_hmac_verify_fn)(void *context, const uint8_t key[SX_DEVICE_KEY_SIZE],
                                 const uint8_t *data, size_t data_len,
                                 const uint8_t tag[32]);
typedef bool (*sx_policy_load_fn)(void *context, sx_policy_v1_t *policy);
typedef bool (*sx_policy_commit_fn)(void *context, const sx_policy_v1_t *policy);
typedef bool (*sx_rate_limit_allow_fn)(void *context, uint32_t now_ms,
                                       uint16_t max_commands_per_second);

typedef struct {
    void *context;
    void (*force_off)(void *context);
    void (*set_level_permille)(void *context, uint16_t level_permille);
} sx_output_interface_t;

typedef struct {
    void *context;
    sx_policy_load_fn load;
    sx_policy_commit_fn commit;
} sx_policy_store_t;

typedef struct {
    void *context;
    sx_key_get_fn get_key;
} sx_key_provider_t;

typedef struct {
    void *context;
    sx_hmac_verify_fn verify;
} sx_crypto_provider_t;

typedef enum {
    SX_APP_RESULT_SUCCESS = SX_RESULT_SUCCESS,
    SX_APP_RESULT_FRAME_INVALID = SX_ERR_FRAME_INVALID,
    SX_APP_RESULT_LENGTH_INVALID = SX_ERR_LENGTH_INVALID,
    SX_APP_RESULT_DEVICE_MISMATCH = SX_ERR_DEVICE_ID_MISMATCH,
    SX_APP_RESULT_HMAC_INVALID = SX_ERR_HMAC_INVALID,
    SX_APP_RESULT_REPLAY = SX_ERR_REPLAY_OR_STALE_SEQUENCE,
    SX_APP_RESULT_RATE_LIMITED = SX_ERR_RATE_LIMITED,
    SX_APP_RESULT_COMMAND_DENIED = SX_ERR_COMMAND_NOT_ALLOWED,
    SX_APP_RESULT_PARAMETER_INVALID = SX_ERR_PARAMETER_OUT_OF_RANGE,
    SX_APP_RESULT_PERSISTENCE_FAILED = SX_ERR_PERSISTENCE_FAILED,
    SX_APP_RESULT_SAFE_MODE = SX_ERR_SAFE_MODE_ACTIVE,
    SX_APP_RESULT_UNKNOWN_COMMAND = SX_ERR_UNKNOWN_COMMAND,
    SX_APP_RESULT_PAYLOAD_INVALID = SX_ERR_PAYLOAD_INVALID,
    SX_APP_RESULT_KEY_UNKNOWN = SX_ERR_KEY_ID_UNKNOWN,
    SX_APP_RESULT_LINK_OFFLINE = SX_ERR_LINK_OFFLINE,
    SX_APP_RESULT_STORAGE_UNAVAILABLE = SX_ERR_STORAGE_UNAVAILABLE,
    SX_APP_RESULT_POLICY_PENDING = SX_ERR_POLICY_PENDING
} sx_app_result_code_t;

typedef struct {
    sx_app_result_code_t result_code;
    uint8_t execution_state; /* 0 rejected, 1 completed, 2 indeterminate */
    uint8_t output_state;    /* 0 off, 1 on, 255 unknown */
    uint16_t output_level_permille;
} sx_app_result_t;

typedef struct {
    uint8_t device_id[SX_SIZE_DEVICE_ID];
    sx_replay_guard_t replay_guard;
    sx_sequence_store_t sequence_store;
    sx_key_provider_t key_provider;
    sx_crypto_provider_t crypto_provider;
    sx_policy_store_t policy_store;
    sx_rate_limit_allow_fn rate_limit_allow;
    void *rate_limit_context;
    sx_output_interface_t output;
    sx_policy_v1_t active_policy;
    bool policy_valid;
    bool storage_ready;
    bool link_seen;
    bool link_online;
    uint32_t last_link_status_ms;
    uint8_t output_state;
    uint16_t output_level_permille;
    uint16_t last_nonzero_level_permille;
    uint32_t accepted_count;
    uint32_t rejected_count;
} sx_app_core_t;

bool sx_app_core_init(sx_app_core_t *app,
                      const uint8_t device_id[SX_SIZE_DEVICE_ID],
                      const sx_sequence_store_t *sequence_store,
                      const sx_key_provider_t *key_provider,
                      const sx_crypto_provider_t *crypto_provider,
                      const sx_policy_store_t *policy_store,
                      sx_rate_limit_allow_fn rate_limit_allow,
                      void *rate_limit_context,
                      const sx_output_interface_t *output);

sx_app_result_t sx_app_process_envelope(sx_app_core_t *app,
                                        const uint8_t *envelope, size_t envelope_len,
                                        uint32_t now_ms);
void sx_app_process_link_status(sx_app_core_t *app, const uint8_t payload[7],
                                uint32_t now_ms);
void sx_app_tick(sx_app_core_t *app, uint32_t now_ms);
void sx_app_force_off(sx_app_core_t *app);

#endif /* SX_APP_CORE_H */
