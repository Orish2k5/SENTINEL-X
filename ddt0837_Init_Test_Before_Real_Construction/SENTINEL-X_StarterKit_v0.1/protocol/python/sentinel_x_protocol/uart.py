"""UART outer framing v1: SOF, frame type, length, payload and CRC16."""
from __future__ import annotations
from dataclasses import dataclass
import struct
from .constants import CRC16_INIT, CRC16_POLY, UART_FRAME_AUTH_ENVELOPE, UART_FRAME_LINK_STATUS, UART_MAX_FRAME_SIZE, UART_SOF
from .envelope import ProtocolError

@dataclass(frozen=True)
class UartFrame:
    frame_type: int
    payload: bytes

@dataclass(frozen=True)
class LinkStatus:
    wifi_state: int
    mqtt_state: int
    esp32_uptime_ms: int

def crc16_ccitt_false(data: bytes) -> int:
    crc = CRC16_INIT
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = (((crc << 1) ^ CRC16_POLY) if crc & 0x8000 else (crc << 1)) & 0xFFFF
    return crc

def _validate_frame_payload(frame_type: int, payload: bytes) -> None:
    if frame_type == UART_FRAME_AUTH_ENVELOPE:
        if not 81 <= len(payload) <= 209:
            raise ProtocolError("AUTH_ENVELOPE payload length must be 81..209")
    elif frame_type == UART_FRAME_LINK_STATUS:
        if len(payload) != 7:
            raise ProtocolError("LINK_STATUS payload length must be exactly 7")
        if payload[0] not in (0, 1) or payload[1] not in (0, 1) or payload[2] != 0:
            raise ProtocolError("invalid LINK_STATUS state/reserved bytes")
    else:
        raise ProtocolError("unsupported UART frame_type")

def build_uart_frame(frame_type: int, payload: bytes) -> bytes:
    payload = bytes(payload)
    _validate_frame_payload(frame_type, payload)
    body = bytes((frame_type,)) + struct.pack("!H", len(payload)) + payload
    frame = UART_SOF + body + struct.pack("!H", crc16_ccitt_false(body))
    if len(frame) > UART_MAX_FRAME_SIZE:
        raise ProtocolError("UART frame exceeds 216 bytes")
    return frame

def parse_uart_frame(data: bytes) -> UartFrame:
    raw = bytes(data)
    if len(raw) < 7 or len(raw) > UART_MAX_FRAME_SIZE:
        raise ProtocolError("UART frame length outside valid limits")
    if raw[:2] != UART_SOF:
        raise ProtocolError("invalid UART SOF")
    payload_len = int.from_bytes(raw[3:5], "big")
    if len(raw) != 2 + 1 + 2 + payload_len + 2:
        raise ProtocolError("UART payload length does not match frame length")
    if int.from_bytes(raw[-2:], "big") != crc16_ccitt_false(raw[2:-2]):
        raise ProtocolError("UART CRC16 mismatch")
    payload = raw[5:-2]
    _validate_frame_payload(raw[2], payload)
    return UartFrame(raw[2], payload)

def encode_link_status(status: LinkStatus) -> bytes:
    if status.wifi_state not in (0, 1) or status.mqtt_state not in (0, 1):
        raise ProtocolError("wifi_state and mqtt_state must be 0 or 1")
    if not 0 <= status.esp32_uptime_ms <= 0xFFFFFFFF:
        raise ProtocolError("esp32_uptime_ms must fit uint32")
    return bytes((status.wifi_state, status.mqtt_state, 0)) + struct.pack("!I", status.esp32_uptime_ms)

def decode_link_status(payload: bytes) -> LinkStatus:
    _validate_frame_payload(UART_FRAME_LINK_STATUS, payload)
    return LinkStatus(payload[0], payload[1], int.from_bytes(payload[3:7], "big"))
