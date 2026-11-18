#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0
"""Build the Settings GSP Chinese fallback from DroidSansFallbackFull.ttf."""

import argparse

from fontTools import subset
from fontTools.ttLib import TTFont


def characters() -> set[int]:
    codepoints = set(range(0x3000, 0x3100))
    for codepoint in range(0x3400, 0x10000):
        character = chr(codepoint)
        try:
            if character.encode("gb2312").decode("gb2312") == character:
                codepoints.add(codepoint)
        except UnicodeError:
            pass

    # Big5 level 1 contains the frequently used traditional characters.
    for lead in range(0xA1, 0xC7):
        for trail in (*range(0x40, 0x7F), *range(0xA1, 0xFF)):
            try:
                character = bytes((lead, trail)).decode("big5")
                if len(character) == 1:
                    codepoints.add(ord(character))
            except UnicodeError:
                pass
    return codepoints


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", help="Path to DroidSansFallbackFull.ttf")
    parser.add_argument("output", help="Path to the generated subset TTF")
    args = parser.parse_args()

    font = TTFont(args.source, recalcTimestamp=False)
    options = subset.Options()
    options.name_IDs = ["*"]
    options.name_languages = ["*"]
    options.name_legacy = True
    subsetter = subset.Subsetter(options=options)
    subsetter.populate(unicodes=characters())
    subsetter.subset(font)
    font.save(args.output)


if __name__ == "__main__":
    main()
