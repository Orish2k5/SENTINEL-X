"""PolicyV1 payload codec/validator.

Baseline command-specific numeric ranges: SET_OUTPUT 0..1, SET_LEVEL 0..1000,
GET_STATUS exactly 0. This interpretation should be confirmed in the group contract.
"""
from __future__ import annotations
from dataclasses import dataclass
import struct
from .constants import OP_GET_STATUS, OP_SET_LEVEL, OP_SET_OUTPUT, POLICY_FLAG_FORCE_OFF_ON_LINK_LOSS
from .envelope import ProtocolError
KNOWN_COMMANDS = {OP_SET_OUTPUT, OP_SET_LEVEL, OP_GET_STATUS}

@dataclass(frozen=True)
class PolicyEntry:
    command_code: int
    allowed: bool
    min_value: int
    max_value: int

@dataclass(frozen=True)
class Policy:
    policy_version: int
    max_commands_per_second: int
    flags: int
    entries: tuple[PolicyEntry, ...]

def _validate_policy(policy: Policy, active_policy_version: int = 0) -> None:
    if not 0 <= policy.policy_version <= 0xFFFFFFFF or policy.policy_version <= active_policy_version:
        raise ProtocolError("policy_version must be uint32 and greater than active version")
    if not 1 <= policy.max_commands_per_second <= 20:
        raise ProtocolError("max_commands_per_second must be 1..20")
    if policy.flags & ~POLICY_FLAG_FORCE_OFF_ON_LINK_LOSS:
        raise ProtocolError("reserved PolicyV1 flags must be zero")
    if not 1 <= len(policy.entries) <= 8:
        raise ProtocolError("PolicyV1 entry_count must be 1..8")
    seen: set[int] = set()
    limits = {OP_SET_OUTPUT: 1, OP_SET_LEVEL: 1000, OP_GET_STATUS: 0}
    for entry in policy.entries:
        if entry.command_code not in KNOWN_COMMANDS or entry.command_code in seen:
            raise ProtocolError("unknown or duplicate PolicyV1 command_code")
        seen.add(entry.command_code)
        if not isinstance(entry.allowed, bool):
            raise ProtocolError("PolicyV1 allowed must be bool")
        if not 0 <= entry.min_value <= entry.max_value <= limits[entry.command_code]:
            raise ProtocolError("PolicyV1 min/max exceeds absolute command bounds")
        if entry.command_code == OP_GET_STATUS and (entry.min_value or entry.max_value):
            raise ProtocolError("GET_STATUS min/max must both be zero")

def encode_policy(policy: Policy, active_policy_version: int = 0) -> bytes:
    _validate_policy(policy, active_policy_version)
    result = bytearray(struct.pack("!I H B B", policy.policy_version, policy.max_commands_per_second, len(policy.entries), policy.flags))
    for entry in policy.entries:
        result.extend(struct.pack("!H B H H", entry.command_code, 1 if entry.allowed else 0, entry.min_value, entry.max_value))
    if len(result) > 64:
        raise ProtocolError("PolicyV1 payload exceeds 64 bytes")
    return bytes(result)

def decode_policy(payload: bytes, active_policy_version: int = 0) -> Policy:
    raw = bytes(payload)
    if len(raw) < 15 or len(raw) > 64:
        raise ProtocolError("PolicyV1 payload length must be 15..64")
    version, cps, count, flags = struct.unpack("!I H B B", raw[:8])
    if not 1 <= count <= 8 or len(raw) != 8 + 7 * count:
        raise ProtocolError("PolicyV1 length/count mismatch")
    entries = []
    offset = 8
    for _ in range(count):
        code, allowed, min_value, max_value = struct.unpack("!H B H H", raw[offset:offset + 7])
        if allowed not in (0, 1):
            raise ProtocolError("PolicyV1 allowed must be 0 or 1")
        entries.append(PolicyEntry(code, bool(allowed), min_value, max_value))
        offset += 7
    policy = Policy(version, cps, flags, tuple(entries))
    _validate_policy(policy, active_policy_version)
    return policy
