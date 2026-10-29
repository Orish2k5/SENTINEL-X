# Shared protocol contracts

This directory is the source of truth for cross-component interfaces. Implementations in STM32, ESP32, Edge Backend and PC App must follow the approved contracts here.

- `envelope_v1/` — authenticated message envelope and field definitions.
- `uart/` — framing, CRC, timeouts and STM32↔ESP32 transport behavior.
- `mqtt/` — topics, QoS, retained-message rules and message lifecycle.
- `rest/` — HTTP endpoints, request/response/error schemas and authorization.
- `schemas/` — machine-readable schemas and shared examples.

Any change to a shared contract requires a PR, review by affected component owners, compatibility notes, and updates to tests. Do not place real secrets in protocol examples. Clearly label any not-yet-approved proposal as `PROPOSED`.
