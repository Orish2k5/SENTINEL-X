# REST API Contract v1

Normative definition: `docs/interface_contracts_v1.0.md`.

- API prefix: `/api/v1`.
- Backend must enforce RBAC on every protected operation.
- `request_id`, `command_id`, and device `message_id` are distinct.
- HTTP 202 means queued/accepted, not completed.
- Reusing an idempotency key with a different body is a conflict.
- Never expose secrets, tokens, HMAC keys or internal exception traces.
