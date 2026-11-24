#!/usr/bin/env python3
"""Verify the public SENTINEL-X v1 HMAC and CRC test vectors. Standard library only."""
from __future__ import annotations

import hashlib
import hmac
import json
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
VECTOR_PATH = REPO_ROOT / "protocol" / "envelope_v1" / "test-vectors" / "envelope_v1_hmac.json"


def crc16_ccitt_false(data: bytes) -> int:
    """CRC-16/CCITT-FALSE: poly=0x1021, init=0xFFFF, refin=false, refout=false, xorout=0."""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if (crc & 0x8000) else (crc << 1) & 0xFFFF
    return crc


def main() -> int:
    vector = json.loads(VECTOR_PATH.read_text(encoding="utf-8"))
    key = bytes.fromhex(vector["key_hex_test_only"])
    message = bytes.fromhex(vector["authenticated_bytes_hex"])
    expected_tag = vector["expected_tag_hex"].lower()

    actual_tag = hmac.new(key, message, hashlib.sha256).hexdigest()
    if not hmac.compare_digest(actual_tag, expected_tag):
        raise SystemExit(f"FAIL HMAC: expected {expected_tag}, got {actual_tag}")

    # Negative test: changed authenticated bytes with the original tag must fail.
    mutated = bytearray(message)
    mutated[-1] ^= 0x01
    mutated_tag = hmac.new(key, bytes(mutated), hashlib.sha256).hexdigest()
    if hmac.compare_digest(mutated_tag, expected_tag):
        raise SystemExit("FAIL negative HMAC mutation test")

    # Standard CRC-16/CCITT-FALSE check value.
    crc_check = crc16_ccitt_false(b"123456789")
    if crc_check != 0x29B1:
        raise SystemExit(f"FAIL CRC: expected 0x29B1, got 0x{crc_check:04X}")

    full_envelope_len = len(message) + len(bytes.fromhex(expected_tag))
    if full_envelope_len != vector["expected_envelope_length_bytes"]:
        raise SystemExit(
            f"FAIL envelope length: expected {vector['expected_envelope_length_bytes']}, got {full_envelope_len}"
        )
    if len(message) != 49 + vector["expected_payload_length_bytes"]:
        raise SystemExit("FAIL header/payload length consistency")

    print("PASS SX-HMAC-001: HMAC-SHA-256 matches expected tag")
    print("PASS negative mutation: modified authenticated bytes do not match original tag")
    print("PASS CRC-16/CCITT-FALSE check: '123456789' -> 0x29B1")
    print(f"PASS envelope length: {full_envelope_len} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
