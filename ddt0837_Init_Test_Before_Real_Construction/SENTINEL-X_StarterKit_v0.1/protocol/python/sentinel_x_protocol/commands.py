"""Command payload encoders/validators from the SENTINEL-X v1 baseline."""
from __future__ import annotations
import struct
from .constants import OP_GET_STATUS, OP_SET_LEVEL, OP_SET_OUTPUT
from .envelope import ProtocolError

def encode_set_output(output_id: int, state: int) -> bytes:
    if output_id != 1 or state not in (0, 1):
        raise ProtocolError("baseline SET_OUTPUT requires output_id=1 and state 0/1")
    return bytes((output_id, state))

def encode_set_level(channel_id: int, level_permille: int) -> bytes:
    if channel_id != 1 or not 0 <= level_permille <= 1000:
        raise ProtocolError("baseline SET_LEVEL requires channel_id=1 and level 0..1000")
    return bytes((channel_id,)) + struct.pack("!H", level_permille)

def encode_get_status() -> bytes:
    return b""

def validate_command_payload(operation_code: int, payload: bytes) -> None:
    payload = bytes(payload)
    if operation_code == OP_SET_OUTPUT:
        if len(payload) != 2 or payload[0] != 1 or payload[1] not in (0, 1):
            raise ProtocolError("invalid SET_OUTPUT payload")
    elif operation_code == OP_SET_LEVEL:
        if len(payload) != 3 or payload[0] != 1 or not 0 <= int.from_bytes(payload[1:3], "big") <= 1000:
            raise ProtocolError("invalid SET_LEVEL payload")
    elif operation_code == OP_GET_STATUS:
        if payload:
            raise ProtocolError("GET_STATUS payload must be empty")
    else:
        raise ProtocolError("unsupported command operation_code")
