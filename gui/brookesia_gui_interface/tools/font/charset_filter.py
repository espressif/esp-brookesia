#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
brookesia_charset_filter.py

Filter a large charset file, such as charset_zh_en_gbk.txt, into a smaller
Chinese/English charset suitable for Brookesia / ESP32 / LVGL font subsetting.

Typical usage:

  # Keep GB2312 level-1 common Chinese chars + English/symbols.
  # Output files are written to <script_dir>/build/ by default.
  python3 brookesia_charset_filter.py \
    --input charset_zh_en_gbk.txt \
    --output charset_zh_en_gb2312_l1.txt \
    --removed removed_chars.txt \
    --gb-level l1

  # Keep GB2312 level-1 + level-2 chars, and force keep UI strings.
  python3 brookesia_charset_filter.py \
    -i charset_zh_en_gbk.txt \
    -o charset_zh_en_gb2312_l1_l2.txt \
    --gb-level l2 \
    --ui-strings brookesia_strings.txt \
    --stats stats.json

  # Generate a subset TTF in one step, if fonttools is installed.
  python3 brookesia_charset_filter.py \
    -i charset_zh_en_gbk.txt \
    -o charset_zh_en_gb2312_l1.txt \
    --gb-level l1 \
    --font-path /usr/share/fonts/truetype/wqy/wqy-microhei.ttc \
    --font-number 0 \
    --subset-output wqy-microhei-zh-en-gb2312-l1.ttf

Output directory:
  Relative paths passed to --output / --removed / --stats / --subset-output are
  placed under <script_dir>/build/ by default. Absolute paths are kept as-is.
  Override the build directory with --build-dir <path>.

GB level meanings:
  none / 0       Keep no GB Chinese chars; only symbols and forced strings.
  l1 / 1         Keep GB2312 level-1 chars. Rows 16-55, about 3755 common chars.
  l2 / 2         Keep GB2312 level-1 + level-2 chars. Rows 16-87, about 6763 chars.
  gbk            Keep CJK chars decoded from GBK double-byte space.

Notes:
  - This script does not download or bundle any font files.
  - The filtering step itself uses only Python standard library.
  - The optional TTF/OTF subsetting step requires fonttools:
      pip install fonttools
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Iterable


SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_BUILD_DIR = SCRIPT_DIR / "build"


DEFAULT_EXTRA_SYMBOLS = (
    "℃°㎡μΩ±×÷←↑→↓↔↕…—–·￥€$£"
    "✓✔✕✖●○◉◎◆◇■□▲▼◀▶★☆"
    "•⌁⌂⌘⌫⏎⏻⏱⏲☀☁☂☃☕⚙⚠"
)

BASIC_SYMBOL_RANGES = (
    (0x0020, 0x007E),  # ASCII: English, digits, basic punctuation
    (0x00A0, 0x00FF),  # Latin-1 supplement
    (0x2000, 0x206F),  # General punctuation
    (0x3000, 0x303F),  # CJK symbols and punctuation
    (0xFF00, 0xFFEF),  # Halfwidth and fullwidth forms
)


def die(message: str, exit_code: int = 1) -> None:
    print(f"error: {message}", file=sys.stderr)
    raise SystemExit(exit_code)


def read_chars(path: Path, *, ignore_whitespace: bool = True) -> set[str]:
    if not path.exists():
        die(f"file not found: {path}")

    text = path.read_text(encoding="utf-8")
    if ignore_whitespace:
        return {ch for ch in text if ch not in "\r\n\t"}
    return set(text)


def read_chars_from_many(paths: Iterable[Path], *, ignore_whitespace: bool = True) -> set[str]:
    chars: set[str] = set()
    for path in paths:
        chars |= read_chars(path, ignore_whitespace=ignore_whitespace)
    return chars


def write_chars(path: Path, chars: Iterable[str], *, append_newline: bool = False) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    text = "".join(sorted(chars, key=ord))
    if append_newline:
        text += "\n"
    path.write_text(text, encoding="utf-8")


def resolve_output_path(path: Path | None, build_dir: Path) -> Path | None:
    """Place relative output paths under build_dir. Absolute paths pass through."""
    if path is None:
        return None
    if path.is_absolute():
        return path
    return build_dir / path


def is_cjk_han(ch: str) -> bool:
    cp = ord(ch)
    return (
        0x3400 <= cp <= 0x4DBF or    # CJK Extension A
        0x4E00 <= cp <= 0x9FFF or    # CJK Unified Ideographs
        0xF900 <= cp <= 0xFAFF or    # CJK Compatibility Ideographs
        0x20000 <= cp <= 0x2A6DF or  # CJK Extension B
        0x2A700 <= cp <= 0x2B73F or  # CJK Extension C
        0x2B740 <= cp <= 0x2B81F or  # CJK Extension D
        0x2B820 <= cp <= 0x2CEAF or  # CJK Extension E/F
        0x2CEB0 <= cp <= 0x2EBEF     # CJK Extension F/G/H range area
    )


def is_basic_symbol(ch: str, extra_symbols: str = DEFAULT_EXTRA_SYMBOLS) -> bool:
    cp = ord(ch)
    for start, end in BASIC_SYMBOL_RANGES:
        if start <= cp <= end:
            return True
    return ch in extra_symbols


def normalize_gb_level(value: str) -> str:
    normalized = value.strip().lower().replace("_", "-")
    aliases = {
        "0": "none",
        "none": "none",
        "no": "none",
        "off": "none",
        "1": "l1",
        "l1": "l1",
        "level1": "l1",
        "level-1": "l1",
        "gb2312-l1": "l1",
        "gb2312-level1": "l1",
        "gb2312-level-1": "l1",
        "2": "l2",
        "l2": "l2",
        "level2": "l2",
        "level-2": "l2",
        "gb2312-l2": "l2",
        "gb2312-l1-l2": "l2",
        "gb2312-level2": "l2",
        "gb2312-level-2": "l2",
        "gb2312-all": "l2",
        "gbk": "gbk",
        "gbk-cjk": "gbk",
        "gbk-han": "gbk",
    }
    if normalized not in aliases:
        die(
            "unsupported --gb-level. Use one of: "
            "none, l1, l2, gbk. Aliases 0/1/2 are also accepted."
        )
    return aliases[normalized]


def build_gb2312_chars(level: str) -> set[str]:
    """
    Build GB2312 level-1 or level-1+level-2 Han characters.

    GB2312 Han rows:
      Level 1: rows 16-55
      Level 2: rows 56-87

    In GB2312 byte form, row/cell are offset by 0xA0.
    """
    if level == "l1":
        row_start, row_end_inclusive = 16, 55
    elif level == "l2":
        row_start, row_end_inclusive = 16, 87
    else:
        die(f"internal error: unsupported GB2312 level: {level}")

    chars: set[str] = set()
    for row in range(row_start, row_end_inclusive + 1):
        for cell in range(1, 95):
            high = row + 0xA0
            low = cell + 0xA0
            try:
                ch = bytes([high, low]).decode("gb2312")
            except UnicodeDecodeError:
                continue
            if len(ch) == 1 and is_cjk_han(ch):
                chars.add(ch)
    return chars


def build_gbk_cjk_chars() -> set[str]:
    """
    Build a broad set of Han characters decoded from GBK double-byte space.
    This is larger than GB2312 and may include less common characters.
    """
    chars: set[str] = set()
    for high in range(0x81, 0xFF):
        for low in list(range(0x40, 0x7F)) + list(range(0x80, 0xFF)):
            try:
                ch = bytes([high, low]).decode("gbk")
            except UnicodeDecodeError:
                continue
            if len(ch) == 1 and is_cjk_han(ch):
                chars.add(ch)
    return chars


def build_gb_chars(gb_level: str) -> set[str]:
    gb_level = normalize_gb_level(gb_level)
    if gb_level == "none":
        return set()
    if gb_level in {"l1", "l2"}:
        return build_gb2312_chars(gb_level)
    if gb_level == "gbk":
        return build_gbk_cjk_chars()
    die(f"internal error: unknown normalized GB level: {gb_level}")


def run_fonttools_subset(
    *,
    font_path: Path,
    charset_path: Path,
    output_path: Path,
    font_number: int,
    fonttools_bin: str,
    no_hinting: bool,
    flavor: str | None,
    extra_args: list[str],
) -> None:
    if not font_path.exists():
        die(f"font file not found: {font_path}")

    if shutil.which(fonttools_bin) is None:
        die(
            f"fonttools executable not found: {fonttools_bin}. "
            "Install it with: pip install fonttools"
        )

    cmd = [
        fonttools_bin,
        "subset",
        str(font_path),
        f"--font-number={font_number}",
        f"--text-file={charset_path}",
        f"--output-file={output_path}",
        "--layout-features=*",
        "--glyph-names",
        "--symbol-cmap",
        "--legacy-cmap",
        "--notdef-glyph",
        "--notdef-outline",
        "--recommended-glyphs",
        "--name-IDs=*",
        "--name-legacy",
        "--name-languages=*",
        "--ignore-missing-glyphs",
    ]

    if no_hinting:
        cmd.append("--no-hinting")

    if flavor:
        # Example: --flavor=woff2. Usually not needed for ESP32/LVGL.
        cmd.append(f"--flavor={flavor}")

    cmd.extend(extra_args)

    print("running:", " ".join(cmd))
    subprocess.run(cmd, check=True)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Filter charset_zh_en_gbk.txt into a smaller Chinese/English charset "
            "for Brookesia / ESP32 / LVGL font subsetting."
        ),
        formatter_class=argparse.RawTextHelpFormatter,
    )

    parser.add_argument(
        "-i", "--input",
        required=True,
        type=Path,
        help="Input charset file, for example charset_zh_en_gbk.txt.",
    )
    parser.add_argument(
        "-o", "--output",
        required=True,
        type=Path,
        help="Output filtered charset file.",
    )
    parser.add_argument(
        "--removed",
        type=Path,
        default=None,
        help="Optional output file for characters removed from the input charset.",
    )
    parser.add_argument(
        "--stats",
        type=Path,
        default=None,
        help="Optional JSON stats output file.",
    )
    parser.add_argument(
        "--build-dir",
        type=Path,
        default=DEFAULT_BUILD_DIR,
        help=(
            "Directory where relative output files (--output, --removed, --stats,\n"
            "--subset-output) are written. Absolute paths bypass this directory.\n"
            "Default: <script_dir>/build."
        ),
    )

    parser.add_argument(
        "--gb-level",
        default="l1",
        help=(
            "Chinese charset level to keep. Default: l1\n"
            "  none / 0 : keep no GB Han chars\n"
            "  l1 / 1   : GB2312 level-1 common chars, rows 16-55\n"
            "  l2 / 2   : GB2312 level-1 + level-2 chars, rows 16-87\n"
            "  gbk      : CJK Han chars decoded from GBK double-byte space"
        ),
    )
    parser.add_argument(
        "--no-basic-symbols",
        action="store_true",
        help="Do not automatically keep ASCII, Latin-1, CJK punctuation and fullwidth symbols.",
    )
    parser.add_argument(
        "--extra-symbols",
        default=DEFAULT_EXTRA_SYMBOLS,
        help=(
            "Extra symbols to keep when basic symbols are enabled.\n"
            "Default includes degree, arrows, check marks, geometric symbols, etc."
        ),
    )
    parser.add_argument(
        "--extra-symbols-file",
        action="append",
        type=Path,
        default=[],
        help="Additional file containing symbols/chars to keep. Can be specified multiple times.",
    )
    parser.add_argument(
        "--ui-strings",
        "--force-keep-file",
        dest="force_keep_files",
        action="append",
        type=Path,
        default=[],
        help=(
            "File containing UI strings/chars that must be kept.\n"
            "Can be specified multiple times. Useful for Brookesia fixed UI text."
        ),
    )
    parser.add_argument(
        "--exclude-file",
        action="append",
        type=Path,
        default=[],
        help="File containing chars to remove even if selected. Can be specified multiple times.",
    )
    parser.add_argument(
        "--strict-source",
        action="store_true",
        help=(
            "Only keep characters that exist in the input charset.\n"
            "By default, forced UI strings and extra-symbol files may add chars outside the input."
        ),
    )
    parser.add_argument(
        "--keep-whitespace",
        action="store_true",
        help="Keep whitespace characters from input and string files. Default ignores CR/LF/TAB.",
    )
    parser.add_argument(
        "--append-newline",
        action="store_true",
        help="Append a newline to output files.",
    )

    # Optional fonttools subset step.
    parser.add_argument(
        "--font-path",
        type=Path,
        default=None,
        help="Optional font file path to subset after generating the charset, e.g. wqy-microhei.ttc.",
    )
    parser.add_argument(
        "--font-number",
        type=int,
        default=0,
        help="Font index for TTC files. Default: 0.",
    )
    parser.add_argument(
        "--subset-output",
        type=Path,
        default=None,
        help="Optional subset font output path, e.g. wqy-microhei-zh-en-gb2312-l1.ttf.",
    )
    parser.add_argument(
        "--fonttools-bin",
        default="fonttools",
        help="fonttools executable name/path. Default: fonttools.",
    )
    parser.add_argument(
        "--no-hinting",
        action="store_true",
        help="Pass --no-hinting to fonttools subset to reduce output size.",
    )
    parser.add_argument(
        "--flavor",
        default=None,
        help="Optional fonttools flavor such as woff or woff2. Usually not needed for LVGL.",
    )
    parser.add_argument(
        "--fonttools-extra-arg",
        action="append",
        default=[],
        help="Extra argument passed to fonttools subset. Can be specified multiple times.",
    )

    return parser.parse_args()


def main() -> None:
    args = parse_args()

    gb_level = normalize_gb_level(args.gb_level)
    ignore_whitespace = not args.keep_whitespace

    build_dir = args.build_dir
    build_dir.mkdir(parents=True, exist_ok=True)

    output_path = resolve_output_path(args.output, build_dir)
    removed_path = resolve_output_path(args.removed, build_dir)
    stats_path = resolve_output_path(args.stats, build_dir)
    subset_output_path = resolve_output_path(args.subset_output, build_dir)

    source_chars = read_chars(args.input, ignore_whitespace=ignore_whitespace)

    keep_chars: set[str] = set()

    if not args.no_basic_symbols:
        keep_chars |= {
            ch for ch in source_chars
            if is_basic_symbol(ch, args.extra_symbols)
        }
        keep_chars |= {
            ch for ch in args.extra_symbols
            if not ignore_whitespace or ch not in "\r\n\t"
        }

    gb_chars = build_gb_chars(gb_level)
    keep_chars |= source_chars & gb_chars

    extra_file_chars = read_chars_from_many(
        args.extra_symbols_file,
        ignore_whitespace=ignore_whitespace,
    )
    force_keep_chars = read_chars_from_many(
        args.force_keep_files,
        ignore_whitespace=ignore_whitespace,
    )
    exclude_chars = read_chars_from_many(
        args.exclude_file,
        ignore_whitespace=ignore_whitespace,
    )

    keep_chars |= extra_file_chars
    keep_chars |= force_keep_chars

    if args.strict_source:
        keep_chars &= source_chars

    # Manual exclude is applied last and therefore wins.
    keep_chars -= exclude_chars

    removed_chars = source_chars - keep_chars
    added_outside_source = keep_chars - source_chars

    write_chars(output_path, keep_chars, append_newline=args.append_newline)
    if removed_path:
        write_chars(removed_path, removed_chars, append_newline=args.append_newline)

    stats = {
        "input": str(args.input),
        "output": str(output_path),
        "build_dir": str(build_dir),
        "gb_level": gb_level,
        "source_count": len(source_chars),
        "gb_candidate_count": len(gb_chars),
        "kept_count": len(keep_chars),
        "removed_count": len(removed_chars),
        "added_outside_source_count": len(added_outside_source),
        "forced_file_count": len(args.force_keep_files),
        "forced_char_count": len(force_keep_chars),
        "extra_symbols_file_count": len(args.extra_symbols_file),
        "extra_symbols_file_char_count": len(extra_file_chars),
        "exclude_file_count": len(args.exclude_file),
        "exclude_char_count": len(exclude_chars),
        "strict_source": bool(args.strict_source),
        "basic_symbols_enabled": not args.no_basic_symbols,
    }

    if stats_path:
        stats_path.parent.mkdir(parents=True, exist_ok=True)
        stats_path.write_text(
            json.dumps(stats, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )

    print("charset filtering done")
    print(f"  input chars          : {stats['source_count']}")
    print(f"  GB level             : {gb_level}")
    print(f"  kept chars           : {stats['kept_count']}")
    print(f"  removed chars        : {stats['removed_count']}")
    print(f"  added outside source : {stats['added_outside_source_count']}")
    print(f"  build dir            : {build_dir}")
    print(f"  output               : {output_path}")
    if removed_path:
        print(f"  removed output       : {removed_path}")
    if stats_path:
        print(f"  stats output         : {stats_path}")

    if args.font_path or subset_output_path:
        if not args.font_path or not subset_output_path:
            die("--font-path and --subset-output must be used together")
        run_fonttools_subset(
            font_path=args.font_path,
            charset_path=output_path,
            output_path=subset_output_path,
            font_number=args.font_number,
            fonttools_bin=args.fonttools_bin,
            no_hinting=args.no_hinting,
            flavor=args.flavor,
            extra_args=args.fonttools_extra_arg,
        )
        print(f"  subset font output   : {subset_output_path}")


if __name__ == "__main__":
    main()
