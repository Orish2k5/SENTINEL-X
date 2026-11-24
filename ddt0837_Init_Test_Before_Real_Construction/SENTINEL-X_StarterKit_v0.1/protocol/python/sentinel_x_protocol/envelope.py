"""EnvelopeV1 codec and HMAC-SHA-256 helpers (Python reference implementation)."""
from __future__ import annotations
from dataclasses import dataclass
import hashlib
import hmac
import re
import struct
import uuid
from typing import Union
from .constants import HEADER_SIZE, MAGIC, MAX_ENVELOPE_SIZE, MAX_PAYLOAD, MSG_ACK, MSG_CMD, MSG_EVENT, MSG_POLICY_UPDATE, MSG_STATUS, PROTOCOL_VERSION, TAG_SIZE, VALID_MESSAGE_TYPES

_DEVICE_ID_RE = re.compile(rb"^[A-Z0-9_-]{1,16}$")
BytesLike = Union[bytes, bytearray, memoryview]

class ProtocolError(ValueError):
    """Raised when a wire object violates the v1 protocol structure."""

@dataclass(frozen=True)
class Envelope:
    protocol_version: int
    message_type: int
    device_id: str
    key_id: int
    sequence: int
    message_id: uuid.UUID
    operation_code: int
    payload: bytes
    tag: bytes

def _device_id_bytes(device_id: str) -> bytes:
    try:
        raw = device_id.encode("ascii")
    except UnicodeEncodeError as exc:
        raise ProtocolError("device_id must be ASCII") from exc
    if not _DEVICE_ID_RE.fullmatch(raw):
        raise ProtocolError("device_id must match [A-Z0-9_-], length 1..16")
    return raw.ljust(16, b"\x00")

def _decode_device_id(raw: bytes) -> str:
    if len(raw) != 16:
        raise ProtocolError("device_id field must be exactly 16 bytes")
    name = raw.rstrip(b"\x00")
    if not name or b"\x00" in name or not _DEVICE_ID_RE.fullmatch(name):
        raise ProtocolError("invalid or incorrectly padded device_id")
    return name.decode("ascii")

def build_envelope(*, key: bytes, device_id: str, key_id: int, sequence: int,
                   message_id: Union[uuid.UUID, bytes, str], message_type: int,
                   operation_code: int, payload: bytes = b"", protocol_version: int = PROTOCOL_VERSION) -> bytes:
    """Build a complete EnvelopeV1 and append its 32-byte HMAC tag."""
    if len(key) != 32:
        raise ProtocolError("baseline device HMAC key must be exactly 32 bytes")
    if not 0 <= key_id <= 0xFF or not 0 <= sequence <= 0xFFFFFFFFFFFFFFFF:
        raise ProtocolError("key_id/sequence outside wire range")
    if message_type not in VALID_MESSAGE_TYPES:
        raise ProtocolError("unsupported message_type")
    if protocol_version != PROTOCOL_VERSION:
        raise ProtocolError("only protocol version 1 is supported")
    if not 0 <= operation_code <= 0xFFFF:
        raise ProtocolError("operation_code must fit uint16")
    payload = bytes(payload)
    if len(payload) > MAX_PAYLOAD:
        raise ProtocolError("payload exceeds 128 bytes")
    if isinstance(message_id, uuid.UUID):
        message_id_bytes = message_id.bytes
    elif isinstance(message_id, str):
        try:
            message_id_bytes = uuid.UUID(message_id).bytes
        except ValueError as exc:
            raise ProtocolError("message_id must be a UUID") from exc
    else:
        message_id_bytes = bytes(message_id)
    if len(message_id_bytes) != 16:
        raise ProtocolError("message_id must be exactly 16 UUID bytes")
    header = b"".join((MAGIC, struct.pack("!BB", protocol_version, message_type),
                       _device_id_bytes(device_id), struct.pack("!BQ", key_id, sequence),
                       message_id_bytes, struct.pack("!HH", operation_code, len(payload))))
    if len(header) != HEADER_SIZE:
        raise AssertionError(f"internal header size error: {len(header)}")
    authenticated = header + payload
    return authenticated + hmac.new(key, authenticated, hashlib.sha256).digest()

def parse_envelope(data: BytesLike) -> Envelope:
    """Parse and validate EnvelopeV1 structure; does not authenticate its HMAC."""
    raw = bytes(data)
    if not HEADER_SIZE + TAG_SIZE <= len(raw) <= MAX_ENVELOPE_SIZE:
        raise ProtocolError("envelope length outside 81..209 bytes")
    if raw[:2] != MAGIC:
        raise ProtocolError("invalid envelope magic")
    version, message_type = raw[2], raw[3]
    if version != PROTOCOL_VERSION:
        raise ProtocolError("unsupported protocol version")
    if message_type not in VALID_MESSAGE_TYPES:
        raise ProtocolError("unsupported message_type")
    device_id = _decode_device_id(raw[4:20])
    key_id = raw[20]
    sequence = int.from_bytes(raw[21:29], "big")
    message_id = uuid.UUID(bytes=raw[29:45])
    operation_code = int.from_bytes(raw[45:47], "big")
    payload_len = int.from_bytes(raw[47:49], "big")
    if payload_len > MAX_PAYLOAD:
        raise ProtocolError("payload_len exceeds 128 bytes")
    expected_len = HEADER_SIZE + payload_len + TAG_SIZE
    if len(raw) != expected_len:
        raise ProtocolError(f"length mismatch: header declares {expected_len}, got {len(raw)}")
    return Envelope(version, message_type, device_id, key_id, sequence, message_id,
                    operation_code, raw[HEADER_SIZE:HEADER_SIZE + payload_len], raw[-TAG_SIZE:])

def verify_envelope_hmac(data: BytesLike, key: bytes) -> bool:
    """Return True only when the envelope structure and HMAC are valid."""
    if len(key) != 32:
        return False
    try:
        envelope = parse_envelope(data)
    except (ProtocolError, TypeError, ValueError):
        return False
    raw = bytes(data)
    expected = hmac.new(key, raw[:-TAG_SIZE], hashlib.sha256).digest()
    return hmac.compare_digest(expected, envelope.tag)

def message_type_name(message_type: int) -> str:
    return {MSG_CMD: "CMD", MSG_ACK: "ACK", MSG_EVENT: "EVENT", MSG_POLICY_UPDATE: "POLICY_UPDATE", MSG_STATUS: "STATUS"}.get(message_type, "UNKNOWN")
