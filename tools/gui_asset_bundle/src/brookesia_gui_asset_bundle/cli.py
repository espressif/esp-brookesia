# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

"""Command line entry points for the GUI asset bundle tool."""

import argparse
import sys
from typing import List, Optional

from .bundle import DEFAULT_PATTERNS, BundleError, pack_assets, read_bundle, verify_bundle


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="brookesia-gui-asset-bundle",
        description="Pack a GUI resource tree into one bundle and verify it.",
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    pack = subparsers.add_parser("pack", help="pack a resource tree into a bundle")
    pack.add_argument("--res-root", required=True, help="resource root directory")
    pack.add_argument("--out", required=True, help="output bundle path")
    pack.add_argument(
        "--include",
        action="append",
        dest="patterns",
        metavar="GLOB",
        help=f"relative path glob to include, repeatable (default: {DEFAULT_PATTERNS[0]})",
    )

    verify = subparsers.add_parser("verify", help="check a bundle against its source tree")
    verify.add_argument("--bundle", required=True, help="bundle file to verify")
    verify.add_argument("--res-root", required=True, help="resource root directory")
    verify.add_argument("--include", action="append", dest="patterns", metavar="GLOB")

    listing = subparsers.add_parser("list", help="list the entries stored in a bundle")
    listing.add_argument("--bundle", required=True, help="bundle file to inspect")

    return parser


def _patterns(args: argparse.Namespace) -> tuple:
    return tuple(args.patterns) if args.patterns else DEFAULT_PATTERNS


def main(argv: Optional[List[str]] = None) -> int:
    args = _build_parser().parse_args(argv)
    try:
        if args.command == "pack":
            stats = pack_assets(args.res_root, args.out, _patterns(args))
            print(
                f"packed {stats.entries} entries: index {stats.index_bytes} B, "
                f"payload {stats.payload_bytes} B, total {stats.total_bytes} B -> {args.out}"
            )
            return 0

        if args.command == "verify":
            problems = verify_bundle(args.bundle, args.res_root, _patterns(args))
            if problems:
                for problem in problems:
                    print(f"error: {problem}", file=sys.stderr)
                return 1
            print(f"bundle matches {args.res_root}: {args.bundle}")
            return 0

        bundle = read_bundle(args.bundle)
        for entry in bundle.entries:
            print(f"{entry.size:8d}  {entry.path}")
        print(f"{len(bundle.entries)} entries, {len(bundle.data)} bytes total")
        return 0
    except (BundleError, OSError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
