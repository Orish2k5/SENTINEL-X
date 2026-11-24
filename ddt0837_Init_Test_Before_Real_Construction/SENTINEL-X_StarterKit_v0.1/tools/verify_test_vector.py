#!/usr/bin/env python3
"""Verify public test-only HMAC vector and EnvelopeV1 codec."""
from __future__ import annotations
import json
from pathlib import Path
import sys
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "protocol" / "python"))
from sentinel_x_protocol import parse_envelope, verify_envelope_hmac

def main() -> int:
    v = json.loads((ROOT / "protocol" / "vectors" / "envelope_v1_hmac.json").read_text(encoding="utf-8"))
    key = bytes.fromhex(v["key_hex_test_only"])
    full = bytes.fromhex(v["authenticated_bytes_hex"] + v["expected_tag_hex"])
    if not verify_envelope_hmac(full, key): raise SystemExit("FAIL: HMAC vector mismatch")
    obj = parse_envelope(full)
    if len(full) != v["expected_envelope_length_bytes"] or len(obj.payload) != v["expected_payload_length_bytes"]:
        raise SystemExit("FAIL: envelope/payload length mismatch")
    mutated = bytearray(full); mutated[50] ^= 1
    if verify_envelope_hmac(mutated, key): raise SystemExit("FAIL: mutation not detected")
    print("PASS SX-HMAC-001: vector and EnvelopeV1 parsing")
    print("PASS negative HMAC mutation test")
    print("WARNING: test vector key is public; NEVER use it as a deployment key")
    return 0
if __name__ == "__main__": raise SystemExit(main())
