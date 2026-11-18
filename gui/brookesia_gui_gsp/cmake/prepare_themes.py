#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Resolve scene palettes before GSP compilation, retaining one authored layout."""
import argparse
import json
from pathlib import Path

PALETTES = {
    "light": {
        "page_bg": "#F4F5F7", "background": "#F4F5F7", "surface": "#FFFFFF",
        "text": "#1C1C1E", "muted": "#636366", "control": "#E8E8EA",
        "accent": "#0A84FF", "on_accent": "#FFFFFF", "rail": "#E5E5EA", "key": "#FFFFFF",
    },
    "dark": {
        "page_bg": "#101114", "background": "#101114", "surface": "#1C1C1E",
        "text": "#F5F5F7", "muted": "#A0A0A8", "control": "#33343A",
        "accent": "#0A84FF", "on_accent": "#FFFFFF", "rail": "#48484A", "key": "#2C2C2E",
    },
}


def prepare(source, destination, palette):
    scene = json.loads(source.read_text(encoding="utf-8"))

    def resolve(value, key=""):
        if isinstance(value, dict):
            if "theme" in value:
                return palette[value["theme"]]
            return {k: resolve(v, k) for k, v in value.items() if k != "themes"}
        if isinstance(value, list):
            return [resolve(item) for item in value]
        if key in {"font", "font_charset_file"} and isinstance(value, str):
            if key == "font" and value.startswith("brookesia_gui_gsp/"):
                return str((Path(__file__).resolve().parent.parent / "gsp/fonts" / value.split("/", 1)[1]).resolve())
            return str((source.parent / value).resolve())
        return value

    result = resolve(scene)
    result["screen_bg"] = palette["page_bg"]
    destination.parent.mkdir(parents=True, exist_ok=True)
    content = json.dumps(result, ensure_ascii=False, indent=2) + "\n"
    if not destination.exists() or destination.read_text(encoding="utf-8") != content:
        destination.write_text(content, encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--theme", choices=PALETTES, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("scenes", nargs="+", type=Path)
    args = parser.parse_args()
    for source in args.scenes:
        prepare(source.resolve(), args.output / source.name, PALETTES[args.theme])


if __name__ == "__main__":
    main()
