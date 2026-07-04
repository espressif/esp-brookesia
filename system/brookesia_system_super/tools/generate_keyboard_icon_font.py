#!/usr/bin/env python3
#
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0
"""Generate fixed-size keyboard image-font glyph PNGs."""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image, ImageFilter


KEYS = ("backspace", "cancel", "left", "right", "space", "ok")
SIZES = (20, 24, 28)


def _crop_alpha(image: Image.Image) -> Image.Image:
    rgba = image.convert("RGBA")
    bbox = rgba.getchannel("A").getbbox()
    if bbox is None:
        return rgba
    return rgba.crop(bbox)


def _resize_glyph(name: str, source: Image.Image, size: int) -> Image.Image:
    glyph = _crop_alpha(source)
    if glyph.width <= 0 or glyph.height <= 0:
        return Image.new("RGBA", (size, size), (0, 0, 0, 0))

    if name == "space":
        max_width = max(1, round(size * 0.70))
        max_height = max(2, round(size * 0.24))
    else:
        max_width = max(1, round(size * 0.72))
        max_height = max(1, round(size * 0.72))

    scale = min(max_width / glyph.width, max_height / glyph.height)
    target_width = max(1, round(glyph.width * scale))
    target_height = max(1, round(glyph.height * scale))
    resized = glyph.resize((target_width, target_height), Image.Resampling.LANCZOS)

    if name == "space":
        alpha = resized.getchannel("A").filter(ImageFilter.MaxFilter(3))
        resized.putalpha(alpha)

    canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    canvas.alpha_composite(resized, ((size - target_width) // 2, (size - target_height) // 2))
    return canvas


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    for key in KEYS:
        source_path = args.source / f"{key}.png"
        if not source_path.exists():
            raise FileNotFoundError(source_path)
        source = Image.open(source_path)
        for size in SIZES:
            output_dir = args.output / str(size)
            output_dir.mkdir(parents=True, exist_ok=True)
            _resize_glyph(key, source, size).save(output_dir / f"{key}.png")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
