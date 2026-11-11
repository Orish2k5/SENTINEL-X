#!/usr/bin/env python3
"""Verify the repository's public HMAC test vector (never a device key)."""

from __future__ import annotations

import hashlib
import hmac
import json
from pathlib import Path


def main() -> int:
    vector_path = (
        Path(__file__).resolve().parents[3]
        / "protocol"
        / "envelope_v1"
        / "test-vectors"
        / "envelope_v1_hmac.json"
    )
    if not vector_path.is_file():
        raise FileNotFoundError(
            f"Cannot find shared protocol vector: {vector_path}. "
            "Run from the SENTINEL-X repository layout."
        )

    vector = json.loads(vector_path.read_text(encoding="utf-8"))
    key = bytes.fromhex(vector["key_hex_test_only"])
    message = bytes.fromhex(vector["authenticated_bytes_hex"])
    expected = bytes.fromhex(vector["expected_tag_hex"])

    actual = hmac.new(key, message, hashlib.sha256).digest()
    if not hmac.compare_digest(actual, expected):
        raise AssertionError("HMAC vector mismatch")

    mutated = bytearray(message)
    mutated[0] ^= 0x01
    mutated_tag = hmac.new(key, bytes(mutated), hashlib.sha256).digest()
    if hmac.compare_digest(mutated_tag, expected):
        raise AssertionError("Mutated authenticated bytes unexpectedly matched the original tag")

    expected_envelope_len = len(message) + len(expected)
    if expected_envelope_len != vector["expected_envelope_length_bytes"]:
        raise AssertionError("Envelope length in vector is inconsistent")

    print("HMAC vector: PASS (valid tag and one-bit mutation test)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
