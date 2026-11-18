#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

"""Generate the example's short, quiet sound check without external media assets."""

import math
import struct
import sys
import wave
from pathlib import Path


def main():
    target = Path(sys.argv[1])
    target.parent.mkdir(parents=True, exist_ok=True)
    sample_rate = 16000
    count = int(sample_rate * 0.6)
    frames = bytearray()
    for i in range(count):
        envelope = min(1.0, i / 320, (count - i - 1) / 320)
        sample = int(6000 * envelope * math.sin(2 * math.pi * 660 * i / sample_rate))
        frames.extend(struct.pack("<h", sample))
    with wave.open(str(target), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(sample_rate)
        output.writeframes(frames)


if __name__ == "__main__":
    main()
