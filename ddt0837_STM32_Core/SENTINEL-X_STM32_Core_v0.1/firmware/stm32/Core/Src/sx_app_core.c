#include "sx_app_core.h"

#include <string.h>

static bool sx_policy_struct_valid(const sx_policy_v1_t *policy);

static sx_app_result_t sx_result(sx_app_core_t *app, sx_app_result_code_t code,
                                 uint8_t execution_state)
{
    sx_app_result_t result;
    result.result_code = code;
    result.execution_state = execution_state;
    result.output_state = app != NULL ? app->output_state : 255u;
    result.output_level_permille = app != NULL ? app->output_level_permille : 65535u;
    if ((app != NULL) && (code == SX_APP_RESULT_SUCCESS)) {
        app->accepted_count++;
    } else if (app != NULL) {
        app->rejected_count++;
    }
    return result;
}

static void sx_force_output_off(sx_app_core_t *app)
{
    if (app == NULL) {
        return;
    }
    if ((app->output.force_off != NULL)) {
        app->output.force_off(app->output.context);
    } else if (app->output.set_level_permille != NULL) {
        app->output.set_level_permille(app->output.context, 0u);
    }
    app->output_state = 0u;
    app->output_level_permille = 0u;
}

void sx_app_force_off(sx_app_core_t *app)
{
    sx_force_output_off(app);
}

bool sx_app_core_init(sx_app_core_t *app,
                      const uint8_t device_id[SX_SIZE_DEVICE_ID],
                      const sx_sequence_store_t *sequence_store,
                      const sx_key_provider_t *key_provider,
                      const sx_crypto_provider_t *crypto_provider,
                      const sx_policy_store_t *policy_store,
                      sx_rate_limit_allow_fn rate_limit_allow,
                      void *rate_limit_context,
                      const sx_output_interface_t *output)
{
    bool sequence_ok;

    if ((app == NULL) || (device_id == NULL) || (sequence_store == NULL) ||
        (key_provider == NULL) || (crypto_provider == NULL) || (output == NULL)) {
        return false;
    }

    (void)memset(app, 0, sizeof(*app));
    (void)memcpy(app->device_id, device_id, SX_SIZE_DEVICE_ID);
    app->key_provider = *key_provider;
    app->crypto_provider = *crypto_provider;
    app->sequence_store = *sequence_store;
    app->output = *output;
    app->rate_limit_allow = rate_limit_allow;
    app->rate_limit_context = rate_limit_context;
    app->last_nonzero_level_permille = 1000u;
    app->output_state = 0u;
    app->output_level_permille = 0u;

    sequence_ok = sx_replay_guard_init(&app->replay_guard, sequence_store);
    app->storage_ready = sequence_ok;
    if (!sequence_ok) {
        sx_force_output_off(app);
        return false;
    }

    if (policy_store != NULL) {
        app->policy_store = *policy_store;
        if ((policy_store->load != NULL) && policy_store->load(policy_store->context, &app->active_policy) &&
            sx_policy_struct_valid(&app->active_policy)) {
            /* The backend must also validate the FRAM record CRC/generation/commit marker. */
            app->policy_valid = true;
        }
    }

    sx_force_output_off(app);
    return true;
}

static bool sx_policy_struct_valid(const sx_policy_v1_t *policy)
{
    uint8_t i;
    uint8_t j;

    if ((policy == NULL) || (policy->version == 0u) ||
        (policy->max_commands_per_second < 1u) ||
        (policy->max_commands_per_second > 20u) ||
        (policy->entry_count < 1u) || (policy->entry_count > 8u) ||
        ((policy->flags & 0xFEu) != 0u)) {
        return false;
    }

    for (i = 0u; i < policy->entry_count; ++i) {
        const sx_policy_entry_t *entry = &policy->entries[i];
        bool bounds_valid = false;

        if (entry->allowed > 1u) {
            return false;
        }
        if (entry->min_value > entry->max_value) {
            return false;
        }
        switch (entry->command_code) {
        case SX_OP_SET_OUTPUT:
            bounds_valid = entry->max_value <= 1u;
            break;
        case SX_OP_SET_LEVEL:
            bounds_valid = entry->max_value <= 1000u;
            break;
        case SX_OP_GET_STATUS:
            bounds_valid = (entry->min_value == 0u) && (entry->max_value == 0u);
            break;
        default:
            bounds_valid = false;
            break;
        }
        if (!bounds_valid) {
            return false;
        }
        for (j = 0u; j < i; ++j) {
            if (policy->entries[j].command_code == entry->command_code) {
                return false;
            }
        }
    }
    return true;
}

static bool sx_policy_allows_command(const sx_policy_v1_t *policy,
                                     const sx_command_t *command)
{
    uint8_t i;
    uint16_t value;

    if ((policy == NULL) || (command == NULL)) {
        return false;
    }

    value = command->value;
    for (i = 0u; i < policy->entry_count; ++i) {
        const sx_policy_entry_t *entry = &policy->entries[i];
        if (entry->command_code == command->operation_code) {
            return (entry->allowed == 1u) &&
                   (value >= entry->min_value) && (value <= entry->max_value);
        }
    }
    return false;
}

static bool sx_apply_command(sx_app_core_t *app, const sx_command_t *command)
{
    uint16_t level;

    if ((app == NULL) || (command == NULL)) {
        return false;
    }

    switch (command->operation_code) {
    case SX_OP_SET_OUTPUT:
        if (command->value == 0u) {
            sx_force_output_off(app);
            return true;
        }
        level = app->last_nonzero_level_permille;
        if (level == 0u) {
            level = 1000u;
        }
        if (app->output.set_level_permille == NULL) {
            return false;
        }
        app->output.set_level_permille(app->output.context, level);
        app->output_state = 1u;
        app->output_level_permille = level;
        return true;

    case SX_OP_SET_LEVEL:
        if (command->value == 0u) {
            sx_force_output_off(app);
            return true;
        }
        if (app->output.set_level_permille == NULL) {
            return false;
        }
        app->output.set_level_permille(app->output.context, command->value);
        app->last_nonzero_level_permille = command->value;
        app->output_state = 1u;
        app->output_level_permille = command->value;
        return true;

    case SX_OP_GET_STATUS:
        return true;

    default:
        return false;
    }
}

sx_app_result_t sx_app_process_envelope(sx_app_core_t *app,
                                        const uint8_t *envelope, size_t envelope_len,
                                        uint32_t now_ms)
{
    sx_envelope_view_t view;
    sx_envelope_error_t parse_error;
    sx_command_t command;
    sx_policy_v1_t new_policy;
    uint8_t key[SX_DEVICE_KEY_SIZE];
    sx_replay_result_t replay_result;

    if ((app == NULL) || (envelope == NULL)) {
        return sx_result(app, SX_APP_RESULT_FRAME_INVALID, 0u);
    }
    if (!app->storage_ready || !app->replay_guard.ready) {
        sx_force_output_off(app);
        return sx_result(app, SX_APP_RESULT_STORAGE_UNAVAILABLE, 0u);
    }
    if (!sx_envelope_parse(envelope, envelope_len, app->device_id, &view, &parse_error)) {
        if (parse_error == SX_ENVELOPE_ERR_DEVICE_ID) {
            return sx_result(app, SX_APP_RESULT_DEVICE_MISMATCH, 0u);
        }
        if (parse_error == SX_ENVELOPE_ERR_KEY_ID) {
            return sx_result(app, SX_APP_RESULT_KEY_UNKNOWN, 0u);
        }
        if ((parse_error == SX_ENVELOPE_ERR_LENGTH) || (parse_error == SX_ENVELOPE_ERR_PAYLOAD_LENGTH)) {
            return sx_result(app, SX_APP_RESULT_LENGTH_INVALID, 0u);
        }
        return sx_result(app, SX_APP_RESULT_FRAME_INVALID, 0u);
    }

    if ((app->key_provider.get_key == NULL) ||
        !app->key_provider.get_key(app->key_provider.context, view.key_id, key)) {
        volatile uint8_t *wipe = key;
        size_t i;
        for (i = 0u; i < sizeof(key); ++i) {
            wipe[i] = 0u;
        }
        return sx_result(app, SX_APP_RESULT_KEY_UNKNOWN, 0u);
    }
    if ((app->crypto_provider.verify == NULL) ||
        (app->crypto_provider.verify(app->crypto_provider.context, key,
                                     view.authenticated_bytes, view.authenticated_length,
                                     view.tag) != 1)) {
        volatile uint8_t *wipe = key;
        size_t i;
        for (i = 0u; i < sizeof(key); ++i) {
            wipe[i] = 0u;
        }
        return sx_result(app, SX_APP_RESULT_HMAC_INVALID, 0u);
    }
    {
        volatile uint8_t *wipe = key;
        size_t i;
        for (i = 0u; i < sizeof(key); ++i) {
            wipe[i] = 0u;
        }
    }

    if (view.message_type == SX_MSG_CMD) {
        if (!sx_command_decode(&view, &command, &parse_error)) {
            return sx_result(app,
                (view.operation_code == SX_OP_SET_OUTPUT) ||
                (view.operation_code == SX_OP_SET_LEVEL) ||
                (view.operation_code == SX_OP_GET_STATUS)
                    ? SX_APP_RESULT_PAYLOAD_INVALID : SX_APP_RESULT_UNKNOWN_COMMAND,
                0u);
        }
    } else if (view.message_type == SX_MSG_POLICY_UPDATE) {
        if ((view.operation_code != SX_OP_POLICY_UPDATE) ||
            !sx_policy_v1_decode(view.payload, view.payload_len,
                                 app->policy_valid ? app->active_policy.version : 0u,
                                 &new_policy, &parse_error)) {
            return sx_result(app, SX_APP_RESULT_PARAMETER_INVALID, 0u);
        }
    } else {
        return sx_result(app, SX_APP_RESULT_FRAME_INVALID, 0u);
    }

    /* A schema-valid authenticated message consumes its sequence even if policy denies it. */
    replay_result = sx_replay_guard_consume(&app->replay_guard, view.sequence);
    if (replay_result == SX_REPLAY_REJECTED) {
        return sx_result(app, SX_APP_RESULT_REPLAY, 0u);
    }
    if (replay_result != SX_REPLAY_ACCEPTED) {
        app->storage_ready = false;
        sx_force_output_off(app);
        return sx_result(app, SX_APP_RESULT_PERSISTENCE_FAILED, 0u);
    }

    if (view.message_type == SX_MSG_POLICY_UPDATE) {
        if ((app->policy_store.commit == NULL) ||
            !app->policy_store.commit(app->policy_store.context, &new_policy)) {
            app->policy_valid = false;
            sx_force_output_off(app);
            return sx_result(app, SX_APP_RESULT_PERSISTENCE_FAILED, 0u);
        }
        app->active_policy = new_policy;
        app->policy_valid = true;
        return sx_result(app, SX_APP_RESULT_SUCCESS, 1u);
    }

    if ((command.operation_code != SX_OP_GET_STATUS) &&
        (!app->link_seen || !app->link_online ||
         ((uint32_t)(now_ms - app->last_link_status_ms) > SX_LINK_TIMEOUT_MS))) {
        sx_force_output_off(app);
        return sx_result(app, SX_APP_RESULT_LINK_OFFLINE, 0u);
    }

    if (!app->policy_valid) {
        /* Contract allows authenticated GET_STATUS while policy is absent; actuation stays OFF. */
        if (command.operation_code == SX_OP_GET_STATUS) {
            return sx_result(app, SX_APP_RESULT_SUCCESS, 1u);
        }
        return sx_result(app, SX_APP_RESULT_SAFE_MODE, 0u);
    }

    if (!sx_policy_allows_command(&app->active_policy, &command)) {
        return sx_result(app, SX_APP_RESULT_COMMAND_DENIED, 0u);
    }

    if (command.operation_code != SX_OP_GET_STATUS) {
        if ((app->rate_limit_allow == NULL) ||
            !app->rate_limit_allow(app->rate_limit_context, now_ms,
                                   app->active_policy.max_commands_per_second)) {
            return sx_result(app, SX_APP_RESULT_RATE_LIMITED, 0u);
        }
    }

    if (!sx_apply_command(app, &command)) {
        sx_force_output_off(app);
        return sx_result(app, SX_APP_RESULT_PARAMETER_INVALID, 0u);
    }
    return sx_result(app, SX_APP_RESULT_SUCCESS, 1u);
}

void sx_app_process_link_status(sx_app_core_t *app, const uint8_t payload[7],
                                uint32_t now_ms)
{
    if ((app == NULL) || (payload == NULL)) {
        return;
    }

    if ((payload[0] > 1u) || (payload[1] > 1u) || (payload[2] != 0u)) {
        app->link_online = false;
        sx_force_output_off(app);
        return;
    }

    app->link_seen = true;
    app->link_online = (payload[0] == 1u) && (payload[1] == 1u);
    app->last_link_status_ms = now_ms;
    if (!app->link_online) {
        sx_force_output_off(app);
    }
}

void sx_app_tick(sx_app_core_t *app, uint32_t now_ms)
{
    if (app == NULL) {
        return;
    }
    if (!app->link_seen || ((uint32_t)(now_ms - app->last_link_status_ms) > SX_LINK_TIMEOUT_MS) ||
        !app->link_online) {
        sx_force_output_off(app);
    }
    if (!app->storage_ready || !app->replay_guard.ready) {
        sx_force_output_off(app);
    }
}
