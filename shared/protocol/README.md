# SENTINEL-X shared protocol assets

This directory contains versioned, cross-language protocol artifacts.

## Source of truth
- `../../docs/03-interface-contracts/INTERFACE_CONTRACTS_v1.0.md` is the human-readable contract.
- `v1/` contains C-facing constants for the binary protocol.
- `test-vectors/` contains public interoperability vectors. The vector key is test-only and MUST NOT be used on a deployed device.
- `tools/verify_vectors.py` verifies the HMAC vector and CRC-16/CCITT-FALSE check value using Python's standard library.

## Rules
1. Do not change offsets, field sizes, enum values, byte order, or test vectors in only one implementation.
2. Any incompatible protocol change requires a new version and an ADR.
3. Every change to a contract must include updated cross-language tests and review by affected module owners.
4. Never place deployment secrets in this directory. Test vectors are public and intentionally use a public test key.
