# UART Framing v1

Normative definition: `docs/interface_contracts_v1.0.md`.

- UART: 115200 baud, 8N1.
- SOF: `A5 5A`.
- CRC-16/CCITT-FALSE: poly `0x1021`, init `0xFFFF`, no reflection, xorout `0x0000`.
- Check string `123456789` must produce `0x29B1`.
- CRC detects accidental transmission errors; it is not authentication.
- Do not implement until sender and receiver agree on length convention, CRC coverage and field offsets.
