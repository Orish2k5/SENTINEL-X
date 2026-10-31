# SENTINEL-X Protocol Contracts

Master normative contract: `../docs/interface_contracts_v1.0.md`.

- `envelope_v1/envelope_v1.md`: envelope layout.
- `envelope_v1/test-vectors/envelope_v1_hmac.json`: public test vector; key is test-only.
- `uart/uart_framing_v1.md`: UART/CRC rules.
- `mqtt/mqtt_contract_v1.md`: topics/QoS/ACL principles.
- `rest/rest_api_v1.md`: REST/RBAC principles.
- `schemas/error_codes_v1.json`: error code registry.
- `schemas/command_lifecycle_v1.md`: command/policy lifecycle.
- `c/sentinel_x_protocol_v1.h`: C constants.
- `../tools/scripts/verify_vectors.py`: host-side verifier.

If any detail conflicts, stop implementation and resolve the master contract through review and a recorded decision.
