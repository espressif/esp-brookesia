#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

"""Wrap an unchanged GSPC bundle in a small, checked zlib envelope."""

import argparse
import struct
import zlib
from pathlib import Path

MAGIC = b"BGZ1"
HEADER = struct.Struct("<4sIII")
MAX_EXPANDED_BUNDLE_BYTES = 8 * 1024 * 1024


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    source = args.input.read_bytes()
    if not source.startswith(b"GSPB") or len(source) > MAX_EXPANDED_BUNDLE_BYTES:
        raise ValueError("Expected a GSPC bundle of at most 8 MiB")
    compressed = zlib.compress(source, level=6)
    if len(compressed) > 0xFFFFFFFF:
        raise ValueError("Compressed GSPC bundle exceeds 4 GiB")
    envelope = HEADER.pack(MAGIC, len(source), len(compressed), zlib.crc32(source)) + compressed
    if zlib.decompress(compressed) != source:
        raise ValueError("Compressed GSPC bundle failed the generation round trip")
    args.output.write_bytes(envelope)


if __name__ == "__main__":
    main()
