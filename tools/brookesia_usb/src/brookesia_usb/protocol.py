"""Wire protocol helpers for the ESP-Brookesia USB service."""

from __future__ import annotations

import hashlib
import json
import struct
from dataclasses import dataclass
from enum import IntEnum
from typing import Any, BinaryIO, Iterable


PROTOCOL_VERSION = 1
FRAME_MAGIC = b"BU"
FRAME_FIELDS = struct.Struct("<2sBBIIII")
FRAME_FIELDS_SIZE = FRAME_FIELDS.size
FRAME_HEADER_SIZE = FRAME_FIELDS_SIZE + 4
MAX_FRAME_PAYLOAD = 16 * 1024


class FrameType(IntEnum):
    DATA = 1
    END = 2
    CANCEL = 3


@dataclass(frozen=True)
class BinaryFrame:
    frame_type: FrameType
    request_id: int
    sequence: int
    payload: bytes = b""
    version: int = PROTOCOL_VERSION


def crc32(data: bytes) -> int:
    """Return the same CRC32 used by the firmware frame parser."""

    value = 0xFFFFFFFF
    for byte in data:
        value ^= byte
        for _ in range(8):
            value = (value >> 1) ^ (0xEDB88320 if value & 1 else 0)
    return (~value) & 0xFFFFFFFF


def encode_frame(frame: BinaryFrame) -> bytes:
    if len(frame.payload) > MAX_FRAME_PAYLOAD:
        raise ValueError("frame payload is too large")
    if frame.frame_type != FrameType.DATA and frame.payload:
        raise ValueError("control frames cannot contain a payload")
    header = FRAME_FIELDS.pack(
        FRAME_MAGIC,
        frame.version,
        int(frame.frame_type),
        frame.request_id,
        frame.sequence,
        len(frame.payload),
        crc32(frame.payload),
    )
    return header + struct.pack("<I", crc32(header)) + frame.payload


def decode_frame(data: bytes, max_payload: int = MAX_FRAME_PAYLOAD) -> BinaryFrame:
    if len(data) < FRAME_HEADER_SIZE:
        raise ValueError("frame is incomplete")
    magic, version, frame_type, request_id, sequence, payload_length, payload_crc = FRAME_FIELDS.unpack(
        data[:FRAME_FIELDS_SIZE]
    )
    if magic != FRAME_MAGIC:
        raise ValueError("bad frame magic")
    if version != PROTOCOL_VERSION:
        raise ValueError("unsupported frame version")
    if payload_length > max_payload:
        raise ValueError("frame payload is too large")
    header_without_crc = data[:20]
    header_crc = struct.unpack("<I", data[20:24])[0]
    if crc32(header_without_crc) != header_crc:
        raise ValueError("bad frame header crc")
    end = FRAME_HEADER_SIZE + payload_length
    if len(data) != end:
        raise ValueError("frame length does not match payload length")
    payload = data[FRAME_HEADER_SIZE:end]
    if crc32(payload) != payload_crc:
        raise ValueError("bad frame payload crc")
    try:
        parsed_type = FrameType(frame_type)
    except ValueError as error:
        raise ValueError("unsupported frame type") from error
    if parsed_type != FrameType.DATA and payload:
        raise ValueError("control frames cannot contain a payload")
    return BinaryFrame(parsed_type, request_id, sequence, payload, version)


def command(op: str, request_id: int, **fields: Any) -> bytes:
    value = {"version": PROTOCOL_VERSION, "op": op, "request_id": request_id, **fields}
    return (json.dumps(value, separators=(",", ":"), ensure_ascii=False) + "\n").encode("utf-8")


def sha256_file(file_obj: BinaryIO, chunk_size: int = 64 * 1024) -> tuple[int, str]:
    digest = hashlib.sha256()
    size = 0
    while True:
        chunk = file_obj.read(chunk_size)
        if not chunk:
            break
        digest.update(chunk)
        size += len(chunk)
    return size, digest.hexdigest()


def iter_file_frames(
    file_obj: BinaryIO,
    request_id: int,
    frame_payload_size: int = MAX_FRAME_PAYLOAD,
) -> Iterable[bytes]:
    sequence = 0
    while True:
        payload = file_obj.read(frame_payload_size)
        if not payload:
            break
        yield encode_frame(BinaryFrame(FrameType.DATA, request_id, sequence, payload))
        sequence += 1
    yield encode_frame(BinaryFrame(FrameType.END, request_id, sequence))
