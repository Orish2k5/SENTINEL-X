# EnvelopeV1

Normative definition: `docs/interface_contracts_v1.0.md`.

- Header: 49 bytes.
- Payload: 0–128 bytes.
- HMAC-SHA-256 tag: 32 bytes.
- Maximum envelope: 209 bytes.
- Multi-byte integers: big-endian.
- `device_id`: ASCII uppercase, max 16 bytes.
- `message_id`: 16-byte binary UUID.
- HMAC covers header plus payload; tag is appended.
- The test key is public test material only and must never be provisioned to a real device.
