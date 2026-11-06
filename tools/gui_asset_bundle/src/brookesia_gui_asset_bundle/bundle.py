# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

"""Packer and reader for the Brookesia GUI asset bundle (format v1).

The runtime reads one JSON asset per file today. On littlefs every open costs a
fixed ~5.25 ms regardless of size, so a 30-asset document spends roughly 157 ms
of its 230 ms I/O on open overhead alone. The bundle turns those opens into one:
a header, a sorted index, and the concatenated payload, read by seek.

Layout (little endian, 4-byte aligned payload):

    offset 0   char[4]  magic "BGB1"
    offset 4   u32      version
    offset 8   u32      flags
    offset 12  u32      entry count
    offset 16  u32      index size in bytes
    offset 20  u32      payload offset from the start of the file
    offset 24  entries: u32 path_len, path bytes, u32 offset, u32 size, u32 flags
    ...        payload: raw file bytes, each entry offset 4-byte aligned
"""

import fnmatch
import os
import struct
from dataclasses import dataclass
from pathlib import Path
from typing import List, Sequence, Tuple

MAGIC = b"BGB1"
VERSION = 1
ALIGNMENT = 4
DEFAULT_PATTERNS = ("*.json",)

HEADER_FORMAT = "<4sIIIII"
HEADER_SIZE = struct.calcsize(HEADER_FORMAT)
ENTRY_FORMAT = "<III"
ENTRY_FIXED_SIZE = 4 + struct.calcsize(ENTRY_FORMAT)

_U32_MAX = 0xFFFFFFFF


class BundleError(Exception):
    """Raised when a bundle cannot be built or parsed."""


@dataclass(frozen=True)
class BundleEntry:
    path: str
    offset: int
    size: int
    flags: int


@dataclass(frozen=True)
class BundleStats:
    entries: int
    index_bytes: int
    payload_bytes: int
    total_bytes: int


@dataclass(frozen=True)
class Bundle:
    version: int
    flags: int
    entries: Tuple[BundleEntry, ...]
    payload_offset: int
    data: bytes

    def paths(self) -> List[str]:
        return [entry.path for entry in self.entries]

    def find(self, path: str) -> BundleEntry:
        for entry in self.entries:
            if entry.path == path:
                return entry
        raise KeyError(path)

    def read(self, path: str) -> bytes:
        entry = self.find(path)
        start = self.payload_offset + entry.offset
        return self.data[start:start + entry.size]


def collect_assets(
    res_root: os.PathLike,
    patterns: Sequence[str] = DEFAULT_PATTERNS,
    exclude: os.PathLike = None,
) -> List[Tuple[str, Path]]:
    """Return (relative posix path, source path) pairs, sorted by relative path."""
    root = Path(res_root)
    if not root.is_dir():
        raise BundleError(f"resource root is not a directory: {root}")

    exclude_path = Path(exclude).resolve() if exclude is not None else None
    found: List[Tuple[str, Path]] = []
    for path in sorted(root.rglob("*")):
        if path.is_symlink() or not path.is_file():
            continue
        if exclude_path is not None and path.resolve() == exclude_path:
            continue
        rel = path.relative_to(root).as_posix()
        if any(fnmatch.fnmatchcase(rel, pattern) for pattern in patterns):
            found.append((rel, path))
    return found


def pack_assets(
    res_root: os.PathLike,
    out_path: os.PathLike,
    patterns: Sequence[str] = DEFAULT_PATTERNS,
) -> BundleStats:
    """Pack the matching files under res_root into out_path, deterministically."""
    out = Path(out_path)
    entries = collect_assets(res_root, patterns, exclude=out)

    index = bytearray()
    payload = bytearray()
    for rel, source in entries:
        data = source.read_bytes()
        if len(data) > _U32_MAX:
            raise BundleError(f"asset is too large for the bundle format: {rel}")

        offset = len(payload)
        payload.extend(data)
        padding = (-len(payload)) % ALIGNMENT
        if padding:
            payload.extend(b"\x00" * padding)

        path_bytes = rel.encode("utf-8")
        if len(path_bytes) > _U32_MAX:
            raise BundleError(f"asset path is too long for the bundle format: {rel}")
        index.extend(struct.pack("<I", len(path_bytes)))
        index.extend(path_bytes)
        index.extend(struct.pack(ENTRY_FORMAT, offset, len(data), 0))

    index_size = len(index)
    payload_offset = HEADER_SIZE + index_size
    payload_offset += (-payload_offset) % ALIGNMENT
    header = struct.pack(HEADER_FORMAT, MAGIC, VERSION, 0, len(entries), index_size, payload_offset)
    gap = b"\x00" * (payload_offset - HEADER_SIZE - index_size)

    out.parent.mkdir(parents=True, exist_ok=True)
    temporary = out.with_name(out.name + ".tmp")
    temporary.write_bytes(header + bytes(index) + gap + bytes(payload))
    os.replace(temporary, out)

    return BundleStats(
        entries=len(entries),
        index_bytes=index_size,
        payload_bytes=len(payload),
        total_bytes=payload_offset + len(payload),
    )


def read_bundle(bundle_path: os.PathLike) -> Bundle:
    """Parse a bundle file and validate its header, index and payload bounds."""
    data = Path(bundle_path).read_bytes()
    if len(data) < HEADER_SIZE:
        raise BundleError("bundle is shorter than its header")

    magic, version, flags, count, index_size, payload_offset = struct.unpack_from(HEADER_FORMAT, data, 0)
    if magic != MAGIC:
        raise BundleError(f"bad bundle magic: {magic!r}")
    if version != VERSION:
        raise BundleError(f"unsupported bundle version: {version}")
    if payload_offset > len(data):
        raise BundleError("bundle payload offset is past the end of the file")
    if HEADER_SIZE + index_size > payload_offset:
        raise BundleError("bundle index overlaps the payload")

    entries: List[BundleEntry] = []
    cursor = HEADER_SIZE
    for _ in range(count):
        if cursor + 4 > HEADER_SIZE + index_size:
            raise BundleError("bundle index is truncated")
        (path_len,) = struct.unpack_from("<I", data, cursor)
        cursor += 4
        if cursor + path_len + struct.calcsize(ENTRY_FORMAT) > HEADER_SIZE + index_size:
            raise BundleError("bundle entry is truncated")
        path = data[cursor:cursor + path_len].decode("utf-8")
        cursor += path_len
        offset, size, entry_flags = struct.unpack_from(ENTRY_FORMAT, data, cursor)
        cursor += struct.calcsize(ENTRY_FORMAT)
        if payload_offset + offset + size > len(data):
            raise BundleError(f"bundle entry is past the end of the file: {path}")
        entries.append(BundleEntry(path=path, offset=offset, size=size, flags=entry_flags))

    if cursor - HEADER_SIZE != index_size:
        raise BundleError("bundle index size does not match its entries")
    return Bundle(version=version, flags=flags, entries=tuple(entries), payload_offset=payload_offset, data=data)


def verify_bundle(
    bundle_path: os.PathLike,
    res_root: os.PathLike,
    patterns: Sequence[str] = DEFAULT_PATTERNS,
) -> List[str]:
    """Return a list of problems; an empty list means the bundle matches the tree."""
    bundle = read_bundle(bundle_path)
    problems: List[str] = []
    expected = dict(collect_assets(res_root, patterns, exclude=bundle_path))

    for rel in bundle.paths():
        source = expected.pop(rel, None)
        if source is None:
            problems.append(f"bundle entry has no source file: {rel}")
            continue
        try:
            packed = bundle.read(rel)
        except KeyError:
            problems.append(f"bundle entry cannot be read: {rel}")
            continue
        if packed != source.read_bytes():
            problems.append(f"bundle entry differs from its source: {rel}")

    for rel in sorted(expected):
        problems.append(f"source file is missing from the bundle: {rel}")
    return problems
