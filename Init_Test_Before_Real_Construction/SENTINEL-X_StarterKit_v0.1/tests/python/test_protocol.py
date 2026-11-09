from __future__ import annotations
import json
from pathlib import Path
import sys
import unittest
import uuid
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "protocol" / "python"))
from sentinel_x_protocol import (LinkStatus, Policy, PolicyEntry, ProtocolError, build_envelope, build_uart_frame,  
    crc16_ccitt_false, decode_link_status, decode_policy, encode_get_status, encode_link_status, encode_policy,
    encode_set_level, encode_set_output, parse_envelope, parse_uart_frame, validate_command_payload,
    verify_envelope_hmac)
from sentinel_x_protocol.constants import MSG_CMD, OP_GET_STATUS, OP_SET_LEVEL, OP_SET_OUTPUT, UART_FRAME_AUTH_ENVELOPE, UART_FRAME_LINK_STATUS
VECTOR = json.loads((ROOT / "protocol" / "vectors" / "envelope_v1_hmac.json").read_text())
TEST_KEY = bytes.fromhex(VECTOR["key_hex_test_only"])

class EnvelopeTests(unittest.TestCase):
    def test_public_vector(self):
        full = bytes.fromhex(VECTOR["authenticated_bytes_hex"] + VECTOR["expected_tag_hex"])
        self.assertEqual(len(full), 83)
        self.assertTrue(verify_envelope_hmac(full, TEST_KEY))
        parsed = parse_envelope(full)
        self.assertEqual(parsed.device_id, "SX-001")
        self.assertEqual(parsed.sequence, 1)
        self.assertEqual(parsed.message_id, uuid.UUID("123e4567-e89b-12d3-a456-426614174000"))
        self.assertEqual((parsed.operation_code, parsed.payload), (OP_SET_OUTPUT, b"\x01\x01"))
    def test_build_matches_vector(self):
        built = build_envelope(key=TEST_KEY, device_id="SX-001", key_id=1, sequence=1,
            message_id="123e4567-e89b-12d3-a456-426614174000", message_type=MSG_CMD,
            operation_code=OP_SET_OUTPUT, payload=b"\x01\x01")
        self.assertEqual(built.hex(), VECTOR["authenticated_bytes_hex"] + VECTOR["expected_tag_hex"])
    def test_mutation_and_wrong_length_rejected(self):
        full = bytearray.fromhex(VECTOR["authenticated_bytes_hex"] + VECTOR["expected_tag_hex"])
        full[50] ^= 1
        self.assertFalse(verify_envelope_hmac(full, TEST_KEY))
        bad_len = bytearray.fromhex(VECTOR["authenticated_bytes_hex"] + VECTOR["expected_tag_hex"])
        bad_len[48] = 3
        with self.assertRaises(ProtocolError): parse_envelope(bad_len)

class UartTests(unittest.TestCase):
    def test_crc_check(self): self.assertEqual(crc16_ccitt_false(b"123456789"), 0x29B1)
    def test_envelope_frame_round_trip(self):
        env = bytes.fromhex(VECTOR["authenticated_bytes_hex"] + VECTOR["expected_tag_hex"])
        frame = build_uart_frame(UART_FRAME_AUTH_ENVELOPE, env)
        self.assertEqual(parse_uart_frame(frame).payload, env)
    def test_crc_mutation_rejected(self):
        env = bytes.fromhex(VECTOR["authenticated_bytes_hex"] + VECTOR["expected_tag_hex"])
        frame = bytearray(build_uart_frame(UART_FRAME_AUTH_ENVELOPE, env)); frame[10] ^= 1
        with self.assertRaises(ProtocolError): parse_uart_frame(frame)
    def test_link_status_round_trip(self):
        status = LinkStatus(1, 0, 123456)
        payload = encode_link_status(status)
        self.assertEqual(decode_link_status(parse_uart_frame(build_uart_frame(UART_FRAME_LINK_STATUS, payload)).payload), status)

class CommandPolicyTests(unittest.TestCase):
    def test_commands(self):
        validate_command_payload(OP_SET_OUTPUT, encode_set_output(1, 1))
        validate_command_payload(OP_SET_LEVEL, encode_set_level(1, 750))
        validate_command_payload(OP_GET_STATUS, encode_get_status())
        with self.assertRaises(ProtocolError): encode_set_level(1, 1001)
    def test_policy_round_trip(self):
        p = Policy(1, 5, 1, (PolicyEntry(OP_SET_OUTPUT, True, 0, 1),
            PolicyEntry(OP_SET_LEVEL, True, 0, 1000), PolicyEntry(OP_GET_STATUS, True, 0, 0)))
        raw = encode_policy(p)
        self.assertEqual(len(raw), 29)
        self.assertEqual(decode_policy(raw), p)
    def test_policy_duplicate_or_rollback_rejected(self):
        p = Policy(1, 5, 0, (PolicyEntry(OP_SET_OUTPUT, True, 0, 1),))
        with self.assertRaises(ProtocolError): encode_policy(p, active_policy_version=1)
        dup = Policy(2, 5, 0, (PolicyEntry(OP_SET_OUTPUT, True, 0, 1), PolicyEntry(OP_SET_OUTPUT, False, 0, 1)))
        with self.assertRaises(ProtocolError): encode_policy(dup)

if __name__ == "__main__": unittest.main()
