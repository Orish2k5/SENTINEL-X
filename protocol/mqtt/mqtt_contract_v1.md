# MQTT Contract v1

Normative definition: `docs/interface_contracts_v1.0.md`.

- MQTT 3.1.1; QoS 1 for command delivery and required results.
- Commands are not retained.
- QoS 1 can redeliver; consumers must handle duplicates safely.
- Use least-privilege per-client ACL and TLS certificate validation on untrusted networks.
- ESP32 is a forwarding node; final device-side enforcement remains on STM32.
- HTTP 202 means accepted for asynchronous processing, not completed.
