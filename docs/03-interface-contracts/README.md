# Interface Contracts

- Canonical baseline: `INTERFACE_CONTRACTS_v1.0.md`
- C constants: `shared/protocol/v1/sentinel_x_protocol_v1.h`
- Public HMAC vector: `shared/protocol/test-vectors/envelope_v1_hmac.json`
- Host verifier: `shared/protocol/tools/verify_vectors.py`
- Example broker ACL: `configs/examples/mosquitto_acl.example`

## Validation status
The HMAC test vector and the CRC-16/CCITT-FALSE standard check value were independently verified with Python's standard library while preparing this package. This is a host-side verification only; it does not prove the C firmware implementation or hardware transport is correct.

## Before merge
1. Review the contract with all three module owners.
2. Run `python shared/protocol/tools/verify_vectors.py`.
3. Cross-check the C implementation against the same JSON vector.
4. Do not mark the contract gate complete until C and Python results match and the team accepts any open items.
